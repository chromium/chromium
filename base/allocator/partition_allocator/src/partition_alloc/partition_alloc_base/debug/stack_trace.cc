// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/debug/stack_trace.h"

#include <cstdint>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/process/process_handle.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread.h"

#if (PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)) && defined(__GLIBC__)
extern "C" void* __libc_stack_end;
#endif

namespace partition_alloc::internal::base::debug {
namespace {

#if PA_BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

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
#if PA_BUILDFLAG(PA_ARCH_CPU_ARM64)
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
  PA_MSAN_UNPOISON(fp_addr, sizeof(uintptr_t));
  return fp_addr[0] - kStackFrameAdjustment;
}

uintptr_t GetStackFramePC(uintptr_t fp) {
  const uintptr_t* fp_addr = reinterpret_cast<const uintptr_t*>(fp);
  PA_MSAN_UNPOISON(&fp_addr[1], sizeof(uintptr_t));
  return StripPointerAuthenticationBits(fp_addr[1]);
}

bool IsStackFrameValid(uintptr_t fp, uintptr_t prev_fp, uintptr_t stack_end) {
  // With the stack growing downwards, older stack frame must be
  // at a greater address that the current one.
  if (fp <= prev_fp) {
    return false;
  }

  // Assume huge stack frames are bogus.
  if (fp - prev_fp > 100000) {
    return false;
  }

  // Check alignment.
  if (fp & (sizeof(uintptr_t) - 1)) {
    return false;
  }

  if (stack_end) {
    // Both fp[0] and fp[1] must be within the stack.
    if (fp > stack_end - 2 * sizeof(uintptr_t)) {
      return false;
    }

    // Additional check to filter out false positives.
    if (GetStackFramePC(fp) < 32768) {
      return false;
    }
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
  uintptr_t last_fp_to_scan =
      std::min(fp + kMaxStackScanArea, stack_end) - sizeof(uintptr_t);
  for (; fp <= last_fp_to_scan; fp += sizeof(uintptr_t)) {
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

#endif  // PA_BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

}  // namespace

#if PA_BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

// We force this function to be inlined into its callers (e.g.
// TraceStackFramePointers()) in all build modes so we don't have to worry about
// conditionally skipping a frame based on potential inlining or tail calls.
__attribute__((always_inline)) size_t TraceStackFramePointersInternal(
    uintptr_t fp,
    uintptr_t stack_end,
    size_t max_depth,
    size_t skip_initial,
    bool enable_scanning,
    const void** out_trace) {
  size_t depth = 0;
  while (depth < max_depth) {
    uintptr_t pc = GetStackFramePC(fp);
    if (skip_initial != 0) {
      skip_initial--;
    } else {
      out_trace[depth++] = reinterpret_cast<const void*>(pc);
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

PA_NOINLINE size_t TraceStackFramePointers(const void** out_trace,
                                           size_t max_depth,
                                           size_t skip_initial,
                                           bool enable_scanning) {
  return TraceStackFramePointersInternal(
      reinterpret_cast<uintptr_t>(__builtin_frame_address(0)) -
          kStackFrameAdjustment,
      GetStackEnd(), max_depth, skip_initial, enable_scanning, out_trace);
}

#endif  // BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

#if PA_BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
uintptr_t GetStackEnd() {
#if PA_BUILDFLAG(IS_ANDROID)
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
  PA_BASE_DCHECK(!error);

  uintptr_t stack_end = stack_begin + stack_size;
  if (is_main_thread) {
    main_stack_end = stack_end;
  }
  return stack_end;  // 0 in case of error
#elif PA_BUILDFLAG(IS_APPLE)
  // No easy way to get end of the stack for non-main threads,
  // see crbug.com/617730.
  return reinterpret_cast<uintptr_t>(pthread_get_stackaddr_np(pthread_self()));
#else

#if (PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)) && defined(__GLIBC__)
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

}  // namespace partition_alloc::internal::base::debug
