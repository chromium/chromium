// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_COMPILER_SPECIFIC_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_COMPILER_SPECIFIC_H_

#include "build/build_config.h"

// A wrapper around `__has_cpp_attribute`.
#if defined(__has_cpp_attribute)
#define PA_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#define PA_HAS_CPP_ATTRIBUTE(x) 0
#endif

// A wrapper around `__has_attribute`, similar to PA_HAS_CPP_ATTRIBUTE.
#if defined(__has_attribute)
#define PA_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define PA_HAS_ATTRIBUTE(x) 0
#endif

// A wrapper around `__has_builtin`, similar to PA_HAS_CPP_ATTRIBUTE.
#if defined(__has_builtin)
#define PA_HAS_BUILTIN(x) __has_builtin(x)
#else
#define PA_HAS_BUILTIN(x) 0
#endif

// Annotate a function indicating it should not be inlined.
// Use like:
//   NOINLINE void DoStuff() { ... }
#if defined(__clang__) && PA_HAS_ATTRIBUTE(noinline)
#define PA_NOINLINE [[clang::noinline]]
#elif defined(COMPILER_GCC) && PA_HAS_ATTRIBUTE(noinline)
#define PA_NOINLINE __attribute__((noinline))
#elif defined(COMPILER_MSVC)
#define PA_NOINLINE __declspec(noinline)
#else
#define PA_NOINLINE
#endif

#if defined(__clang__) && defined(NDEBUG) && PA_HAS_ATTRIBUTE(always_inline)
#define PA_ALWAYS_INLINE [[clang::always_inline]] inline
#elif defined(COMPILER_GCC) && defined(NDEBUG) && \
    PA_HAS_ATTRIBUTE(always_inline)
#define PA_ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined(COMPILER_MSVC) && defined(NDEBUG)
#define PA_ALWAYS_INLINE __forceinline
#else
#define PA_ALWAYS_INLINE inline
#endif

// Annotate a function indicating it should never be tail called. Useful to make
// sure callers of the annotated function are never omitted from call-stacks.
// To provide the complementary behavior (prevent the annotated function from
// being omitted) look at NOINLINE. Also note that this doesn't prevent code
// folding of multiple identical caller functions into a single signature. To
// prevent code folding, see NO_CODE_FOLDING() in base/debug/alias.h.
// Use like:
//   void NOT_TAIL_CALLED FooBar();
#if defined(__clang__) && PA_HAS_ATTRIBUTE(not_tail_called)
#define PA_NOT_TAIL_CALLED [[clang::not_tail_called]]
#else
#define PA_NOT_TAIL_CALLED
#endif

// Specify memory alignment for structs, classes, etc.
// Use like:
//   class PA_ALIGNAS(16) MyClass { ... }
//   PA_ALIGNAS(16) int array[4];
//
// In most places you can use the C++11 keyword "alignas", which is preferred.
//
// Historically, compilers had trouble mixing __attribute__((...)) syntax with
// alignas(...) syntax. However, at least Clang is very accepting nowadays. It
// may be that this macro can be removed entirely.
#if defined(__clang__)
#define PA_ALIGNAS(byte_alignment) alignas(byte_alignment)
#elif defined(COMPILER_MSVC)
#define PA_ALIGNAS(byte_alignment) __declspec(align(byte_alignment))
#elif defined(COMPILER_GCC) && PA_HAS_ATTRIBUTE(aligned)
#define PA_ALIGNAS(byte_alignment) __attribute__((aligned(byte_alignment)))
#endif

// In case the compiler supports it PA_NO_UNIQUE_ADDRESS evaluates to the C++20
// attribute [[no_unique_address]]. This allows annotating data members so that
// they need not have an address distinct from all other non-static data members
// of its class.
//
// References:
// * https://en.cppreference.com/w/cpp/language/attributes/no_unique_address
// * https://wg21.link/dcl.attr.nouniqueaddr
#if defined(COMPILER_MSVC) && PA_HAS_CPP_ATTRIBUTE(msvc::no_unique_address)
// Unfortunately MSVC ignores [[no_unique_address]] (see
// https://devblogs.microsoft.com/cppblog/msvc-cpp20-and-the-std-cpp20-switch/#msvc-extensions-and-abi),
// and clang-cl matches it for ABI compatibility reasons. We need to prefer
// [[msvc::no_unique_address]] when available if we actually want any effect.
#define PA_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif PA_HAS_CPP_ATTRIBUTE(no_unique_address)
#define PA_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define PA_NO_UNIQUE_ADDRESS
#endif

