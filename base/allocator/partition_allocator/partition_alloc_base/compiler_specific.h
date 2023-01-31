// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_COMPILER_SPECIFIC_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_COMPILER_SPECIFIC_H_

#include "build/build_config.h"

// This is a wrapper around `__has_cpp_attribute`, which can be used to test for
// the presence of an attribute. In case the compiler does not support this
// macro it will simply evaluate to 0.
//
// References:
// https://wg21.link/sd6#testing-for-the-presence-of-an-attribute-__has_cpp_attribute
// https://wg21.link/cpp.cond#:__has_cpp_attribute
#if defined(__has_cpp_attribute)
#define PA_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#define PA_HAS_CPP_ATTRIBUTE(x) 0
#endif

// A wrapper around `__has_attribute`, similar to HAS_CPP_ATTRIBUTE.
#if defined(__has_attribute)
#define PA_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define PA_HAS_ATTRIBUTE(x) 0
#endif

// A wrapper around `__has_builtin`, similar to HAS_CPP_ATTRIBUTE.
#if defined(__has_builtin)
#define PA_HAS_BUILTIN(x) __has_builtin(x)
#else
#define PA_HAS_BUILTIN(x) 0
#endif

// Annotate a function indicating it should not be inlined.
// Use like:
//   NOINLINE void DoStuff() { ... }
#if defined(COMPILER_GCC) || defined(__clang__)
#define PA_NOINLINE __attribute__((noinline))
#elif defined(COMPILER_MSVC)
#define PA_NOINLINE __declspec(noinline)
#else
#define PA_NOINLINE
#endif

#if defined(COMPILER_GCC) && defined(NDEBUG)
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
#define PA_NOT_TAIL_CALLED __attribute__((not_tail_called))
#else
#define PA_NOT_TAIL_CALLED
#endif

// Specify memory alignment for structs, classes, etc.
// Use like:
//   class ALIGNAS(16) MyClass { ... }
//   ALIGNAS(16) int array[4];
//
// In most places you can use the C++11 keyword "alignas", which is preferred.
//
// But compilers have trouble mixing __attribute__((...)) syntax with
// alignas(...) syntax.
//
// Doesn't work in clang or gcc:
//   struct alignas(16) __attribute__((packed)) S { char c; };
// Works in clang but not gcc:
//   struct __attribute__((packed)) alignas(16) S2 { char c; };
// Works in clang and gcc:
//   struct alignas(16) S3 { char c; } __attribute__((packed));
//
// There are also some attributes that must be specified *before* a class
// definition: visibility (used for exporting functions/classes) is one of
// these attributes. This means that it is not possible to use alignas() with a
// class that is marked as exported.
#if defined(COMPILER_MSVC)
#define PA_ALIGNAS(byte_alignment) __declspec(align(byte_alignment))
#elif defined(COMPILER_GCC)
#define PA_ALIGNAS(byte_alignment) __attribute__((aligned(byte_alignment)))
#endif

// In case the compiler supports it NO_UNIQUE_ADDRESS evaluates to the C++20
// attribute [[no_unique_address]]. This allows annotating data members so that
// they need not have an address distinct from all other non-static data members
// of its class.
//
// References:
// * https://en.cppreference.com/w/cpp/language/attributes/no_unique_address
// * https://wg21.link/dcl.attr.nouniqueaddr
#if PA_HAS_CPP_ATTRIBUTE(no_unique_address)
#define PA_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define PA_NO_UNIQUE_ADDRESS
#endif

// Tell the compiler a function is using a printf-style format string.
// |format_param| is the one-based index of the format string parameter;
// |dots_param| is the one-based index of the "..." parameter.
// For v*printf functions (which take a va_list), pass 0 for dots_param.
// (This is undocumented but matches what the system C headers do.)
// For member functions, the implicit this parameter counts as index 1.
#if defined(COMPILER_GCC) || defined(__clang__)
#define PA_PRINTF_FORMAT(format_param, dots_param) \
  __attribute__((format(printf, format_param, dots_param)))
#else
#define PA_PRINTF_FORMAT(format_param, dots_param)
#endif

// WPRINTF_FORMAT is the same, but for wide format strings.
// This doesn't appear to yet be implemented in any compiler.
// See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=38308 .
#define PA_WPRINTF_FORMAT(format_param, dots_param)
// If available, it would look like:
//   __attribute__((format(wprintf, format_param, dots_param)))

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

