// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_COMPILER_SPECIFIC_H_
#define BASE_COMPILER_SPECIFIC_H_

#include "build/build_config.h"

#if defined(COMPILER_MSVC) && !defined(__clang__)
#error "Only clang-cl is supported on Windows, see https://crbug.com/988071"
#endif

// This is a wrapper around `__has_cpp_attribute`, which can be used to test for
// the presence of an attribute. In case the compiler does not support this
// macro it will simply evaluate to 0.
// 这是 `__has_cpp_attribute` 的包装器，可用于测试属性是否存在。如果编译器不支持这个宏，它只会计算为0.
//
// References:
// https://wg21.link/sd6#testing-for-the-presence-of-an-attribute-__has_cpp_attribute
// https://wg21.link/cpp.cond#:__has_cpp_attribute
#if defined(__has_cpp_attribute)
#define HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#define HAS_CPP_ATTRIBUTE(x) 0
#endif

// A wrapper around `__has_builtin`, similar to HAS_CPP_ATTRIBUTE.
#if defined(__has_builtin)
#define HAS_BUILTIN(x) __has_builtin(x)
#else
#define HAS_BUILTIN(x) 0
#endif

// Annotate a variable indicating it's ok if the variable is not used.
// 注释一个变量，表明如果不使用该变量是可以的。
// (Typically used to silence a compiler warning when the assignment
// is important for some other reason.)
// （通常用于在由于某些其他原因分配很重要时使编译器警告静音。）
// Use like:
//   int x = ...;
//   ALLOW_UNUSED_LOCAL(x);
#define ALLOW_UNUSED_LOCAL(x) (void)x

// Annotate a typedef or function indicating it's ok if it's not used.
// 注释 typedef 或函数，表明如果不使用它是可以的。
// Use like:
//   typedef Foo Bar ALLOW_UNUSED_TYPE;
#if defined(COMPILER_GCC) || defined(__clang__)
#define ALLOW_UNUSED_TYPE __attribute__((unused))
#else
#define ALLOW_UNUSED_TYPE
#endif

// Annotate a function indicating it should not be inlined.
// 注释一个函数，表明它不应该被内联。
// Use like:
//   NOINLINE void DoStuff() { ... }
#if defined(COMPILER_GCC) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#elif defined(COMPILER_MSVC)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE
#endif

// 表示应该被内联
#if defined(COMPILER_GCC) && defined(NDEBUG)
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined(COMPILER_MSVC) && defined(NDEBUG)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE inline
#endif

// Annotate a function indicating it should never be tail called. Useful to make
// sure callers of the annotated function are never omitted from call-stacks.
// 注释一个函数，表明它不应该被尾调用。 有助于确保注释函数的调用者永远不会从调用堆栈中省略。
// To provide the complementary behavior (prevent the annotated function from
// being omitted) look at NOINLINE. Also note that this doesn't prevent code
// folding of multiple identical caller functions into a single signature. To
// prevent code folding, see NO_CODE_FOLDING() in base/debug/alias.h.
// 要提供补充行为（防止注释函数被省略），请查看 NOINLINE。 另请注意，这不会阻止将多个相同调用函
// 数的代码折叠成一个签名。 要防止代码折叠，请参阅 base/debug/alias.h 中的 NO_CODE_FOLDING()。
// Use like:
//   void NOT_TAIL_CALLED FooBar();
// 尾调用(Tail call)概念解释：函数式编程的一个重要概念. 尾调用的概念非常简单，一句话就能说清楚，
// 就是指某个函数的最后一步是调用另一个函数。例如：
// void f(x) {
//   return g(x);
// }
// 上面代码中，函数f的最后一步是调用函数g，这就叫尾调用。
// 尾调用之所以与其他调用不同，就在于它的特殊的调用位置，会导致尾调用优化。
// 我们知道，函数调用会在内存形成一个"调用记录"，又称"调用帧"（call frame），保存调用位置和内部
// 变量等信息。如果在函数A的内部调用函数B，那么在A的调用记录上方，还会形成一个B的调用记录。等到B运
// 行结束，将结果返回到A，B的调用记录才会消失。如果函数B内部还调用函数C，那就还有一个C的调用记录栈，
// 以此类推。所有的调用记录，就形成一个"调用栈"（call stack）。
// 尾调用由于是函数的最后一步操作，所以不需要保留外层函数的调用记录，因为调用位置、内部变量等信息都
// 不会再用到了，只要直接用内层函数的调用记录，取代外层函数的调用记录就可以了，这就叫做"尾调用优化
// "（Tail call optimization）
// 即只保留内层函数的调用记录。如果所有函数都是尾调用，那么完全可以做到每次执行时，调用记录只有一项，
// 这将大大节省内存。这就是"尾调用优化"的意义。
// 参见：https://www.ruanyifeng.com/blog/2015/04/tail-call.html