// Tells the compiler a function is using a printf-style format string.
// |format_param| is the one-based index of the format string parameter;
// |dots_param| is the one-based index of the "..." parameter.
// For v*printf functions (which take a va_list), pass 0 for dots_param.
// (This is undocumented but matches what the system C headers do.)
// For member functions, the implicit this parameter counts as index 1.
#if (defined(COMPILER_GCC) || defined(__clang__)) && PA_HAS_ATTRIBUTE(format)
#define PA_PRINTF_FORMAT(format_param, dots_param) \
  __attribute__((format(printf, format_param, dots_param)))
#else
#define PA_PRINTF_FORMAT(format_param, dots_param)
#endif

// Sanitizers annotations.
#if PA_HAS_ATTRIBUTE(no_sanitize)
#define PA_NO_SANITIZE(what) __attribute__((no_sanitize(what)))
#endif
#if !defined(PA_NO_SANITIZE)
#define PA_NO_SANITIZE(what)
#endif

// MemorySanitizer annotations.
#if defined(MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>

// Mark a memory region fully initialized.
// Use this to annotate code that deliberately reads uninitialized data, for
// example a GC scavenging root set pointers from the stack.
#define PA_MSAN_UNPOISON(p, size) __msan_unpoison(p, size)
#else  // MEMORY_SANITIZER
#define PA_MSAN_UNPOISON(p, size)
#endif  // MEMORY_SANITIZER

// Macro for hinting that an expression is likely to be false.
#if !defined(PA_UNLIKELY)
#if defined(COMPILER_GCC) || defined(__clang__)
#define PA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define PA_UNLIKELY(x) (x)
#endif  // defined(COMPILER_GCC)
#endif  // !defined(PA_UNLIKELY)

#if !defined(PA_LIKELY)
#if defined(COMPILER_GCC) || defined(__clang__)
#define PA_LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define PA_LIKELY(x) (x)
#endif  // defined(COMPILER_GCC)
#endif  // !defined(PA_LIKELY)

// Compiler feature-detection.
// clang.llvm.org/docs/LanguageExtensions.html#has-feature-and-has-extension
#if defined(__has_feature)
#define PA_HAS_FEATURE(FEATURE) __has_feature(FEATURE)
#else
#define PA_HAS_FEATURE(FEATURE) 0
#endif

#if !defined(PA_CPU_ARM_NEON)
#if defined(__arm__)
#if !defined(__ARMEB__) && !defined(__ARM_EABI__) && !defined(__EABI__) && \
    !defined(__VFP_FP__) && !defined(_WIN32_WCE) && !defined(ANDROID)
#error Chromium does not support middle endian architecture
#endif
#if defined(__ARM_NEON__)
#define PA_CPU_ARM_NEON 1
#endif
#endif  // defined(__arm__)
#endif  // !defined(CPU_ARM_NEON)

#if !defined(PA_HAVE_MIPS_MSA_INTRINSICS)
#if defined(__mips_msa) && defined(__mips_isa_rev) && (__mips_isa_rev >= 5)
#define PA_HAVE_MIPS_MSA_INTRINSICS 1
#endif
#endif

// The ANALYZER_ASSUME_TRUE(bool arg) macro adds compiler-specific hints
// to Clang which control what code paths are statically analyzed,
// and is meant to be used in conjunction with assert & assert-like functions.
// The expression is passed straight through if analysis isn't enabled.
//
// ANALYZER_SKIP_THIS_PATH() suppresses static analysis for the current
// codepath and any other branching codepaths that might follow.
#if defined(__clang_analyzer__)

namespace partition_alloc::internal {

inline constexpr bool AnalyzerNoReturn() __attribute__((analyzer_noreturn)) {
  return false;
}

inline constexpr bool AnalyzerAssumeTrue(bool arg) {
  // PartitionAllocAnalyzerNoReturn() is invoked and analysis is terminated if
  // |arg| is false.
  return arg || AnalyzerNoReturn();
}

}  // namespace partition_alloc::internal

