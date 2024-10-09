// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"

#include <stddef.h>

#include <limits>
#include <sstream>
#include <string>

#include "base/allocator/buildflags.h"
#include "base/containers/span.h"
#include "base/debug/debugging_buildflags.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier.h"
#include "base/strings/cstring_view.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "partition_alloc/partition_alloc.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/shim/allocator_shim.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "base/test/multiprocess_test.h"
#endif

namespace base {
namespace debug {

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
typedef MultiProcessTest StackTraceTest;
#else
typedef testing::Test StackTraceTest;
#endif
typedef testing::Test StackTraceDeathTest;

#if !defined(__UCLIBC__) && !defined(_AIX)
// StackTrace::OutputToStream() is not implemented under uclibc, nor AIX.
// See https://crbug.com/706728

TEST_F(StackTraceTest, OutputToStream) {
  StackTrace trace;

  // Dump the trace into a string.
  std::ostringstream os;
  trace.OutputToStream(&os);
  std::string backtrace_message = os.str();

  // ToString() should produce the same output.
  EXPECT_EQ(backtrace_message, trace.ToString());

  span<const void* const> addresses = trace.addresses();

#if defined(OFFICIAL_BUILD) && \
    ((BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)) || BUILDFLAG(IS_FUCHSIA))
  // Stack traces require an extra data table that bloats our binaries,
  // so they're turned off for official builds. Stop the test here, so
  // it at least verifies that StackTrace calls don't crash.
  return;
#endif  // defined(OFFICIAL_BUILD) &&
        // ((BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)) ||
        // BUILDFLAG(IS_FUCHSIA))

  ASSERT_GT(addresses.size(), 5u) << "Too few frames found.";
  ASSERT_TRUE(addresses[0]);

  if (!StackTrace::WillSymbolizeToStreamForTesting())
    return;

  // Check if the output has symbol initialization warning.  If it does, fail.
  ASSERT_EQ(backtrace_message.find("Dumping unresolved backtrace"),
            std::string::npos)
      << "Unable to resolve symbols.";

  // Expect a demangled symbol.
  // Note that Windows Release builds omit the function parameters from the
  // demangled stack output, otherwise this could be "testing::UnitTest::Run()".
  EXPECT_TRUE(backtrace_message.find("testing::UnitTest::Run") !=
              std::string::npos)
      << "Expected a demangled symbol in backtrace:\n"
      << backtrace_message;

  // Expect to at least find main.
  EXPECT_TRUE(backtrace_message.find("main") != std::string::npos)
      << "Expected to find main in backtrace:\n"
      << backtrace_message;

  // Expect to find this function as well.
  // Note: This will fail if not linked with -rdynamic (aka -export_dynamic)
  EXPECT_TRUE(backtrace_message.find(__func__) != std::string::npos)
      << "Expected to find " << __func__ << " in backtrace:\n"
      << backtrace_message;
}

#if !defined(OFFICIAL_BUILD) && !defined(NO_UNWIND_TABLES)
// Disabled in Official builds, where Link-Time Optimization can result in two
// or fewer stack frames being available, causing the test to fail.
TEST_F(StackTraceTest, TruncatedTrace) {
  StackTrace trace;

  ASSERT_LT(2u, trace.addresses().size());

  StackTrace truncated(2);
  EXPECT_EQ(2u, truncated.addresses().size());
}
#endif  // !defined(OFFICIAL_BUILD) && !defined(NO_UNWIND_TABLES)

// The test is used for manual testing, e.g., to see the raw output.
TEST_F(StackTraceTest, DebugOutputToStream) {
  StackTrace trace;
  std::ostringstream os;
  trace.OutputToStream(&os);
  VLOG(1) << os.str();
}

// The test is used for manual testing, e.g., to see the raw output.
TEST_F(StackTraceTest, DebugPrintBacktrace) {
  StackTrace().Print();
}

// The test is used for manual testing, e.g., to see the raw output.
TEST_F(StackTraceTest, DebugPrintWithPrefixBacktrace) {
  StackTrace().PrintWithPrefix("[test]");
}

// Make sure nullptr prefix doesn't crash. Output not examined, much
// like the DebugPrintBacktrace test above.
TEST_F(StackTraceTest, DebugPrintWithNullPrefixBacktrace) {
  StackTrace().PrintWithPrefix({});
}

// Test OutputToStreamWithPrefix, mainly to make sure it doesn't
// crash. Any "real" stack trace testing happens above.
TEST_F(StackTraceTest, DebugOutputToStreamWithPrefix) {
  StackTrace trace;
  cstring_view prefix_string = "[test]";
  std::ostringstream os;
  trace.OutputToStreamWithPrefix(&os, prefix_string);
  std::string backtrace_message = os.str();

  // ToStringWithPrefix() should produce the same output.
  EXPECT_EQ(backtrace_message, trace.ToStringWithPrefix(prefix_string));
}

// Make sure nullptr prefix doesn't crash. Output not examined, much
// like the DebugPrintBacktrace test above.
TEST_F(StackTraceTest, DebugOutputToStreamWithNullPrefix) {
  StackTrace trace;
  std::ostringstream os;
  trace.OutputToStreamWithPrefix(&os, {});
  trace.ToStringWithPrefix({});
}

#endif  // !defined(__UCLIBC__) && !defined(_AIX)

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
// Since Mac's base::debug::StackTrace().Print() is not malloc-free, skip
// StackDumpSignalHandlerIsMallocFree if BUILDFLAG(IS_MAC).
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM) && !BUILDFLAG(IS_MAC)

