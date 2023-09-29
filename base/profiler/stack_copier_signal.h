// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_STACK_COPIER_SIGNAL_H_
#define BASE_PROFILER_STACK_COPIER_SIGNAL_H_

#include <memory>

#include "base/base_export.h"
#include "base/memory/raw_ptr.h"
#include "base/profiler/stack_copier.h"
#include "base/time/tick_clock.h"

namespace base {

class ThreadDelegate;

// Supports stack copying on platforms where a signal must be delivered to the
// profiled thread and the stack is copied from the signal handler.
class BASE_EXPORT StackCopierSignal : public StackCopier {
 public:
  StackCopierSignal(std::unique_ptr<ThreadDelegate> thread_delegate);
  ~StackCopierSignal() override;

  // StackCopier:
  bool CopyStack(StackBuffer* stack_buffer,
                 uintptr_t* stack_top,
                 TimeTicks* timestamp,
                 RegisterContext* thread_context,
                 Delegate* delegate) override;

  using StackCopier::CopyStackContentsAndRewritePointers;

  void set_clock_for_testing(const TickClock* clock) { clock_ = clock; }

  // Events that happen during CopyStack; used for the
  // UMA.StackProfiler.CopyStack.Event histogram. Public for use by tests.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CopyStackEvent {
    kStarted = 0,
    kSucceeded = 1,
    kSigactionFailed = 2,
    kTgkillFailed = 3,
    kWaitFailed = 4,
    kMaxValue = kWaitFailed
  };

 private:
  // Records an event during a run of CopyStack to the
  // UMA.StackProfiler.CopyStack.Event histogram.
  static void RecordEvent(CopyStackEvent event);

  std::unique_ptr<ThreadDelegate> thread_delegate_;
  // Clock used for time inside CopyStack. NOT used for getting the time in the
  // signal handler, which always uses the real system tick clock.
  raw_ptr<const TickClock> clock_;
};

}  // namespace base

#endif  // BASE_PROFILER_STACK_COPIER_SIGNAL_H_
