// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"

#include <string.h>

#include <algorithm>
#include <sstream>

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "build/config/compiler/compiler_buildflags.h"

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#include <pthread.h>

#include "base/process/process_handle.h"
#include "base/threading/platform_thread.h"
#endif

#if defined(OS_APPLE)
#include <pthread.h>
#endif

#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(__GLIBC__)
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

uintptr_t GetNextStackFrame(uintptr_t fp) {
  const uintptr_t* fp_addr = reinterpret_cast<const uintptr_t*>(fp);
  MSAN_UNPOISON(fp_addr, sizeof(uintptr_t));
  return fp_addr[0] - kStackFrameAdjustment;
}

uintptr_t GetStackFramePC(uintptr_t fp) {
  const uintptr_t* fp_addr = reinterpret_cast<const uintptr_t*>(fp);
  MSAN_UNPOISON(&fp_addr[1], sizeof(uintptr_t));
  return fp_addr[1];
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

}  // namespace

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
uintptr_t GetStackEnd() {
#if defined(OS_ANDROID)
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
#elif defined(OS_APPLE)
  // No easy way to get end of the stack for non-main threads,
  // see crbug.com/617730.
  return reinterpret_cast<uintptr_t>(pthread_get_stackaddr_np(pthread_self()));
#else

#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(__GLIBC__)
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

StackTrace::StackTrace() : StackTrace(base::size(trace_)) {}

StackTrace::StackTrace(size_t count) {
  count_ = CollectStackTrace(trace_, std::min(count, base::size(trace_)));
}

StackTrace::StackTrace(const void* const* trace, size_t count) {
  count = std::min(count, base::size(trace_));
  if (count)
    memcpy(trace_, trace, count * sizeof(trace_[0]));
  count_ = count;
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
    ((defined(OS_POSIX) && !defined(OS_APPLE)) || defined(OS_FUCHSIA))
  // On some platforms stack traces require an extra data table that bloats our
  // binaries, so they're turned off for official builds.
  return false;
#elif defined(OFFICIAL_BUILD) && defined(OS_APPLE)
  // Official Mac OS X builds contain enough information to unwind the stack,
  // but not enough to symbolize the output.
  return false;
#elif defined(OS_FUCHSIA) || defined(OS_ANDROID)
  // Under Fuchsia and Android, StackTrace emits executable build-Ids and
  // address offsets which are symbolized on the test host system, rather than
  // being symbolized in-process.
  return false;
#elif defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
  // Sanitizer configurations (ASan, TSan, MSan) emit unsymbolized stacks.
  return false;
#else
  return true;
#endif
}

const void *const *StackTrace::Addresses(size_t* count) const {
  *count = count_;
  if (count_)
    return trace_;
  return nullptr;
}

void StackTrace::Print() const {
  PrintWithPrefix(nullptr);
}

void StackTrace::OutputToStream(std::ostream* os) const {
  OutputToStreamWithPrefix(os, nullptr);
}

std::string StackTrace::ToString() const {
  return ToStringWithPrefix(nullptr);
}
std::string StackTrace::ToStringWithPrefix(const char* prefix_string) const {
  std::stringstream stream;
#if !defined(__UCLIBC__) && !defined(_AIX)
  OutputToStreamWithPrefix(&stream, prefix_string);
#endif
  return stream.str();
}

std::ostream& operator<<(std::ostream& os, const StackTrace& s) {
#if !defined(__UCLIBC__) && !defined(_AIX)
  s.OutputToStream(&os);
#else
  os << "StackTrace::OutputToStream not implemented.";
#endif
  return os;
}

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

struct AddressRange {
  uintptr_t start;
  uintptr_t end;
};

bool IsWithinRange(uintptr_t address, const AddressRange& range) {
  return address >= range.start && address <= range.end;
}

size_t TraceStackFramePointersInternal(
    absl::optional<uintptr_t> fp,
    uintptr_t stack_end,
    size_t max_depth,
    size_t skip_initial,
    bool enable_scanning,
    absl::optional<AddressRange> caller_function_range,
    const void** out_trace) {
  // If |fp| is not provided then try to unwind the current stack. In this case
  // the caller function cannot pass in it's own frame pointer to unwind
  // because the frame pointer may not be valid here. The compiler can optimize
  // the tail function call from the caller to skip to the previous frame of the
  // caller directly, making it's frame pointer invalid when we reach this
  // function.
  if (!fp) {
    // Usage of __builtin_frame_address() enables frame pointers in this
    // function even if they are not enabled globally. So 'fp' will always
    // be valid.
    fp = reinterpret_cast<uintptr_t>(__builtin_frame_address(0)) -
         kStackFrameAdjustment;
  }

  size_t depth = 0;
  while (depth < max_depth) {
    uintptr_t pc = GetStackFramePC(*fp);
    // Case 1: If we are unwinding on a copied stack, then
    // |caller_function_range| will not exist.
    //
    // Case 2: If we are unwinding the current stack from this function's frame,
    // the next frame could be either the caller (TraceStackFramePointers()) or
    // the function that called TraceStackFramePointers() (say Fn()).
    //
    // 2a. If the current function (depending on optimization of the build) is
    // inlined, or the tail call to this function from TraceStackFramePointers()
    // causes the frame pointer to skip directly to Fn(), the stack will look
    // like this:
    //    1st Frame: TraceStackFramePointersInternal()
    //               TraceStackFramePointers() has no frame
    //    2nd Frame: Fn()
    //    ...
    //  In this case we do not want to skip the caller from the output.
    //
    //  2b. Otherwise the stack will look like this:
    //    1st Frame: TraceStackFramePointersInternal()
    //    2nd Frame: <stack space of TraceStackFramePointers()>   <- Skip
    //    3rd Frame: Fn()
    //  In this case, the next pc will be within the caller function's
    //  addresses, so skip the frame.
    if (!caller_function_range || !IsWithinRange(pc, *caller_function_range)) {
      if (skip_initial != 0) {
        skip_initial--;
      } else {
        out_trace[depth++] = reinterpret_cast<const void*>(pc);
      }
    }

    uintptr_t next_fp = GetNextStackFrame(*fp);
    if (IsStackFrameValid(next_fp, *fp, stack_end)) {
      fp = next_fp;
      continue;
    }

    if (!enable_scanning)
      break;

    next_fp = ScanStackForNextFrame(*fp, stack_end);
    if (next_fp) {
      fp = next_fp;
    } else {
      break;
    }
  }

  return depth;
}

size_t TraceStackFramePointers(const void** out_trace,
                               size_t max_depth,
                               size_t skip_initial,
                               bool enable_scanning) {
  // This function's frame can be skipped by the compiler since the callee
  // function can jump to caller of this function directly while execution.
  // Since there is no way to guarantee that the first frame the trace stack
  // function finds will be this function or the previous function, skip the
  // current function if it is found.
TraceStackFramePointers_start:
  AddressRange current_fn_range = {
      reinterpret_cast<uintptr_t>(&&TraceStackFramePointers_start),
      reinterpret_cast<uintptr_t>(&&TraceStackFramePointers_end)};
  size_t depth = TraceStackFramePointersInternal(
      /*fp=*/absl::nullopt, GetStackEnd(), max_depth, skip_initial,
      enable_scanning, current_fn_range, out_trace);
TraceStackFramePointers_end:
  return depth;
}

size_t TraceStackFramePointersFromBuffer(uintptr_t fp,
                                         uintptr_t stack_end,
                                         const void** out_trace,
                                         size_t max_depth,
                                         size_t skip_initial,
                                         bool enable_scanning) {
  return TraceStackFramePointersInternal(fp, stack_end, max_depth, skip_initial,
                                         enable_scanning, absl::nullopt,
                                         out_trace);
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