namespace {

// ImmediateCrash if a signal handler incorrectly uses malloc().
// In an actual implementation, this could cause infinite recursion into the
// signal handler or other problems. Because malloc() is not guaranteed to be
// async signal safe.
void* BadMalloc(size_t, void*) {
  base::ImmediateCrash();
}

void* BadCalloc(size_t, size_t, void* context) {
  base::ImmediateCrash();
}

void* BadAlignedAlloc(size_t, size_t, void*) {
  base::ImmediateCrash();
}

void* BadAlignedRealloc(void*, size_t, size_t, void*) {
  base::ImmediateCrash();
}

void* BadRealloc(void*, size_t, void*) {
  base::ImmediateCrash();
}

void BadFree(void*, void*) {
  base::ImmediateCrash();
}

allocator_shim::AllocatorDispatch g_bad_malloc_dispatch = {
    &BadMalloc,         /* alloc_function */
    &BadMalloc,         /* alloc_unchecked_function */
    &BadCalloc,         /* alloc_zero_initialized_function */
    &BadAlignedAlloc,   /* alloc_aligned_function */
    &BadRealloc,        /* realloc_function */
    &BadRealloc,        /* realloc_unchecked_function */
    &BadFree,           /* free_function */
    nullptr,            /* get_size_estimate_function */
    nullptr,            /* good_size_function */
    nullptr,            /* claimed_address_function */
    nullptr,            /* batch_malloc_function */
    nullptr,            /* batch_free_function */
    nullptr,            /* free_definite_size_function */
    nullptr,            /* try_free_default_function */
    &BadAlignedAlloc,   /* aligned_malloc_function */
    &BadAlignedAlloc,   /* aligned_malloc_unchecked_function */
    &BadAlignedRealloc, /* aligned_realloc_function */
    &BadAlignedRealloc, /* aligned_realloc_unchecked_function */
    &BadFree,           /* aligned_free_function */
    nullptr,            /* next */
};

}  // namespace

// Regression test for StackDumpSignalHandler async-signal unsafety.
// Since malloc() is not guaranteed to be async signal safe, it is not allowed
// to use malloc() inside StackDumpSignalHandler().
TEST_F(StackTraceDeathTest, StackDumpSignalHandlerIsMallocFree) {
  EXPECT_DEATH_IF_SUPPORTED(
      [] {
        // On Android, base::debug::EnableInProcessStackDumping() does not
        // change any actions taken by signals to be StackDumpSignalHandler. So
        // the StackDumpSignalHandlerIsMallocFree test doesn't work on Android.
        EnableInProcessStackDumping();
        allocator_shim::InsertAllocatorDispatch(&g_bad_malloc_dispatch);
        // Raise SIGSEGV to invoke StackDumpSignalHandler().
        kill(getpid(), SIGSEGV);
      }(),
      "\\[end of stack trace\\]\n");
}
#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

namespace {

std::string itoa_r_wrapper(intptr_t i, size_t sz, int base, size_t padding) {
  std::array<char, 1024> buffer;
  internal::itoa_r(i, base, padding, base::span(buffer).first(sz));
  EXPECT_NE(buffer[0], '\0');
  for (char c : buffer) {
    if (c == '\0') {
      return std::string(buffer.data());
    }
  }
  ADD_FAILURE() << "buffer is not NUL terminated";
  return std::string("");
}

}  // namespace

