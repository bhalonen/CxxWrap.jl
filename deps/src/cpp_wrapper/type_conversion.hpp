#ifndef TYPE_CONVERSION_HPP
#define TYPE_CONVERSION_HPP

#include <julia.h>

#include <map>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <typeindex>
#include <type_traits>

#include <iostream>

namespace cpp_wrapper
{

/// Get the symbol name correctly depending on Julia version
inline std::string symbol_name(jl_sym_t* symbol)
{
#if JULIA_VERSION_MAJOR == 0 && JULIA_VERSION_MINOR < 5
	return std::string(symbol->name);
#else
	return std::string(jl_symbol_name(symbol));
#endif
}

/// Helper to easily remove a ref to a const
template<typename T> using remove_const_ref = typename std::remove_const<typename std::remove_reference<T>::type>::type;

/// Base type for all wrapped classes
struct CppAny
{
};

/// Static mapping base template
template<typename SourceT> struct static_type_mapping
{
	typedef jl_value_t* type;
	template<typename T> using remove_const_ref = T;
	static jl_datatype_t* julia_type()
	{
		if(m_type_pointer == nullptr)
		{
			throw std::runtime_error("Type " + std::string(typeid(SourceT).name()) + " has no Julia wrapper");
		}
		return m_type_pointer;
	}

	static void set_julia_type(jl_datatype_t* dt)
	{
		if(m_type_pointer != nullptr)
		{
			throw std::runtime_error("Type " + std::string(typeid(SourceT).name()) + " was already registered");
		}
		m_type_pointer = dt;
	}

	static bool has_julia_type()
	{
		return m_type_pointer != nullptr;
	}

private:
	static jl_datatype_t* m_type_pointer;
};

template<typename SourceT> jl_datatype_t* static_type_mapping<SourceT>::m_type_pointer = nullptr;

/// Type mapping for templates translated to parametric types
template<template<typename...> class SourceT> struct parametric_type_mapping
{
	typedef jl_value_t* type;

	static jl_datatype_t* julia_type()
	{
		if(m_type_pointer == nullptr)
		{
			throw std::runtime_error("No Julia type for requested template type");
		}
		return m_type_pointer;
	}

	static void set_julia_type(jl_datatype_t* dt)
	{
		if(m_type_pointer != nullptr)
		{
			throw std::runtime_error("Template type was already registered as " + std::string(jl_typename_str((jl_value_t*)m_type_pointer)));
		}
		m_type_pointer = dt;
	}

	static bool has_julia_type()
	{
		return m_type_pointer != nullptr;
	}

private:
	static jl_datatype_t* m_type_pointer;
};

template<template<typename...> class SourceT> jl_datatype_t* parametric_type_mapping<SourceT>::m_type_pointer = nullptr;

/// Helper for Singleton types (Type{T} in Julia)
template<typename T>
struct SingletonType
{
};

template<typename T>
struct static_type_mapping<SingletonType<T>>
{
	typedef jl_datatype_t* type;
	static jl_datatype_t* julia_type() { return (jl_datatype_t*)jl_apply_type((jl_value_t*)jl_type_type, jl_svec1(static_type_mapping<T>::julia_type())); }
	template<typename T2> using remove_const_ref = cpp_wrapper::remove_const_ref<T2>;
};

/// Using declarations to avoid having to write typename all the time
template<typename SourceT> using mapped_type = typename static_type_mapping<SourceT>::type;
template<typename T> using mapped_reference_type = typename static_type_mapping<remove_const_ref<T>>::template remove_const_ref<T>;

/// Specializations
template<> struct static_type_mapping<void>
{
	typedef void type;
	static jl_datatype_t* julia_type() { return jl_void_type; }
	template<typename T> using remove_const_ref = cpp_wrapper::remove_const_ref<T>;
};

template<> struct static_type_mapping<double>
{
	typedef double type;
	static jl_datatype_t* julia_type() { return jl_float64_type; }
	template<typename T> using remove_const_ref = cpp_wrapper::remove_const_ref<T>;
};

template<> struct static_type_mapping<int>
{
	typedef int type;
	static jl_datatype_t* julia_type() { return jl_int32_type; }
	template<typename T> using remove_const_ref = cpp_wrapper::remove_const_ref<T>;
};

template<> struct static_type_mapping<unsigned int>
{
	typedef unsigned int type;
	static jl_datatype_t* julia_type() { return jl_uint32_type; }
	template<typename T> using remove_const_ref = cpp_wrapper::remove_const_ref<T>;
};

template<> struct static_type_mapping<uint64_t>
{
	typedef uint64_t type;
	static jl_datatype_t* julia_type() { return jl_uint64_type; }
	template<typename T> using remove_const_ref = cpp_wrapper::remove_const_ref<T>;
};

template<> struct static_type_mapping<std::string>
{
	typedef jl_value_t* type;
	static jl_datatype_t* julia_type() { return jl_any_type; }
	template<typename T> using remove_const_ref = cpp_wrapper::remove_const_ref<T>;
};

template<> struct static_type_mapping<void*>
{
	typedef jl_value_t* type;
	static jl_datatype_t* julia_type() { return jl_voidpointer_type; }
	template<typename T> using remove_const_ref = cpp_wrapper::remove_const_ref<T>;
};

template<> struct static_type_mapping<jl_datatype_t*>
{
	typedef jl_datatype_t* type; // Debatable if this should be jl_value_t*
	static jl_datatype_t* julia_type() { return jl_datatype_type; }
	template<typename T> using remove_const_ref = cpp_wrapper::remove_const_ref<T>;
};

template<> struct static_type_mapping<jl_value_t*>
{
	typedef jl_value_t* type; // Debatable if this should be jl_value_t*
	static jl_datatype_t* julia_type() { return jl_any_type; }
	template<typename T> using remove_const_ref = cpp_wrapper::remove_const_ref<T>;
};

// Helper for ObjectIdDict
struct ObjectIdDict {};

template<> struct static_type_mapping<ObjectIdDict>
{
	typedef jl_value_t* type;
	static jl_datatype_t* julia_type() { return (jl_datatype_t*)jl_get_global(jl_base_module, jl_symbol("ObjectIdDict")); }
	template<typename T> using remove_const_ref = cpp_wrapper::remove_const_ref<T>;
};

/// Auto-conversion to the statically mapped target type.
template<typename T>
inline mapped_type<T> convert_to_julia(const T& cpp_val)
{
	static_assert(std::is_fundamental<T>::value, "Unimplemented convert_to_julia");
	return cpp_val;
}

template<>
inline mapped_type<std::string> convert_to_julia(const std::string& str)
{
	return jl_cstr_to_string(str.c_str());
}

template<>
inline mapped_type<void*> convert_to_julia(void* const& p)
{
	return jl_box_voidpointer(p);
}

template<>
inline jl_value_t* convert_to_julia(jl_value_t* const& p)
{
	return p;
}

template<>
inline mapped_type<jl_datatype_t*> convert_to_julia(jl_datatype_t* const& dt)
{
	return dt;
}

template<typename CppT, typename JuliaT>
inline CppT convert_to_cpp(const JuliaT& julia_val)
{
	static_assert(std::is_fundamental<CppT>::value, "Unimplemented convert_to_cpp");
	return julia_val;
}

namespace detail {

// Unpack based on reference or pointer target type
template<typename IsReference, typename IsPointer>
struct DoUnpack;

// Unpack for a reference
template<>
struct DoUnpack<std::true_type, std::false_type>
{
	template<typename CppT>
	CppT& operator()(CppT* ptr)
	{
		if(ptr == nullptr)
			throw std::runtime_error("C++ object was deleted");

		return *ptr;
	}
};

// Unpack for a pointer
template<>
struct DoUnpack<std::false_type, std::true_type>
{
	template<typename CppT>
	CppT* operator()(CppT* ptr)
	{
		return ptr;
	}
};

// Unpack for a value
template<>
struct DoUnpack<std::false_type, std::false_type>
{
	template<typename CppT>
	CppT operator()(CppT* ptr)
	{
		if(ptr == nullptr)
			throw std::runtime_error("C++ object was deleted");

		return *ptr;
	}
};

inline std::string julia_type_name(jl_datatype_t* dt)
{
	return symbol_name(dt->name->name);
}

/// Helper class to unpack a julia type
template<typename CppT>
struct JuliaUnpacker
{
	// The C++ type stripped of all pointer, reference, const
	typedef typename std::remove_const<typename std::remove_pointer<remove_const_ref<CppT>>::type>::type stripped_cpp_t;

