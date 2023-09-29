// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_copier_signal.h"

#include <errno.h>
#include <linux/futex.h>
#include <signal.h>
#include <stdint.h>
#include <sys/ucontext.h>
#include <syscall.h>

#include <atomic>
#include <cstring>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/profiler/register_context.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/suspendable_thread_delegate.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

namespace {

// Waitable event implementation with futex and without DCHECK(s), since signal
// handlers cannot allocate memory or use pthread api.
class AsyncSafeWaitableEvent {
 public:
  AsyncSafeWaitableEvent() {
    futex_.store(kNotSignaled, std::memory_order_release);
  }
  ~AsyncSafeWaitableEvent() = default;
  AsyncSafeWaitableEvent(const AsyncSafeWaitableEvent&) = delete;
  AsyncSafeWaitableEvent& operator=(const AsyncSafeWaitableEvent&) = delete;

  bool Wait() {
    // futex() can wake up spuriously if this memory address was previously used
    // for a pthread mutex or we get a signal. So, also check the condition.
    while (true) {
      long res =
          syscall(SYS_futex, futex_ptr(), FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
                  kNotSignaled, nullptr, nullptr, 0);
      int futex_errno = errno;
      if (futex_.load(std::memory_order_acquire) != kNotSignaled) {
        return true;
      }
      if (res != 0) {
        // EINTR indicates the wait was interrupted by a signal; retry the wait.
        // EAGAIN happens if this thread sees the FUTEX_WAKE before it sees the
        // atomic_int store in Signal. This can't happen in an unoptimized
        // single total modification order threading model; however, since we
        // using release-acquire semantics on the atomic_uint32_t, it might be.
        // (The futex docs aren't clear what memory/threading model they are
        // using.)
        if (futex_errno != EINTR && futex_errno != EAGAIN) {
          return false;
        }
      }
    }
  }

  void Signal() {
    futex_.store(kSignaled, std::memory_order_release);
    syscall(SYS_futex, futex_ptr(), FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, nullptr,
            nullptr, 0);
  }

 private:
  // The possible values in the futex / atomic_int.
  static constexpr uint32_t kNotSignaled = 0;
  static constexpr uint32_t kSignaled = 1;

  // Provides a pointer to the atomic's storage. std::atomic_uint32_t has
  // standard layout so its address can be used for the pointer as long as it
  // only contains the uint32_t.
  uint32_t* futex_ptr() {
    // futex documents state the futex is 32 bits regardless of the platform
    // size.
    static_assert(sizeof(futex_) == sizeof(uint32_t),
                  "Expected std::atomic_uint32_t to be the same size as "
                  "uint32_t");
    return reinterpret_cast<uint32_t*>(&futex_);
  }

  std::atomic_uint32_t futex_{kNotSignaled};
};

// Scoped signal event that calls Signal on the AsyncSafeWaitableEvent at
// destructor.
class ScopedEventSignaller {
 public:
  ScopedEventSignaller(AsyncSafeWaitableEvent* event,
                       absl::optional<TimeTicks>* signal_time)
      : event_(event), signal_time_(signal_time) {}
  ~ScopedEventSignaller() {
    *signal_time_ = subtle::MaybeTimeTicksNowIgnoringOverride();
    event_->Signal();
  }

 private:
  raw_ptr<AsyncSafeWaitableEvent> event_;
  raw_ptr<absl::optional<TimeTicks>> signal_time_;
};

// Struct to store the arguments to the signal handler.
struct HandlerParams {
  uintptr_t stack_base_address;

  // The event is signalled when signal handler is done executing.
  raw_ptr<AsyncSafeWaitableEvent> event;

  // Return values:

  // Successfully copied the stack segment.
  raw_ptr<bool> success;

  // The thread context of the leaf function.
  raw_ptr<mcontext_t> context;

  // Buffer to copy the stack segment.
  raw_ptr<StackBuffer> stack_buffer;
  raw_ptr<const uint8_t*> stack_copy_bottom;

  // The timestamp when the stack was copied.
  raw_ptr<absl::optional<TimeTicks>> maybe_timestamp;

  // The timestamp when |event| was signaled.
  raw_ptr<absl::optional<TimeTicks>> maybe_timestamp_signaled;

