// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"

#include <string.h>

#include <algorithm>
#include <sstream>
#include <utility>

#include "base/check_op.h"
#include "base/debug/debugging_buildflags.h"
#include "build/build_config.h"
#include "build/config/compiler/compiler_buildflags.h"

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
#include <optional>

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <pthread.h>

#include "base/process/process_handle.h"
#include "base/threading/platform_thread.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include <pthread.h>
#endif

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(__GLIBC__)
extern "C" void* __libc_stack_end;
#endif

#endif  // BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

namespace base {
namespace debug {

namespace {

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

#if defined(__arm__) && defined(__GNUC__) && !defined(__clang__)
// GCC and LLVM generate slightly different frames on ARM, see
// https://llvm.org/bugs/show_bug.cgi?id=18505 - LLVM generates
// x86-compatible frame, while GCC needs adjustment.
constexpr size_t kStackFrameAdjustment = sizeof(uintptr_t);
#else
constexpr size_t kStackFrameAdjustment = 0;
#endif

// On Arm-v8.3+ systems with pointer authentication codes (PAC), signature bits
// are set in the top bits of the pointer, which confuses test assertions.
// Because the signature size can vary based on the system configuration, use
// the xpaclri instruction to remove the signature.
static uintptr_t StripPointerAuthenticationBits(uintptr_t ptr) {
#if defined(ARCH_CPU_ARM64)
  // A single Chromium binary currently spans all Arm systems (including those
  // with and without pointer authentication). xpaclri is used here because it's
  // in the HINT space and treated as a no-op on older Arm cores (unlike the
  // more generic xpaci which has a new encoding). The downside is that ptr has
  // to be moved to x30 to use this instruction. TODO(richard.townsend@arm.com):
  // replace with an intrinsic once that is available.
  register uintptr_t x30 __asm("x30") = ptr;
  asm("xpaclri" : "+r"(x30));
  return x30;
#else
  // No-op on other platforms.
  return ptr;
#endif
}

uintptr_t GetNextStackFrame(uintptr_t fp) {
  const uintptr_t* fp_addr = reinterpret_cast<const uintptr_t*>(fp);
  // SAFETY: `fp` is the address of an array of pointers. The first element
  // is the next stack frame, the second element is the PC.
  UNSAFE_BUFFERS({
    MSAN_UNPOISON(&fp_addr[0], sizeof(uintptr_t));
    return fp_addr[0] - kStackFrameAdjustment;
  })
}

uintptr_t GetStackFramePC(uintptr_t fp) {
  const uintptr_t* fp_addr = reinterpret_cast<const uintptr_t*>(fp);
  // SAFETY: `fp` is the address of an array of pointers. The first element
  // is the next stack frame, the second element is the PC.
  UNSAFE_BUFFERS({
    MSAN_UNPOISON(&fp_addr[1], sizeof(uintptr_t));
    return StripPointerAuthenticationBits(fp_addr[1]);
  })
}

bool IsStackFrameValid(uintptr_t fp, uintptr_t prev_fp, uintptr_t stack_end) {
  // With the stack growing downwards, older stack frame must be
  // at a greater address that the current one.
  if (fp <= prev_fp) return false;

  // Assume huge stack frames are bogus.
  if (fp - prev_fp > 100000) return false;

  // Check alignment.
  if (fp & (sizeof(uintptr_t) - 1)) return false;

  if (stack_end) {
    // Both fp[0] and fp[1] must be within the stack.
    if (fp > stack_end - 2 * sizeof(uintptr_t)) return false;

    // Additional check to filter out false positives.
    if (GetStackFramePC(fp) < 32768) return false;
  }

  return true;
}

// ScanStackForNextFrame() scans the stack for a valid frame to allow unwinding
// past system libraries. Only supported on Linux where system libraries are
// usually in the middle of the trace:
//
//   TraceStackFramePointers
//   <more frames from Chrome>
//   base::WorkSourceDispatch   <-- unwinding stops (next frame is invalid),
//   g_main_context_dispatch        ScanStackForNextFrame() is called
//   <more frames from glib>
//   g_main_context_iteration
//   base::MessagePumpGlib::Run <-- ScanStackForNextFrame() finds valid frame,
//   base::RunLoop::Run             unwinding resumes
//   <more frames from Chrome>
//   __libc_start_main
//
// ScanStackForNextFrame() returns 0 if it couldn't find a valid frame
// (or if stack scanning is not supported on the current platform).
uintptr_t ScanStackForNextFrame(uintptr_t fp, uintptr_t stack_end) {
  // Enough to resume almost all prematurely terminated traces.
  constexpr size_t kMaxStackScanArea = 8192;

  if (!stack_end) {
    // Too dangerous to scan without knowing where the stack ends.
    return 0;
  }

  fp += sizeof(uintptr_t);  // current frame is known to be invalid
  uintptr_t last_fp_to_scan = std::min(fp + kMaxStackScanArea, stack_end) -
                                  sizeof(uintptr_t);
  for (;fp <= last_fp_to_scan; fp += sizeof(uintptr_t)) {
    uintptr_t next_fp = GetNextStackFrame(fp);
    if (IsStackFrameValid(next_fp, fp, stack_end)) {
      // Check two frames deep. Since stack frame is just a pointer to
      // a higher address on the stack, it's relatively easy to find
      // something that looks like one. However two linked frames are
      // far less likely to be bogus.
      uintptr_t next2_fp = GetNextStackFrame(next_fp);
      if (IsStackFrameValid(next2_fp, next_fp, stack_end)) {
        return fp;
      }
    }
  }

  return 0;
}

// Links stack frame |fp| to |parent_fp|, so that during stack unwinding
// TraceStackFramePointers() visits |parent_fp| after visiting |fp|.
// Both frame pointers must come from __builtin_frame_address().
// Returns previous stack frame |fp| was linked to.
void* LinkStackFrames(void* fpp, void* parent_fp) {
  uintptr_t fp = reinterpret_cast<uintptr_t>(fpp) - kStackFrameAdjustment;
  void* prev_parent_fp = reinterpret_cast<void**>(fp)[0];
  reinterpret_cast<void**>(fp)[0] = parent_fp;
  return prev_parent_fp;
}

#endif  // BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

// A message to be emitted in place of a symbolized stack trace. Ordinarily used
// in death test child processes to inform a developer that they may rerun a
// failing test with a switch to prevent the test launcher from suppressing
// stacks in such processes.
std::string* g_stack_trace_message = nullptr;

// True if an OverrideStackTraceOutputForTesting instance is alive to force
// or prevent generation of symbolized stack traces despite a suppression
// message having been set (or not).
OverrideStackTraceOutputForTesting::Mode g_override_suppression =
    OverrideStackTraceOutputForTesting::Mode::kUnset;

}  // namespace

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
uintptr_t GetStackEnd() {
#if BUILDFLAG(IS_ANDROID)
  // Bionic reads proc/maps on every call to pthread_getattr_np() when called
  // from the main thread. So we need to cache end of stack in that case to get
  // acceptable performance.
  // For all other threads pthread_getattr_np() is fast enough as it just reads
  // values from its pthread_t argument.
  static uintptr_t main_stack_end = 0;

  bool is_main_thread = GetCurrentProcId() == PlatformThread::CurrentId();
  if (is_main_thread && main_stack_end) {
    return main_stack_end;
  }

  uintptr_t stack_begin = 0;
  size_t stack_size = 0;
  pthread_attr_t attributes;
  int error = pthread_getattr_np(pthread_self(), &attributes);
  if (!error) {
    error = pthread_attr_getstack(
        &attributes, reinterpret_cast<void**>(&stack_begin), &stack_size);
    pthread_attr_destroy(&attributes);
  }
  DCHECK(!error);

  uintptr_t stack_end = stack_begin + stack_size;
  if (is_main_thread) {
    main_stack_end = stack_end;
  }
  return stack_end;  // 0 in case of error
#elif BUILDFLAG(IS_APPLE)
  // No easy way to get end of the stack for non-main threads,
  // see crbug.com/617730.
  return reinterpret_cast<uintptr_t>(pthread_get_stackaddr_np(pthread_self()));
#else

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(__GLIBC__)
  if (GetCurrentProcId() == PlatformThread::CurrentId()) {
    // For the main thread we have a shortcut.
    return reinterpret_cast<uintptr_t>(__libc_stack_end);
  }
#endif

  // Don't know how to get end of the stack.
  return 0;
#endif
}
#endif  // BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

StackTrace::StackTrace() : StackTrace(std::size(trace_)) {}

StackTrace::StackTrace(size_t count)
    : count_(ShouldSuppressOutput()
                 ? 0
                 : CollectStackTrace(base::span(trace_).first(
                       std::min(count, std::size(trace_))))) {}

StackTrace::StackTrace(span<const void* const> trace)
    : count_(std::min(trace.size(), std::size(trace_))) {
  if (count_) {
    base::span(trace_).copy_prefix_from(trace.first(count_));
  }
}

// static
bool StackTrace::WillSymbolizeToStreamForTesting() {
#if BUILDFLAG(SYMBOL_LEVEL) == 0
  // Symbols are not expected to be reliable when gn args specifies
  // symbol_level=0.
  return false;
#elif defined(__UCLIBC__) || defined(_AIX)
  // StackTrace::OutputToStream() is not implemented under uclibc, nor AIX.
  // See https://crbug.com/706728
  return false;
#elif defined(OFFICIAL_BUILD) && \
    ((BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)) || BUILDFLAG(IS_FUCHSIA))
  // On some platforms stack traces require an extra data table that bloats our
  // binaries, so they're turned off for official builds.
  return false;
#elif defined(OFFICIAL_BUILD) && BUILDFLAG(IS_APPLE)
  // Official Mac OS X builds contain enough information to unwind the stack,
  // but not enough to symbolize the output.
  return false;
#elif BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_ANDROID)
  // Under Fuchsia and Android, StackTrace emits executable build-Ids and
  // address offsets which are symbolized on the test host system, rather than
  // being symbolized in-process.
  return false;
#elif BUILDFLAG(PRINT_UNSYMBOLIZED_STACK_TRACES)
  // Typically set in sanitizer configurations (ASan, TSan, MSan), which emit
  // unsymbolized stacks and rely on an external script for symbolization.
  return false;
#else
  return true;
#endif
}

void StackTrace::Print() const {
  PrintWithPrefix({});
}

void StackTrace::PrintWithPrefix(cstring_view prefix_string) const {
  if (!count_ || ShouldSuppressOutput()) {
    if (g_stack_trace_message) {
      PrintMessageWithPrefix(prefix_string, *g_stack_trace_message);
    }
    return;
  }
  PrintWithPrefixImpl(prefix_string);
}

void StackTrace::OutputToStream(std::ostream* os) const {
  OutputToStreamWithPrefix(os, {});
}

void StackTrace::OutputToStreamWithPrefix(std::ostream* os,
                                          cstring_view prefix_string) const {
  if (!count_ || ShouldSuppressOutput()) {
    if (g_stack_trace_message) {
      (*os) << prefix_string << *g_stack_trace_message;
    }
    return;
  }
  OutputToStreamWithPrefixImpl(os, prefix_string);
}

std::string StackTrace::ToString() const {
  return ToStringWithPrefix({});
}

std::string StackTrace::ToStringWithPrefix(cstring_view prefix_string) const {
  std::stringstream stream;
#if !defined(__UCLIBC__) && !defined(_AIX)
  OutputToStreamWithPrefix(&stream, prefix_string);
#endif
  return stream.str();
}

// static
void StackTrace::SuppressStackTracesWithMessageForTesting(std::string message) {
  delete std::exchange(
      g_stack_trace_message,
      (message.empty() ? nullptr : new std::string(std::move(message))));
}

// static
bool StackTrace::ShouldSuppressOutput() {
  using Mode = OverrideStackTraceOutputForTesting::Mode;
  // Do not generate stack traces if a suppression message has been provided,
  // unless an OverrideStackTraceOutputForTesting instance is alive.
  return g_override_suppression != Mode::kUnset
             ? (g_override_suppression == Mode::kSuppressOutput)
             : (g_stack_trace_message != nullptr);
}

std::ostream& operator<<(std::ostream& os, const StackTrace& s) {
#if !defined(__UCLIBC__) && !defined(_AIX)
  s.OutputToStream(&os);
#else
  os << "StackTrace::OutputToStream not implemented.";
#endif
  return os;
}

OverrideStackTraceOutputForTesting::OverrideStackTraceOutputForTesting(
    Mode mode) {
  CHECK_NE(mode, Mode::kUnset);
  CHECK_EQ(g_override_suppression, Mode::kUnset);  // Nesting not supported.
  g_override_suppression = mode;
}

OverrideStackTraceOutputForTesting::~OverrideStackTraceOutputForTesting() {
  CHECK_NE(g_override_suppression, Mode::kUnset);  // Nesting not supported.
  g_override_suppression = Mode::kUnset;
}

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

struct AddressRange {
  uintptr_t start;
  uintptr_t end;
};

bool IsWithinRange(uintptr_t address, const AddressRange& range) {
  return address >= range.start && address <= range.end;
}

NOINLINE size_t TraceStackFramePointers(span<const void*> out_trace,
                                        size_t skip_initial,
                                        bool enable_scanning) {
  // Since the stack frame contains the return address (meaning the
  // address of the next instruction in relation to the caller), it
  // is necessary to decrement the size of the call instruction, in
  // order to obtain the address to the call instruction.
#if defined(ARCH_CPU_ARM64)
  static constexpr uintptr_t kCallInstructionSize = 4;
#else
  // For all other ARCH, the call stack may be sightly off by 1 instruction
  static constexpr uintptr_t kCallInstructionSize = 0;
#endif

  uintptr_t fp = reinterpret_cast<uintptr_t>(__builtin_frame_address(0)) -
                 kStackFrameAdjustment;
  uintptr_t stack_end = GetStackEnd();
  size_t depth = 0;
  while (depth < out_trace.size()) {
    uintptr_t pc = GetStackFramePC(fp);
    if (skip_initial != 0) {
      skip_initial--;
    } else {
      out_trace[depth++] =
          reinterpret_cast<const void*>(pc - kCallInstructionSize);
    }

    uintptr_t next_fp = GetNextStackFrame(fp);
    if (IsStackFrameValid(next_fp, fp, stack_end)) {
      fp = next_fp;
      continue;
    }

    if (!enable_scanning) {
      break;
    }

    next_fp = ScanStackForNextFrame(fp, stack_end);
    if (next_fp) {
      fp = next_fp;
    } else {
      break;
    }
  }

  return depth;
}

ScopedStackFrameLinker::ScopedStackFrameLinker(void* fp, void* parent_fp)
    : fp_(fp),
      parent_fp_(parent_fp),
      original_parent_fp_(LinkStackFrames(fp, parent_fp)) {}

ScopedStackFrameLinker::~ScopedStackFrameLinker() {
  void* previous_parent_fp = LinkStackFrames(fp_, original_parent_fp_);
  CHECK_EQ(parent_fp_, previous_parent_fp)
      << "Stack frame's parent pointer has changed!";
}

#endif  // BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

}  // namespace debug
}  // namespace base
