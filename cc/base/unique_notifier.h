// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_UNIQUE_NOTIFIER_H_
#define CC_BASE_UNIQUE_NOTIFIER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/base_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace cc {

// Callers must ensure that they only schedule the notifier on the same thread
// that the provided |task_runner| runs on.
class CC_BASE_EXPORT UniqueNotifier {
 public:
  // Configure this notifier to issue the |closure| notification when scheduled.
  UniqueNotifier(base::SequencedTaskRunner* task_runner,
                 base::RepeatingClosure closure);
  UniqueNotifier(const UniqueNotifier&) = delete;

  // Destroying the notifier will ensure that no further notifications will
  // happen from this class.
  ~UniqueNotifier();

  UniqueNotifier& operator=(const UniqueNotifier&) = delete;

  // Schedule a notification to be run. If another notification is already
  // pending, then only one notification will take place.
  void Schedule();

  // Cancel a pending notification, if one was scheduled.
  void Cancel();

 private:
  void Notify();

  // TODO(dcheng): How come this doesn't need to hold a ref to the task runner?
  base::SequencedTaskRunner* const task_runner_;
  const base::RepeatingClosure closure_;

  // Lock should be held before modifying |notification_pending_|.
  base::Lock lock_;
  bool notification_pending_;

  base::WeakPtrFactory<UniqueNotifier> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_BASE_UNIQUE_NOTIFIER_H_