  // The delegate provided to the StackCopier.
  raw_ptr<StackCopier::Delegate> stack_copier_delegate;
};

// Pointer to the parameters to be "passed" to the CopyStackSignalHandler() from
// the sampling thread to the sampled (stopped) thread. This value is set just
// before sending the signal to the thread and reset when the handler is done.
std::atomic<HandlerParams*> g_handler_params;

// CopyStackSignalHandler is invoked on the stopped thread and records the
// thread's stack and register context at the time the signal was received. This
// function may only call reentrant code.
void CopyStackSignalHandler(int n, siginfo_t* siginfo, void* sigcontext) {
  HandlerParams* params = g_handler_params.load(std::memory_order_acquire);

  // MaybeTimeTicksNowIgnoringOverride() is implemented in terms of
  // clock_gettime on Linux, which is signal safe per the signal-safety(7) man
  // page, but is not garanteed to succeed, in which case absl::nullopt is
  // returned. TimeTicks::Now() can't be used because it expects clock_gettime
  // to always succeed and is thus not signal-safe.
  *params->maybe_timestamp = subtle::MaybeTimeTicksNowIgnoringOverride();

  ScopedEventSignaller e(params->event, params->maybe_timestamp_signaled);
  *params->success = false;

  const ucontext_t* ucontext = static_cast<ucontext_t*>(sigcontext);
  std::memcpy(params->context, &ucontext->uc_mcontext, sizeof(mcontext_t));

  const uintptr_t bottom = RegisterContextStackPointer(params->context);
  const uintptr_t top = params->stack_base_address;
  if ((top - bottom) > params->stack_buffer->size()) {
    // The stack exceeds the size of the allocated buffer. The buffer is sized
    // such that this shouldn't happen under typical execution so we can safely
    // punt in this situation.
    return;
  }

  params->stack_copier_delegate->OnStackCopy();

  *params->stack_copy_bottom =
      StackCopierSignal::CopyStackContentsAndRewritePointers(
          reinterpret_cast<uint8_t*>(bottom), reinterpret_cast<uintptr_t*>(top),
          StackBuffer::kPlatformStackAlignment, params->stack_buffer->buffer());

  *params->success = true;
}

// Sets the global handler params for the signal handler function.
class ScopedSetSignalHandlerParams {
 public:
  ScopedSetSignalHandlerParams(HandlerParams* params) {
    g_handler_params.store(params, std::memory_order_release);
  }

  ~ScopedSetSignalHandlerParams() {
    g_handler_params.store(nullptr, std::memory_order_release);
  }
};

class ScopedSigaction {
 public:
  ScopedSigaction(int signal,
                  struct sigaction* action,
                  struct sigaction* original_action)
      : signal_(signal),
        action_(action),
        original_action_(original_action),
        succeeded_(sigaction(signal, action, original_action) == 0) {}

  bool succeeded() const { return succeeded_; }

  ~ScopedSigaction() {
    if (!succeeded_)
      return;

    bool reset_succeeded = sigaction(signal_, original_action_, action_) == 0;
    DCHECK(reset_succeeded);
  }

 private:
  const int signal_;
  const raw_ptr<struct sigaction> action_;
  const raw_ptr<struct sigaction> original_action_;
  const bool succeeded_;
};

}  // namespace

StackCopierSignal::StackCopierSignal(
    std::unique_ptr<ThreadDelegate> thread_delegate)
    : thread_delegate_(std::move(thread_delegate)),
      clock_(DefaultTickClock::GetInstance()) {}

StackCopierSignal::~StackCopierSignal() = default;