#if defined(__clang__) && __has_attribute(not_tail_called)
#define NOT_TAIL_CALLED __attribute__((not_tail_called))
#else
#define NOT_TAIL_CALLED
#endif

// Specify memory alignment for structs, classes, etc.
// 指定结构、类等的内存对齐方式。
// Use like:
//   class ALIGNAS(16) MyClass { ... }
//   ALIGNAS(16) int array[4];
//
// In most places you can use the C++11 keyword "alignas", which is preferred.
// 在大多数地方，您可以使用 C++11 关键字“alignas”，这是首选。
//
// But compilers have trouble mixing __attribute__((...)) syntax with
// alignas(...) syntax.
// 但是编译器在混合 __attribute__((...)) 语法和 alignas(...) 语法时遇到了麻烦。
//
// Doesn't work in clang or gcc:
//   struct alignas(16) __attribute__((packed)) S { char c; };
// Works in clang but not gcc:
//   struct __attribute__((packed)) alignas(16) S2 { char c; };
// Works in clang and gcc:
//   struct alignas(16) S3 { char c; } __attribute__((packed));
// 也就是说，这里告诫我们不要混用C++11中的 alignas关键字 与 __attribute__编译期属性。
//
// There are also some attributes that must be specified *before* a class
// definition: visibility (used for exporting functions/classes) is one of
// these attributes. This means that it is not possible to use alignas() with
// a class that is marked as exported.
// 还有一些属性必须在类定义之前*指定：可见性（用于导出函数/类）是这些属性之一。 这意味着
// 不能将 alignas() 与标记为导出的类一起使用。
#if defined(COMPILER_MSVC)
#define ALIGNAS(byte_alignment) __declspec(align(byte_alignment))
#elif defined(COMPILER_GCC)
#define ALIGNAS(byte_alignment) __attribute__((aligned(byte_alignment)))
#endif

// Annotate a function indicating the caller must examine the return value.
// 注释一个函数，指示调用者必须检查返回值。
// Use like:
//   int foo() WARN_UNUSED_RESULT;
// To explicitly ignore a result, see |ignore_result()| in base/macros.h.
#undef WARN_UNUSED_RESULT
#if defined(COMPILER_GCC) || defined(__clang__)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define WARN_UNUSED_RESULT
#endif

// In case the compiler supports it NO_UNIQUE_ADDRESS evaluates to the C++20
// attribute [[no_unique_address]]. This allows annotating data members so that
// they need not have an address distinct from all other non-static data members
// of its class.
// 如果编译器支持它，则 NO_UNIQUE_ADDRESS 计算为 C++20 属性 [[no_unique_address]]。
// 这允许注释数据成员，以便它们不需要具有不同于其类的所有其他非静态数据成员的地址。
//
// References:
// * https://en.cppreference.com/w/cpp/language/attributes/no_unique_address
// * https://wg21.link/dcl.attr.nouniqueaddr
// 允许 此数据成员 与 其他非静态数据成员 或 其类的基类子对象 重合。C++20才支持。
//
#if HAS_CPP_ATTRIBUTE(no_unique_address)
#define NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define NO_UNIQUE_ADDRESS
#endif

// Tell the compiler a function is using a printf-style format string.
// 告诉编译器一个函数正在使用 printf 样式的格式字符串。
// |format_param| is the one-based index of the format string parameter;
// |dots_param| is the one-based index of the "..." parameter.
// |格式参数| 是格式字符串参数的从一开始的索引；|点参数| 是“...”参数的从 1 开始的索引。
// For v*printf functions (which take a va_list), pass 0 for dots_param.
// 对于 v*printf 函数（采用 va_list），为 dots_param 传递 0。
// (This is undocumented but matches what the system C headers do.)
// （这是未记录的，但与系统 C 标头的功能相匹配。）
// For member functions, the implicit this parameter counts as index 1.
// 对于成员函数，隐含的 this 参数计为索引 1。
#if defined(COMPILER_GCC) || defined(__clang__)
#define PRINTF_FORMAT(format_param, dots_param) \
  __attribute__((format(printf, format_param, dots_param)))
#else
#define PRINTF_FORMAT(format_param, dots_param)
#endif