#define PA_ANALYZER_ASSUME_TRUE(arg) \
  ::partition_alloc::internal::AnalyzerAssumeTrue(!!(arg))
#define PA_ANALYZER_SKIP_THIS_PATH() \
  static_cast<void>(::partition_alloc::internal::AnalyzerNoReturn())

#else  // !defined(__clang_analyzer__)

#define PA_ANALYZER_ASSUME_TRUE(arg) (arg)
#define PA_ANALYZER_SKIP_THIS_PATH()

#endif  // defined(__clang_analyzer__)

// Use nomerge attribute to disable optimization of merging multiple same calls.
#if defined(__clang__) && PA_HAS_ATTRIBUTE(nomerge)
#define PA_NOMERGE [[clang::nomerge]]
#else
#define PA_NOMERGE
#endif

// Marks a type as being eligible for the "trivial" ABI despite having a
// non-trivial destructor or copy/move constructor. Such types can be relocated
// after construction by simply copying their memory, which makes them eligible
// to be passed in registers. The canonical example is std::unique_ptr.
//
// Use with caution; this has some subtle effects on constructor/destructor
// ordering and will be very incorrect if the type relies on its address
// remaining constant. When used as a function argument (by value), the value
// may be constructed in the caller's stack frame, passed in a register, and
// then used and destructed in the callee's stack frame. A similar thing can
// occur when values are returned.
//
// TRIVIAL_ABI is not needed for types which have a trivial destructor and
// copy/move constructors, such as base::TimeTicks and other POD.
//
// It is also not likely to be effective on types too large to be passed in one
// or two registers on typical target ABIs.
//
// See also:
//   https://clang.llvm.org/docs/AttributeReference.html#trivial-abi
//   https://libcxx.llvm.org/docs/DesignDocs/UniquePtrTrivialAbi.html
#if defined(__clang__) && PA_HAS_ATTRIBUTE(trivial_abi)
#define PA_TRIVIAL_ABI [[clang::trivial_abi]]
#else
#define PA_TRIVIAL_ABI
#endif

// Requires constant initialization. See constinit in C++20. Allows to rely on a
// variable being initialized before execution, and not requiring a global
// constructor.
#if PA_HAS_ATTRIBUTE(require_constant_initialization)
#define PA_CONSTINIT __attribute__((require_constant_initialization))
#endif
#if !defined(PA_CONSTINIT)
#define PA_CONSTINIT
#endif

#if defined(__clang__)
#define PA_GSL_POINTER [[gsl::Pointer]]
#else
#define PA_GSL_POINTER
#endif

// Constexpr destructors were introduced in C++20. PartitionAlloc's minimum
// supported C++ version is C++17.
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201907L
#define PA_CONSTEXPR_DTOR constexpr
#else
#define PA_CONSTEXPR_DTOR
#endif

// PA_LIFETIME_BOUND indicates that a resource owned by a function parameter or
// implicit object parameter is retained by the return value of the annotated
// function (or, for a parameter of a constructor, in the value of the
// constructed object). This attribute causes warnings to be produced if a
// temporary object does not live long enough.
//
// When applied to a reference parameter, the referenced object is assumed to be
// retained by the return value of the function. When applied to a non-reference
// parameter (for example, a pointer or a class type), all temporaries
// referenced by the parameter are assumed to be retained by the return value of
// the function.
//
// See also the upstream documentation:
// https://clang.llvm.org/docs/AttributeReference.html#lifetimebound
//
// This attribute is based on `ABSL_ATTRIBUTE_LIFETIME_BOUND`, but:
// * A separate definition is provided to avoid PartitionAlloc => Abseil
//   dependency
// * The definition is tweaked to avoid `__attribute__(lifetime))` because it
//   can't be applied in the same places as `[[clang::lifetimebound]]`.  In
//   particular `operator T*&() && __attribute__(lifetime))` fails to compile on
//   `clang` with the following error: 'lifetimebound' attribute only applies to
//   parameters and implicit object parameters
#if PA_HAS_CPP_ATTRIBUTE(clang::lifetimebound)
#define PA_LIFETIME_BOUND [[clang::lifetimebound]]
#else
#define PA_LIFETIME_BOUND
#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_COMPILER_SPECIFIC_H_