	CppT operator()(jl_value_t* julia_value)
	{
		return DoUnpack<typename std::is_reference<CppT>::type, typename std::is_pointer<CppT>::type>()(extract_cpp_pointer(julia_value));
	}

	/// Convert the void pointer in the julia structure to a C++ pointer, asserting that the type is correct
	static stripped_cpp_t* extract_cpp_pointer(jl_value_t* julia_value)
	{
		assert(julia_value != nullptr);
		assert(jl_type_morespecific(jl_typeof(julia_value), (jl_value_t*)static_type_mapping<stripped_cpp_t>::julia_type()));

		// Get the pointer to the C++ class
		jl_value_t* cpp_ref = jl_fieldref(julia_value,0);
		assert(jl_is_pointer(cpp_ref));
		return reinterpret_cast<stripped_cpp_t*>(jl_unbox_voidpointer(cpp_ref));
	}
};

} // namespace detail

template<typename CppT>
inline CppT convert_to_cpp(jl_value_t* const& julia_value)
{
	return detail::JuliaUnpacker<CppT>()(julia_value);
}

template<>
inline std::string convert_to_cpp(jl_value_t* const& julia_string)
{
	if(julia_string == nullptr || !jl_is_byte_string(julia_string))
	{
		throw std::runtime_error("Any type to convert to string is not a string");
	}
	std::string result(jl_bytestring_ptr(julia_string));
	return result;
}

template<>
inline jl_datatype_t* convert_to_cpp(jl_datatype_t* const& julia_value)
{
	return julia_value;
}

template<typename SingletonT>
inline SingletonT convert_to_cpp(jl_datatype_t* const& julia_value)
{
	return SingletonT();
}

template<>
inline jl_value_t* convert_to_cpp(jl_value_t* const& julia_value)
{
	return julia_value;
}

template<>
inline ObjectIdDict convert_to_cpp(jl_value_t* const&)
{
	return ObjectIdDict();
}

}

#endif