TEST_F(StackTraceTest, itoa_r) {
  EXPECT_EQ("0", itoa_r_wrapper(0, 128, 10, 0));
  EXPECT_EQ("-1", itoa_r_wrapper(-1, 128, 10, 0));

  // Test edge cases.
  if (sizeof(intptr_t) == 4) {
    EXPECT_EQ("ffffffff", itoa_r_wrapper(-1, 128, 16, 0));
    EXPECT_EQ("-2147483648",
              itoa_r_wrapper(std::numeric_limits<intptr_t>::min(), 128, 10, 0));
    EXPECT_EQ("2147483647",
              itoa_r_wrapper(std::numeric_limits<intptr_t>::max(), 128, 10, 0));

    EXPECT_EQ("80000000",
              itoa_r_wrapper(std::numeric_limits<intptr_t>::min(), 128, 16, 0));
    EXPECT_EQ("7fffffff",
              itoa_r_wrapper(std::numeric_limits<intptr_t>::max(), 128, 16, 0));
  } else if (sizeof(intptr_t) == 8) {
    EXPECT_EQ("ffffffffffffffff", itoa_r_wrapper(-1, 128, 16, 0));
    EXPECT_EQ("-9223372036854775808",
              itoa_r_wrapper(std::numeric_limits<intptr_t>::min(), 128, 10, 0));
    EXPECT_EQ("9223372036854775807",
              itoa_r_wrapper(std::numeric_limits<intptr_t>::max(), 128, 10, 0));

    EXPECT_EQ("8000000000000000",
              itoa_r_wrapper(std::numeric_limits<intptr_t>::min(), 128, 16, 0));
    EXPECT_EQ("7fffffffffffffff",
              itoa_r_wrapper(std::numeric_limits<intptr_t>::max(), 128, 16, 0));
  } else {
    ADD_FAILURE() << "Missing test case for your size of intptr_t ("
                  << sizeof(intptr_t) << ")";
  }

  // Test hex output.
  EXPECT_EQ("688", itoa_r_wrapper(0x688, 128, 16, 0));
  EXPECT_EQ("deadbeef", itoa_r_wrapper(0xdeadbeef, 128, 16, 0));

  // Check that itoa_r respects passed buffer size limit.
  std::array<char, 1024> buffer;
  internal::itoa_r(0xdeadbeef, 16, 0, base::span(buffer).first(10u));
  EXPECT_NE(buffer[0u], '\0');
  internal::itoa_r(0xdeadbeef, 16, 0, base::span(buffer).first(9u));
  EXPECT_NE(buffer[0u], '\0');
  internal::itoa_r(0xdeadbeef, 16, 0, base::span(buffer).first(8u));
  EXPECT_EQ(buffer[0u], '\0');
  internal::itoa_r(0xdeadbeef, 16, 0, base::span(buffer).first(7u));
  EXPECT_EQ(buffer[0u], '\0');
  internal::itoa_r(0xbeef, 16, 4, base::span(buffer).first(5u));
  EXPECT_NE(buffer[0u], '\0');
  internal::itoa_r(0xbeef, 16, 5, base::span(buffer).first(5u));
  EXPECT_EQ(buffer[0u], '\0');
  internal::itoa_r(0xbeef, 16, 6, base::span(buffer).first(5u));
  EXPECT_EQ(buffer[0u], '\0');

  // Test padding.
  EXPECT_EQ("1", itoa_r_wrapper(1, 128, 10, 0));
  EXPECT_EQ("1", itoa_r_wrapper(1, 128, 10, 1));
  EXPECT_EQ("01", itoa_r_wrapper(1, 128, 10, 2));
  EXPECT_EQ("001", itoa_r_wrapper(1, 128, 10, 3));
  EXPECT_EQ("0001", itoa_r_wrapper(1, 128, 10, 4));
  EXPECT_EQ("00001", itoa_r_wrapper(1, 128, 10, 5));
  EXPECT_EQ("688", itoa_r_wrapper(0x688, 128, 16, 0));
  EXPECT_EQ("688", itoa_r_wrapper(0x688, 128, 16, 1));
  EXPECT_EQ("688", itoa_r_wrapper(0x688, 128, 16, 2));
  EXPECT_EQ("688", itoa_r_wrapper(0x688, 128, 16, 3));
  EXPECT_EQ("0688", itoa_r_wrapper(0x688, 128, 16, 4));
  EXPECT_EQ("00688", itoa_r_wrapper(0x688, 128, 16, 5));
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

class CopyFunction : public StackCopier {
 public:
  using StackCopier::CopyStackContentsAndRewritePointers;
};

template <size_t Depth>
NOINLINE NOOPT void ExpectStackFramePointers(span<const void*> frames) {
code_start:
  // Calling __builtin_frame_address() forces compiler to emit
  // frame pointers, even if they are not enabled.
  EXPECT_NE(nullptr, __builtin_frame_address(0));
  ExpectStackFramePointers<Depth - 1>(frames);

  constexpr size_t frame_index = Depth - 1;
  const void* frame = frames[frame_index];
  EXPECT_GE(frame, &&code_start) << "For frame at index " << frame_index;
  EXPECT_LE(frame, &&code_end) << "For frame at index " << frame_index;
code_end:
  return;
}

template <>
NOINLINE NOOPT void ExpectStackFramePointers<1>(span<const void*> frames) {
code_start:
  // Calling __builtin_frame_address() forces compiler to emit
  // frame pointers, even if they are not enabled.
  EXPECT_NE(nullptr, __builtin_frame_address(0));
  size_t count = TraceStackFramePointers(frames, 0u);
  ASSERT_EQ(frames.size(), count);

  const void* frame = frames[0];
  EXPECT_GE(frame, &&code_start) << "For the top frame";
  EXPECT_LE(frame, &&code_end) << "For the top frame";
code_end:
  return;
}

#if defined(MEMORY_SANITIZER)
// The test triggers use-of-uninitialized-value errors on MSan bots.
// This is expected because we're walking and reading the stack, and
// sometimes we read fp / pc from the place that previously held
// uninitialized value.
#define MAYBE_TraceStackFramePointers DISABLED_TraceStackFramePointers
#else
#define MAYBE_TraceStackFramePointers TraceStackFramePointers
#endif
TEST_F(StackTraceTest, MAYBE_TraceStackFramePointers) {
  constexpr size_t kDepth = 5;
  const void* frames[kDepth];
  ExpectStackFramePointers<kDepth>(frames);
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_StackEnd StackEnd
#else
#define MAYBE_StackEnd DISABLED_StackEnd
#endif

TEST_F(StackTraceTest, MAYBE_StackEnd) {
  EXPECT_NE(0u, GetStackEnd());
}

#endif  // BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)

#if !defined(ADDRESS_SANITIZER) && !defined(UNDEFINED_SANITIZER)

#if !defined(ARCH_CPU_ARM_FAMILY)
// On Arm architecture invalid math operations such as division by zero are not
// trapped and do not trigger a SIGFPE.
// Hence disable the test for Arm platforms.
TEST(CheckExitCodeAfterSignalHandlerDeathTest, CheckSIGFPE) {
  // Values are volatile to prevent reordering of instructions, i.e. for
  // optimization. Reordering may lead to tests erroneously failing due to
  // SIGFPE being raised outside of EXPECT_EXIT.
  volatile int const nominator = 23;
  volatile int const denominator = 0;
  [[maybe_unused]] volatile int result;

  EXPECT_EXIT(result = nominator / denominator,
              ::testing::KilledBySignal(SIGFPE), "");
}
#endif  // !defined(ARCH_CPU_ARM_FAMILY)

TEST(CheckExitCodeAfterSignalHandlerDeathTest, CheckSIGSEGV) {
  // Pointee and pointer are volatile to prevent reordering of instructions,
  // i.e. for optimization. Reordering may lead to tests erroneously failing due
  // to SIGSEGV being raised outside of EXPECT_EXIT.
  volatile int* const volatile p_int = nullptr;

  EXPECT_EXIT(*p_int = 1234, ::testing::KilledBySignal(SIGSEGV), "");
}

#if defined(ARCH_CPU_X86_64)
TEST(CheckExitCodeAfterSignalHandlerDeathTest,
     CheckSIGSEGVNonCanonicalAddress) {
  // Pointee and pointer are volatile to prevent reordering of instructions,
  // i.e. for optimization. Reordering may lead to tests erroneously failing due
  // to SIGSEGV being raised outside of EXPECT_EXIT.
  //
  // On Linux, the upper half of the address space is reserved by the kernel, so
  // all upper bits must be 0 for canonical addresses.
  volatile int* const volatile p_int =
      reinterpret_cast<int*>(0xabcdabcdabcdabcdULL);

  EXPECT_EXIT(*p_int = 1234, ::testing::KilledBySignal(SIGSEGV), "SI_KERNEL");
}
#endif

#endif  // #if !defined(ADDRESS_SANITIZER) && !defined(UNDEFINED_SANITIZER)

TEST(CheckExitCodeAfterSignalHandlerDeathTest, CheckSIGILL) {
  auto const raise_sigill = [] {
#if defined(ARCH_CPU_X86_FAMILY)
    asm("ud2");
#elif defined(ARCH_CPU_ARM_FAMILY)
    asm("udf 0");
#else
#error Unsupported platform!
#endif
  };

  EXPECT_EXIT(raise_sigill(), ::testing::KilledBySignal(SIGILL), "");
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)

}  // namespace debug
}  // namespace base
