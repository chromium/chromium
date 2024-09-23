// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the default options for various compiler-based dynamic
// tools.

#include "build/build_config.h"

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
// The callbacks we define here will be called from the sanitizer runtime, but
// aren't referenced from the Chrome executable. We must ensure that those
// callbacks are not sanitizer-instrumented, and that they aren't stripped by
// the linker.
#define SANITIZER_HOOK_ATTRIBUTE                                           \
  extern "C"                                                               \
  __attribute__((no_sanitize("address", "memory", "thread", "undefined"))) \
  __attribute__((visibility("default")))                                   \
  __attribute__((used))

// Functions returning default options are declared weak in the tools' runtime
// libraries. To make the linker pick the strong replacements for those
// functions from this module, we explicitly force its inclusion by passing
// -Wl,-u_sanitizer_options_link_helper
// SANITIZER_HOOK_ATTRIBUTE instead of just `extern "C"` solely to make the
// symbol externally visible, for ToolsSanityTest.LinksSanitizerOptions.
SANITIZER_HOOK_ATTRIBUTE void _sanitizer_options_link_helper() {}
#endif

#if defined(ADDRESS_SANITIZER)
// Default options for AddressSanitizer in various configurations:
//   strip_path_prefix=/../../ - prefixes up to and including this
//     substring will be stripped from source file paths in symbolized reports
//   fast_unwind_on_fatal=1 - use the fast (frame-pointer-based) stack unwinder
//     to print error reports. V8 doesn't generate debug info for the JIT code,
//     so the slow unwinder may not work properly.
//   detect_stack_use_after_return=1 - use fake stack to delay the reuse of
//     stack allocations and detect stack-use-after-return errors.
//   symbolize=1 - enable in-process symbolization.
//   external_symbolizer_path=... - provides the path to llvm-symbolizer
//     relative to the main executable
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) | BUILDFLAG(IS_APPLE)
const char kAsanDefaultOptions[] =
    "strip_path_prefix=/../../ fast_unwind_on_fatal=1 "
    "detect_stack_use_after_return=1 symbolize=1 detect_leaks=0 "
    "external_symbolizer_path=%d/../../third_party/llvm-build/Release+Asserts/"
    "bin/llvm-symbolizer";
#elif BUILDFLAG(IS_WIN)
const char* kAsanDefaultOptions =
    "strip_path_prefix=\\..\\..\\ fast_unwind_on_fatal=1 "
    "detect_stack_use_after_return=1 symbolize=1 "
    "external_symbolizer_path=%d/../../third_party/"
    "llvm-build/Release+Asserts/bin/llvm-symbolizer.exe";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_APPLE) || \
    BUILDFLAG(IS_WIN)
// Allow NaCl to override the default asan options.
extern const char* kAsanDefaultOptionsNaCl;
__attribute__((weak)) const char* kAsanDefaultOptionsNaCl = nullptr;

SANITIZER_HOOK_ATTRIBUTE const char *__asan_default_options() {
  if (kAsanDefaultOptionsNaCl)
    return kAsanDefaultOptionsNaCl;
  return kAsanDefaultOptions;
}

extern char kASanDefaultSuppressions[];

SANITIZER_HOOK_ATTRIBUTE const char *__asan_default_suppressions() {
  return kASanDefaultSuppressions;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_APPLE)
        // || BUILDFLAG(IS_WIN)
#endif  // ADDRESS_SANITIZER

#if defined(THREAD_SANITIZER) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
// Default options for ThreadSanitizer in various configurations:
//   second_deadlock_stack=1 - more verbose deadlock reports.
//   report_signal_unsafe=0 - do not report async-signal-unsafe functions
//     called from signal handlers.
//   report_thread_leaks=0 - do not report unjoined threads at the end of
//     the program execution.
//   print_suppressions=1 - print the list of matched suppressions.
//   history_size=7 - make the history buffer proportional to 2^7 (the maximum
//     value) to keep more stack traces.
//   strip_path_prefix=/../../ - prefixes up to and including this
//     substring will be stripped from source file paths in symbolized reports.
//   external_symbolizer_path=... - provides the path to llvm-symbolizer
//     relative to the main executable
const char kTsanDefaultOptions[] =
    "second_deadlock_stack=1 report_signal_unsafe=0 "
    "report_thread_leaks=0 print_suppressions=1 history_size=7 "
    "strip_path_prefix=/../../ external_symbolizer_path=%d/../../third_party/"
    "llvm-build/Release+Asserts/bin/llvm-symbolizer";

