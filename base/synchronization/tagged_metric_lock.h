// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_TAGGED_METRIC_LOCK_H_
#define BASE_SYNCHRONIZATION_TAGGED_METRIC_LOCK_H_

#include "base/base_export.h"
#include "base/check.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/lock_metrics_recorder_tags.h"
#include "base/synchronization/lock_subtle.h"
#include "base/thread_annotations.h"
#include "base/types/optional_ref.h"

namespace base {

// A lock annotated with metric tags for tracking lock contention.
//
// Usage:
// `TaggedMetricLock` is used to measure and categorize lock acquisition
// times under UMA metrics.
//
// Must be acquired using `base::TaggedAutoLock`. Attempting to pass a
// `TaggedMetricLock` to standard `base::AutoLock` will result in a compile-time
// type error.
//
// Tag Selection:
// A lock can be tagged with both a `core_tag` and a `custom_tag`, and it will
// record lock acquisition samples for both tags.
// - `core_tag`: Identifies the primary subsystem or lock category (for example,
// "TaskQueue"). Must not be null.
// - `custom_tag`: An optional tag providing fine-grained categorization for
// specific lock instances within a broader subsystem (for example,
// "TaskQueuePostTask").
//
// Example Usage:
//   class MojoChannel {
//    public:
//     MojoChannel()
//         : lock_(GetMojoTag(), GetMojoChannelLockTag()) {}
//
//     void WriteMessage() {
//       base::TaggedAutoLock auto_lock(lock_);
//       // Critical section...
//     }
//
//    private:
//     base::TaggedMetricLock lock_;
//   };
class LOCKABLE BASE_EXPORT TaggedMetricLock {
 public:
  explicit TaggedMetricLock(const LockMetricTag& core_tag)
      : tag_list_(Lock::GetBaseLockMetricTag(), core_tag) {}

  TaggedMetricLock(const LockMetricTag& core_tag,
                   const LockMetricTag& custom_tag)
      : tag_list_(Lock::GetBaseLockMetricTag(), core_tag, custom_tag) {}

  TaggedMetricLock(const TaggedMetricLock&) = delete;
  TaggedMetricLock& operator=(const TaggedMetricLock&) = delete;

  bool Try(subtle::LockTracking tracking = subtle::LockTracking::kDisabled)
      EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return lock_.Try(tracking);
  }

  void Acquire(subtle::LockTracking tracking = subtle::LockTracking::kDisabled)
      EXCLUSIVE_LOCK_FUNCTION() {
    lock_.Acquire(tag_list_, tracking);
  }

  void Release() UNLOCK_FUNCTION() { lock_.Release(); }
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {
    lock_.AssertAcquired();
  }
  void AssertNotHeld() const { lock_.AssertNotHeld(); }

  const LockMetricTag& core_tag() const { return *tag_list_[1]; }
  optional_ref<const LockMetricTag> custom_tag() const { return tag_list_[2]; }

 private:
  Lock lock_;
  LockMetricTagList tag_list_;
};

// A helper that acquires the given `TaggedMetricLock` while in scope.
// `TaggedAutoLock` can only be used to acquire a `TaggedMetricLock`.
using TaggedAutoLock = internal::BasicAutoLock<TaggedMetricLock>;

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_TAGGED_METRIC_LOCK_H_
