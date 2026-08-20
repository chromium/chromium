// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_TAGS_H_
#define BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_TAGS_H_

#include <array>
#include <cstdint>
#include <string_view>

#include "base/base_export.h"
#include "base/check_op.h"

namespace base {

// A tag that can be used to apply unique identifiers to a lock.
//
// Creation:
// Instances must be created as `constexpr` objects with a unique name for each
// different lock type, as the tag name is used as part of the suffix in UMA
// histogram names.
//
// The `LockMetricTag` should be prefixed with the underlying lock type, for
// example "BaseLock.CustomLockTag," or just the lock type, e.g. "BaseLock".
//
// It is recommended to create a static function getter for the `LockMetricTag`,
// and use the same getter throughout the subsystem:
//   const base::LockMetricTag& GetMyFeatureLockMetricTag() {
//     static constexpr base::LockMetricTag tag("LockType.MyFeatureLock");
//     return tag;
//   }
//
// Usage:
// `LockMetricTag` instances are passed by value within `LockMetricTagList` to
// identify lock types and categories when recording lock acquisition metrics.
//
// If creating a new `LockMetricTag` in a subsystem, add a histogram
// in the appropriate histograms.xml file to capture the lock acquisition times.

class BASE_EXPORT LockMetricTag {
 public:
  constexpr LockMetricTag(const LockMetricTag&) = default;
  constexpr LockMetricTag& operator=(const LockMetricTag&) = default;
  ~LockMetricTag() = default;

  consteval explicit LockMetricTag(std::string_view name)
      : name_(name), hash_(HashName(name)) {}

  constexpr std::string_view name() const { return name_; }
  constexpr uint64_t hash() const { return hash_; }

 protected:
  // Default constructor is protected to prevent clients from creating empty
  // `LockMetricTag` instances, while exposing it to `LockMetricTagList` and
  // unit tests for internal storage initialization.
  constexpr LockMetricTag() = default;
  friend class LockMetricTagList;
  friend class LockMetricsRecorderTest;

 private:
  // Generates a precomputed 64-bit hash key for tag names using FNV-1a hashing.
  // Standard Chromium hashing helpers such as `base::FastHash` and
  // `base::PersistentHash` are non-constexpr and cannot be evaluated in
  // consteval constructors required for `LockMetricTag` instances.
  static constexpr uint64_t HashName(std::string_view name) {
    constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
    constexpr uint64_t kFnvPrime = 1099511628211ULL;

    uint64_t hash = kFnvOffsetBasis;
    for (char c : name) {
      hash ^= static_cast<uint8_t>(c);
      hash *= kFnvPrime;
    }
    return hash;
  }

  std::string_view name_;
  uint64_t hash_ = 0;
};

// An array-backed container of `LockMetricTag` values passed to
// `ScopedLockAcquisitionTimer` to be recorded along with the lock acquisition
// time.
//
// Contains up to `kMaxTags` (3) `LockMetricTag` values representing lock
// categories/types.
//
// Example usage:
//     base::LockMetricTagList tags(GetBaseLockMetricTag(),
//                                  GetThreadPoolLockMetricTag(),
//                                  GetTaskRunnerLockMetricTag());
//     base::LockMetricsRecorder::ScopedLockAcquisitionTimer timer(tags);
//
class BASE_EXPORT LockMetricTagList {
 public:
  static constexpr size_t kMaxTags = 3;

  // Constructs a `LockMetricTagList` with 1 required tag.
  constexpr explicit LockMetricTagList(const LockMetricTag& tag1)
      : tags_{tag1}, size_(1) {}

  // Constructs a `LockMetricTagList` with 2 tags.
  constexpr LockMetricTagList(const LockMetricTag& tag1,
                              const LockMetricTag& tag2)
      : tags_{tag1, tag2}, size_(2) {}

  // Constructs a `LockMetricTagList` with 3 tags.
  constexpr LockMetricTagList(const LockMetricTag& tag1,
                              const LockMetricTag& tag2,
                              const LockMetricTag& tag3)
      : tags_{tag1, tag2, tag3}, size_(3) {}

  constexpr size_t size() const { return size_; }

 protected:
  // Default constructor is protected to prevent clients from creating empty
  // `LockMetricTagList` instances, while exposing it for internal storage
  // initialization in containers like `base::RingBuffer`.
  constexpr LockMetricTagList() = default;

 private:
  friend class LockMetricsRecorder;
  friend class LockMetricsRecorderTest;
  friend class TaggedMetricLock;
  friend class TaggedMetricLockTest;

  // Returns the `LockMetricTag` at the given index, or `nullptr` if out of
  // bounds.
  constexpr const LockMetricTag* operator[](size_t index) const {
    DCHECK_LT(index, kMaxTags);
    if (index >= size_) {
      return nullptr;
    }
    return &tags_[index];
  }

  std::array<LockMetricTag, kMaxTags> tags_{};
  uint8_t size_ = 0;
};

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_LOCK_METRICS_RECORDER_TAGS_H_