SANITIZER_HOOK_ATTRIBUTE const char *__tsan_default_options() {
  return kTsanDefaultOptions;
}

extern char kTSanDefaultSuppressions[];

SANITIZER_HOOK_ATTRIBUTE const char *__tsan_default_suppressions() {
  return kTSanDefaultSuppressions;
}

#endif  // defined(THREAD_SANITIZER) && (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS))

#if defined(MEMORY_SANITIZER)
// Default options for MemorySanitizer:
//   strip_path_prefix=/../../ - prefixes up to and including this
//     substring will be stripped from source file paths in symbolized reports.
//   external_symbolizer_path=... - provides the path to llvm-symbolizer
//     relative to the main executable
const char kMsanDefaultOptions[] =
    "strip_path_prefix=/../../ "
    "external_symbolizer_path=%d/../../third_party/llvm-build/Release+Asserts/"
    "bin/llvm-symbolizer";

SANITIZER_HOOK_ATTRIBUTE const char *__msan_default_options() {
  return kMsanDefaultOptions;
}

#endif  // MEMORY_SANITIZER

#if defined(LEAK_SANITIZER)
// Default options for LeakSanitizer:
//   strip_path_prefix=/../../ - prefixes up to and including this
//     substring will be stripped from source file paths in symbolized reports.
//   external_symbolizer_path=... - provides the path to llvm-symbolizer
//     relative to the main executable
//   use_poisoned=1 - Scan poisoned memory. This is useful for Oilpan (C++
//     garbage collection) which wants to exclude its managed memory from being
//     reported as leaks (through root regions) and also temporarily poisons
//     memory regions before calling destructors of objects to avoid destructors
//     cross-referencing memory in other objects. Main thread termination in
//     Blink is not graceful and leak checks may be emitted at any time, which
//     means that the garbage collector may be in a state with poisoned memory,
//     leading to false-positive reports.
const char kLsanDefaultOptions[] =
    "strip_path_prefix=/../../ use_poisoned=1 "

#if !BUILDFLAG(IS_FUCHSIA)
    "external_symbolizer_path=%d/../../third_party/llvm-build/Release+Asserts/"
    "bin/llvm-symbolizer "
#endif

#if defined(ARCH_CPU_64_BITS)
    // When pointer compression in V8 is enabled the external pointers in the
    // heap are guaranteed to be only 4 bytes aligned. So we need this option
    // in order to ensure that LSAN will find all the external pointers.
    // TODO(crbug.com/40344974): see updates from 2019.
    "use_unaligned=1 "
#endif  // ARCH_CPU_64_BITS
    ;

SANITIZER_HOOK_ATTRIBUTE const char *__lsan_default_options() {
  return kLsanDefaultOptions;
}

// TODO(https://fxbug.dev/102967): Remove when Fuchsia supports
// module-name-based and function-name-based suppression.
#if !BUILDFLAG(IS_FUCHSIA)

extern char kLSanDefaultSuppressions[];

SANITIZER_HOOK_ATTRIBUTE const char *__lsan_default_suppressions() {
  return kLSanDefaultSuppressions;
}

#endif  // !BUILDFLAG(IS_FUCHSIA)
#endif  // LEAK_SANITIZER

#if defined(UNDEFINED_SANITIZER)
// Default options for UndefinedBehaviorSanitizer:
//   print_stacktrace=1 - print the stacktrace when UBSan reports an error.
const char kUbsanDefaultOptions[] =
    "print_stacktrace=1 strip_path_prefix=/../../ "
    "external_symbolizer_path=%d/../../third_party/llvm-build/Release+Asserts/"
    "bin/llvm-symbolizer";

SANITIZER_HOOK_ATTRIBUTE const char* __ubsan_default_options() {
  return kUbsanDefaultOptions;
}

#endif  // UNDEFINED_SANITIZER