bool StackCopierSignal::CopyStack(StackBuffer* stack_buffer,
                                  uintptr_t* stack_top,
                                  TimeTicks* timestamp,
                                  RegisterContext* thread_context,
                                  Delegate* delegate) {
  AsyncSafeWaitableEvent wait_event;
  bool copied = false;
  const uint8_t* stack_copy_bottom = nullptr;
  const uintptr_t stack_base_address = thread_delegate_->GetStackBaseAddress();
  absl::optional<TimeTicks> maybe_timestamp;
  absl::optional<TimeTicks> maybe_timestamp_signaled;
  HandlerParams params = {stack_base_address,
                          &wait_event,
                          &copied,
                          thread_context,
                          stack_buffer,
                          &stack_copy_bottom,
                          &maybe_timestamp,
                          &maybe_timestamp_signaled,
                          delegate};
  TimeTicks signal_time;
  TimeTicks wait_start_time;
  TimeTicks wait_end_time;

  RecordEvent(CopyStackEvent::kStarted);
  {
    ScopedSetSignalHandlerParams scoped_handler_params(&params);

    // Set the signal handler for the thread to the stack copy function.
    struct sigaction action;
    struct sigaction original_action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = CopyStackSignalHandler;
    action.sa_flags = SA_RESTART | SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"),
                       "StackCopierSignal copy stack");
    // SIGURG is chosen here because we observe no crashes with this signal and
    // neither Chrome or the AOSP sets up a special handler for this signal.
    ScopedSigaction scoped_sigaction(SIGURG, &action, &original_action);
    if (!scoped_sigaction.succeeded()) {
      RecordEvent(CopyStackEvent::kSigactionFailed);
      return false;
    }

    signal_time = clock_->NowTicks();

    if (syscall(SYS_tgkill, getpid(), thread_delegate_->GetThreadId(),
                SIGURG) != 0) {
      RecordEvent(CopyStackEvent::kTgkillFailed);
      NOTREACHED();
      return false;
    }

    wait_start_time = clock_->NowTicks();
    bool finished_waiting = wait_event.Wait();
    TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"),
                     "StackCopierSignal copy stack");
    if (!finished_waiting) {
      RecordEvent(CopyStackEvent::kWaitFailed);
      NOTREACHED();
      return false;
    }
    wait_end_time = clock_->NowTicks();
    // Ideally, an accurate timestamp is captured while the sampled thread is
    // paused. In rare cases, this may fail, in which case we resort to
    // capturing an delayed timestamp here instead.
    if (maybe_timestamp.has_value())
      *timestamp = maybe_timestamp.value();
    else {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"),
                   "Fallback on TimeTicks::Now()");
      *timestamp = clock_->NowTicks();
    }
  }

  RecordEvent(CopyStackEvent::kSucceeded);
  // Record UMA stats about how long everything took. Since the profiler can't
  // profile the profiler, this is our only way to make sure the profiler isn't
  // taking excessively long.
  //
  // All times are recorded in microseconds since TimeTicks::IsHighResolution()
  // is always true on Posix systems, and we expect these to be very short
  // times.
  constexpr TimeDelta kMin = Microseconds(1);
  constexpr TimeDelta kMax = Microseconds(200 * 1000);
  constexpr int kBuckets = 100;

  UmaHistogramCustomMicrosecondsTimes(
      "UMA.StackProfiler.CopyStack.TotalCrossThreadTime",
      wait_end_time - signal_time, kMin, kMax, kBuckets);
  UmaHistogramCustomMicrosecondsTimes(
      "UMA.StackProfiler.CopyStack.ProfileThreadTotalWaitTime",
      wait_end_time - wait_start_time, kMin, kMax, kBuckets);
  if (maybe_timestamp) {
    UmaHistogramCustomMicrosecondsTimes(
        "UMA.StackProfiler.CopyStack.SignalToHandlerTime",
        *maybe_timestamp - signal_time, kMin, kMax, kBuckets);

    if (maybe_timestamp_signaled) {
      UmaHistogramCustomMicrosecondsTimes(
          "UMA.StackProfiler.CopyStack.HandlerRunTime",
          *maybe_timestamp_signaled - *maybe_timestamp, kMin, kMax, kBuckets);
    }
  }
  if (maybe_timestamp_signaled) {
    UmaHistogramCustomMicrosecondsTimes(
        "UMA.StackProfiler.CopyStack.EventSignalToWaitEndTime",
        wait_end_time - *maybe_timestamp_signaled, kMin, kMax, kBuckets);
  }

  const uintptr_t bottom = RegisterContextStackPointer(params.context);
  for (uintptr_t* reg :
       thread_delegate_->GetRegistersToRewrite(thread_context)) {
    *reg = StackCopierSignal::RewritePointerIfInOriginalStack(
        reinterpret_cast<uint8_t*>(bottom),
        reinterpret_cast<uintptr_t*>(stack_base_address), stack_copy_bottom,
        *reg);
  }

  *stack_top = reinterpret_cast<uintptr_t>(stack_copy_bottom) +
               (stack_base_address - bottom);

  return copied;
}

// static
void StackCopierSignal::RecordEvent(CopyStackEvent event) {
  UmaHistogramEnumeration("UMA.StackProfiler.CopyStack.Event", event);
}

}  // namespace base