// WPRINTF_FORMAT is the same, but for wide format strings.
// WPRINTF_FORMAT 是相同的，但用于宽格式字符串。
// This doesn't appear to yet be implemented in any compiler.
// 这似乎还没有在任何编译器中实现。
// See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=38308 .
#define WPRINTF_FORMAT(format_param, dots_param)
// If available, it would look like:
//   __attribute__((format(wprintf, format_param, dots_param)))

// Sanitizers annotations.
// 消毒剂注释。
// 在函数声明中使用 no_sanitize 属性来指定特定的检测或检测集合不应该应用于该函数.
// 该属性采用字符串字面值列表，其与-fno-sanitize=标志接受的值具有相同的含义.例如:
// attribute((no_sanitize("address"，"thread"))) 指定 AddressSanitizer 和
// ThreadSanitizer 不应用于该函数.
#if defined(__has_attribute)
#if __has_attribute(no_sanitize)
#define NO_SANITIZE(what) __attribute__((no_sanitize(what)))
#endif
#endif
#if !defined(NO_SANITIZE)
#define NO_SANITIZE(what)
#endif

// MemorySanitizer annotations.
#if defined(MEMORY_SANITIZER) && !defined(OS_NACL)
#include <sanitizer/msan_interface.h>

// Mark a memory region fully initialized.
// 将内存区域标记为完全初始化。
// Use this to annotate code that deliberately reads uninitialized data, for
// example a GC scavenging root set pointers from the stack.
// 使用它来注释故意读取未初始化数据的代码，例如 GC 从堆栈中清除根集指针。
#define MSAN_UNPOISON(p, size) __msan_unpoison(p, size)

// Check a memory region for initializedness, as if it was being used here.
// 检查内存区域的初始化，就好像它在这里被使用一样。
// If any bits are uninitialized, crash with an MSan report.
// 如果任何位未初始化，则使用 MSan 报告崩溃。
// Use this to sanitize data which MSan won't be able to track, e.g. before
// passing data to another process via shared memory.
// 使用它来清理 MSan 无法跟踪的数据，例如 在通过共享内存将数据传递给另一个进程之前。
#define MSAN_CHECK_MEM_IS_INITIALIZED(p, size) \
  __msan_check_mem_is_initialized(p, size)
#else  // MEMORY_SANITIZER
#define MSAN_UNPOISON(p, size)
#define MSAN_CHECK_MEM_IS_INITIALIZED(p, size)
#endif  // MEMORY_SANITIZER

// DISABLE_CFI_PERF -- Disable Control Flow Integrity for perf reasons.
// 出于性能原因禁用控制流完整性。
#if !defined(DISABLE_CFI_PERF)
#if defined(__clang__) && defined(OFFICIAL_BUILD)
#define DISABLE_CFI_PERF __attribute__((no_sanitize("cfi")))
#else
#define DISABLE_CFI_PERF
#endif
#endif

// DISABLE_CFI_ICALL -- Disable Control Flow Integrity indirect call checks.
#if !defined(DISABLE_CFI_ICALL)
#if defined(OS_WIN)
// Windows also needs __declspec(guard(nocf)).
#define DISABLE_CFI_ICALL NO_SANITIZE("cfi-icall") __declspec(guard(nocf))
#else
#define DISABLE_CFI_ICALL NO_SANITIZE("cfi-icall")
#endif
#endif
#if !defined(DISABLE_CFI_ICALL)
#define DISABLE_CFI_ICALL
#endif

// Macro useful for writing cross-platform function pointers.
#if !defined(CDECL)
#if defined(OS_WIN)
#define CDECL __cdecl
#else  // defined(OS_WIN)
#define CDECL
#endif  // defined(OS_WIN)
#endif  // !defined(CDECL)

// Macro for hinting that an expression is likely to be false.
#if !defined(UNLIKELY)
#if defined(COMPILER_GCC) || defined(__clang__)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define UNLIKELY(x) (x)
#endif  // defined(COMPILER_GCC)
#endif  // !defined(UNLIKELY)

#if !defined(LIKELY)
#if defined(COMPILER_GCC) || defined(__clang__)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define LIKELY(x) (x)
#endif  // defined(COMPILER_GCC)
#endif  // !defined(LIKELY)

// Compiler feature-detection.
// clang.llvm.org/docs/LanguageExtensions.html#has-feature-and-has-extension
#if defined(__has_feature)
#define HAS_FEATURE(FEATURE) __has_feature(FEATURE)
#else
#define HAS_FEATURE(FEATURE) 0
#endif

