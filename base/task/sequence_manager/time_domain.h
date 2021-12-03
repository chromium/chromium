// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TIME_DOMAIN_H_
#define BASE_TASK_SEQUENCE_MANAGER_TIME_DOMAIN_H_

#include "base/check.h"
#include "base/task/sequence_manager/lazy_now.h"
#include "base/task/sequence_manager/tasks.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace sequence_manager {

class SequenceManager;

namespace internal {
class SequenceManagerImpl;
}  // namespace internal

// TimeDomain allows subclasses to enable clock overriding
// (e.g. auto-advancing virtual time, throttled clock, etc).
class BASE_EXPORT TimeDomain : public TickClock {
 public:
  TimeDomain(const TimeDomain&) = delete;
  TimeDomain& operator=(const TimeDomain&) = delete;
  ~TimeDomain() override = default;

  // Returns the desired ready time based on the predetermined `next_wake_up`,
  // is_null() if ready immediately, or is_max() to ignore the wake-up. This is
  // typically aligned with `next_wake_up.time` but virtual time domains may
  // elect otherwise. Can be called from main thread only.
  // TODO(857101): Pass `lazy_now` by reference.
  virtual TimeTicks GetNextDelayedTaskTime(WakeUp next_wake_up,
                                           LazyNow* lazy_now) const = 0;

  // Invoked when the thread reaches idle. Gives an opportunity to a virtual
  // time domain impl to fast-forward time and return true to indicate that
  // there's more work to run. If RunLoop::QuitWhenIdle has been called then
  // `quit_when_idle_requested` will be true.
  virtual bool MaybeFastForwardToWakeUp(absl::optional<WakeUp> next_wake_up,
                                        bool quit_when_idle_requested) = 0;

  // Debug info.
  Value AsValue() const;

 protected:
  TimeDomain() = default;

  virtual const char* GetName() const = 0;

  // Tells SequenceManager that internal policy might have changed to
  // re-evaluate GetNextDelayedTaskTime()/MaybeFastForwardToWakeUp().
  void NotifyPolicyChanged();

  // Called when the TimeDomain is assigned to a SequenceManagerImpl.
  // `sequence_manager` is expected to be valid for the duration of TimeDomain's
  // existence. TODO(scheduler-dev): Pass SequenceManager in the constructor.
  void OnAssignedToSequenceManager(
      internal::SequenceManagerImpl* sequence_manager);

 private:
  friend class internal::SequenceManagerImpl;

  internal::SequenceManagerImpl* sequence_manager_ = nullptr;  // Not owned.
};

}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TIME_DOMAIN_H_
