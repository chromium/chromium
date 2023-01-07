// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_DELAYED_UNIQUE_NOTIFIER_H_
#define CC_BASE_DELAYED_UNIQUE_NOTIFIER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "cc/base/base_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace cc {

class CC_BASE_EXPORT DelayedUniqueNotifier {
 public:
  // Configure this notifier to issue the |closure| notification in |delay| time
  // from Schedule() call.
  DelayedUniqueNotifier(base::SequencedTaskRunner* task_runner,
                        base::RepeatingClosure closure,
                        const base::TimeDelta& delay);
  DelayedUniqueNotifier(const DelayedUniqueNotifier&) = delete;

  // Destroying the notifier will ensure that no further notifications will
  // happen from this class.
  virtual ~DelayedUniqueNotifier();

  DelayedUniqueNotifier& operator=(const DelayedUniqueNotifier&) = delete;

  // Schedule a notification to be run. If another notification is already
  // pending, then it will happen in (at least) given delay from now. That is,
  // if delay is 16ms and a notification has been scheduled 10ms ago (ie, it
  // should trigger in no less than 6ms), then calling schedule will ensure that
  // the only notification that arrives will happen in (at least) 16ms from now.
  void Schedule();

  // Cancel any previously scheduled runs.
  void Cancel();

  // Cancel previously scheduled runs and prevent any new runs from starting.
  // After calling this the DelayedUniqueNotifier will have no outstanding
  // WeakPtrs.
  void Shutdown();

  // Returns true if a notification is currently scheduled to run.
  bool HasPendingNotification() const;

 protected:
  // Virtual for testing.
  virtual base::TimeTicks Now() const;

 private:
  void NotifyIfTime();

  THREAD_CHECKER(thread_checker_);

  const raw_ptr<base::SequencedTaskRunner> task_runner_;
  const base::RepeatingClosure closure_;
  const base::TimeDelta delay_;

  base::TimeTicks next_notification_time_;
  bool notification_pending_;

  base::WeakPtrFactory<DelayedUniqueNotifier> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_BASE_DELAYED_UNIQUE_NOTIFIER_H_