// Check a memory region for initializedness, as if it was being used here.
// If any bits are uninitialized, crash with an MSan report.
// Use this to sanitize data which MSan won't be able to track, e.g. before
// passing data to another process via shared memory.
#define PA_MSAN_CHECK_MEM_IS_INITIALIZED(p, size) \
  __msan_check_mem_is_initialized(p, size)
#else  // MEMORY_SANITIZER
#define PA_MSAN_UNPOISON(p, size)
#define PA_MSAN_CHECK_MEM_IS_INITIALIZED(p, size)
#endif  // MEMORY_SANITIZER

// Macro useful for writing cross-platform function pointers.
#if !defined(PA_CDECL)
#if BUILDFLAG(IS_WIN)
#define PA_CDECL __cdecl
#else  // BUILDFLAG(IS_WIN)
#define PA_CDECL
#endif  // BUILDFLAG(IS_WIN)
#endif  // !defined(PA_CDECL)

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

#if defined(COMPILER_GCC)
#define PA_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(COMPILER_MSVC)
#define PA_PRETTY_FUNCTION __FUNCSIG__
#else
// See https://en.cppreference.com/w/c/language/function_definition#func
#define PA_PRETTY_FUNCTION __func__
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

#if defined(__clang__) && PA_HAS_ATTRIBUTE(uninitialized)
// Attribute "uninitialized" disables -ftrivial-auto-var-init=pattern for
// the specified variable.
// Library-wide alternative is
// 'configs -= [ "//build/config/compiler:default_init_stack_vars" ]' in .gn
// file.
//
// See "init_stack_vars" in build/config/compiler/BUILD.gn and
// http://crbug.com/977230
// "init_stack_vars" is enabled for non-official builds and we hope to enable it
// in official build in 2020 as well. The flag writes fixed pattern into
// uninitialized parts of all local variables. In rare cases such initialization
// is undesirable and attribute can be used:
//   1. Degraded performance
// In most cases compiler is able to remove additional stores. E.g. if memory is
// never accessed or properly initialized later. Preserved stores mostly will
// not affect program performance. However if compiler failed on some
// performance critical code we can get a visible regression in a benchmark.
//   2. memset, memcpy calls
// Compiler may replaces some memory writes with memset or memcpy calls. This is
// not -ftrivial-auto-var-init specific, but it can happen more likely with the
// flag. It can be a problem if code is not linked with C run-time library.
//
// Note: The flag is security risk mitigation feature. So in future the
// attribute uses should be avoided when possible. However to enable this
// mitigation on the most of the code we need to be less strict now and minimize
// number of exceptions later. So if in doubt feel free to use attribute, but
// please document the problem for someone who is going to cleanup it later.
// E.g. platform, bot, benchmark or test name in patch description or next to
// the attribute.
#define PA_STACK_UNINITIALIZED __attribute__((uninitialized))
#else
#define PA_STACK_UNINITIALIZED
#endif

// Attribute "no_stack_protector" disables -fstack-protector for the specified
// function.
//
// "stack_protector" is enabled on most POSIX builds. The flag adds a canary
// to each stack frame, which on function return is checked against a reference
// canary. If the canaries do not match, it's likely that a stack buffer
// overflow has occurred, so immediately crashing will prevent exploitation in
// many cases.
//
// In some cases it's desirable to remove this, e.g. on hot functions, or if
// we have purposely changed the reference canary.
#if defined(COMPILER_GCC) || defined(__clang__)
#if PA_HAS_ATTRIBUTE(__no_stack_protector__)
#define PA_NO_STACK_PROTECTOR __attribute__((__no_stack_protector__))
#else
#define PA_NO_STACK_PROTECTOR \
  __attribute__((__optimize__("-fno-stack-protector")))
#endif
#else
#define PA_NO_STACK_PROTECTOR
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

// Marks a member function as reinitializing a moved-from variable.
// See also
// https://clang.llvm.org/extra/clang-tidy/checks/bugprone-use-after-move.html#reinitialization
#if defined(__clang__) && PA_HAS_ATTRIBUTE(reinitializes)
#define PA_REINITIALIZES_AFTER_MOVE [[clang::reinitializes]]
#else
#define PA_REINITIALIZES_AFTER_MOVE
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
#define PA_GSL_OWNER [[gsl::Owner]]
#define PA_GSL_POINTER [[gsl::Pointer]]
#else
#define PA_GSL_OWNER
#define PA_GSL_POINTER
#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_COMPILER_SPECIFIC_H_