// Macro for telling -Wimplicit-fallthrough that a fallthrough is intentional.
#if defined(__clang__)
#define FALLTHROUGH [[clang::fallthrough]]
#else
#define FALLTHROUGH
#endif

#if defined(COMPILER_GCC)
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(COMPILER_MSVC)
#define PRETTY_FUNCTION __FUNCSIG__
#else
// See https://en.cppreference.com/w/c/language/function_definition#func
#define PRETTY_FUNCTION __func__
#endif

#if !defined(CPU_ARM_NEON)
#if defined(__arm__)
#if !defined(__ARMEB__) && !defined(__ARM_EABI__) && !defined(__EABI__) && \
    !defined(__VFP_FP__) && !defined(_WIN32_WCE) && !defined(ANDROID)
#error Chromium does not support middle endian architecture
#endif
#if defined(__ARM_NEON__)
#define CPU_ARM_NEON 1
#endif
#endif  // defined(__arm__)
#endif  // !defined(CPU_ARM_NEON)

#if !defined(HAVE_MIPS_MSA_INTRINSICS)
#if defined(__mips_msa) && defined(__mips_isa_rev) && (__mips_isa_rev >= 5)
#define HAVE_MIPS_MSA_INTRINSICS 1
#endif
#endif

#if defined(__clang__) && __has_attribute(uninitialized)
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
#define STACK_UNINITIALIZED __attribute__((uninitialized))
#else
#define STACK_UNINITIALIZED
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
#if defined(__has_attribute)
#if __has_attribute(__no_stack_protector__)
#define NO_STACK_PROTECTOR __attribute__((__no_stack_protector__))
#else  // __has_attribute(__no_stack_protector__)
#define NO_STACK_PROTECTOR __attribute__((__optimize__("-fno-stack-protector")))
#endif
#else  // defined(__has_attribute)
#define NO_STACK_PROTECTOR __attribute__((__optimize__("-fno-stack-protector")))
#endif
#else
#define NO_STACK_PROTECTOR
#endif

// The ANALYZER_ASSUME_TRUE(bool arg) macro adds compiler-specific hints
// to Clang which control what code paths are statically analyzed,
// and is meant to be used in conjunction with assert & assert-like functions.
// The expression is passed straight through if analysis isn't enabled.
//
// ANALYZER_SKIP_THIS_PATH() suppresses static analysis for the current
// codepath and any other branching codepaths that might follow.
#if defined(__clang_analyzer__)

inline constexpr bool AnalyzerNoReturn() __attribute__((analyzer_noreturn)) {
  return false;
}

inline constexpr bool AnalyzerAssumeTrue(bool arg) {
  // AnalyzerNoReturn() is invoked and analysis is terminated if |arg| is
  // false.
  return arg || AnalyzerNoReturn();
}

#define ANALYZER_ASSUME_TRUE(arg) ::AnalyzerAssumeTrue(!!(arg))
#define ANALYZER_SKIP_THIS_PATH() static_cast<void>(::AnalyzerNoReturn())
#define ANALYZER_ALLOW_UNUSED(var) static_cast<void>(var);

#else  // !defined(__clang_analyzer__)

#define ANALYZER_ASSUME_TRUE(arg) (arg)
#define ANALYZER_SKIP_THIS_PATH()
#define ANALYZER_ALLOW_UNUSED(var) static_cast<void>(var);

#endif  // defined(__clang_analyzer__)

// Use nomerge attribute to disable optimization of merging multiple same calls.
#if defined(__clang__) && __has_attribute(nomerge)
#define NOMERGE [[clang::nomerge]]
#else
#define NOMERGE
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
#if defined(__clang__) && __has_attribute(trivial_abi)
#define TRIVIAL_ABI [[clang::trivial_abi]]
#else
#define TRIVIAL_ABI
#endif

// Marks a member function as reinitializing a moved-from variable.
// See also
// https://clang.llvm.org/extra/clang-tidy/checks/bugprone-use-after-move.html#reinitialization
#if defined(__clang__) && __has_attribute(reinitializes)
#define REINITIALIZES_AFTER_MOVE [[clang::reinitializes]]
#else
#define REINITIALIZES_AFTER_MOVE
#endif

// Requires constant initialization. See constinit in C++20. Allows to rely on a
// variable being initialized before execution, and not requiring a global
// constructor.
#if defined(__has_attribute)
#if __has_attribute(require_constant_initialization)
#define CONSTINIT __attribute__((require_constant_initialization))
#endif
#endif
#if !defined(CONSTINIT)
#define CONSTINIT
#endif

#endif  // BASE_COMPILER_SPECIFIC_H_
