// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_CONDITION_TRACKER_H_
#define CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_CONDITION_TRACKER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/origin.h"

// A ref-counted tracker for a permission condition (such as active origin pages
// or backgrounded tabs). When the last reference to a tracker is released, it
// invokes a closure with a prescribed delay, unless a new reference is acquired
// before that.
class OneTimePermissionsConditionTracker
    : public base::RefCounted<OneTimePermissionsConditionTracker> {
 public:
  explicit OneTimePermissionsConditionTracker(base::OnceClosure on_destruction);

  // Factory class that manages per-origin ConditionTracker instances.
  class Factory {
   public:
    Factory(base::RepeatingCallback<void(const url::Origin&)>
                on_all_references_released,
            base::TimeDelta delay);
    ~Factory();

    scoped_refptr<OneTimePermissionsConditionTracker> New(
        const url::Origin& origin);

    void SetTaskRunnerForTesting(
        scoped_refptr<base::SequencedTaskRunner> task_runner);

   private:
    void OnTrackerDestroyed(const url::Origin& origin);

    void EraseTimer(const url::Origin& origin);

    base::RepeatingCallback<void(const url::Origin&)>
        on_all_references_released_;
    base::TimeDelta delay_;
    scoped_refptr<base::SequencedTaskRunner> task_runner_;

    // Stores raw pointers to the per-origin
    // OneTimePermissionsConditionTracker's. The
    // OneTimePermissionsConditionTracker destructor takes care of deleting the
    // map entry in order to prevent dangling pointers.
    absl::flat_hash_map<url::Origin, OneTimePermissionsConditionTracker*> map_;

    absl::flat_hash_map<url::Origin, std::unique_ptr<base::OneShotTimer>>
        timers_map_;
    base::WeakPtrFactory<Factory> weak_factory_{this};
  };

 private:
  friend class base::RefCounted<OneTimePermissionsConditionTracker>;
  ~OneTimePermissionsConditionTracker();

  base::OnceClosure on_destruction_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_CONDITION_TRACKER_H_
