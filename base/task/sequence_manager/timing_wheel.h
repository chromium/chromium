// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TIMING_WHEEL_H_
#define BASE_TASK_SEQUENCE_MANAGER_TIMING_WHEEL_H_

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <vector>

#include "base/notreached.h"
#include "base/time/time.h"

namespace base::sequence_manager::internal {

// Intended as a wrapper around a |bucket_index_| and |element_element_index_|
// in the vector storage backing a TimingWheel. A TimingWheelHandle is
// associated with each element in a TimingWheel, and is maintained by
// the timing wheel as the object moves around within it. It can be used to
// subsequently remove the element, or update it in place.
class BASE_EXPORT TimingWheelHandle {
 public:
  enum : size_t { kInvalidIndex = std::numeric_limits<size_t>::max() };

  constexpr TimingWheelHandle() = default;

  constexpr TimingWheelHandle(const TimingWheelHandle& other) = default;
  TimingWheelHandle(TimingWheelHandle&& other) noexcept;

  TimingWheelHandle& operator=(const TimingWheelHandle& other) = default;
  TimingWheelHandle& operator=(TimingWheelHandle&& other) noexcept;

  ~TimingWheelHandle() = default;

  // Returns an invalid TimingWheelHandle.
  static TimingWheelHandle Invalid();

  // Resets this handle back to an invalid state.
  void Reset();

  bool IsValid() const;

  // Accessors.
  size_t bucket_index() const;
  size_t element_index() const;

 private:
  template <typename T,
            size_t WheelSize,
            typename TimingWheelHandleAccessor,
            typename GetDelayedRunTime>
  friend class TimingWheel;

  // Only TimingWheels can create valid TimingWheelHandles.
  explicit TimingWheelHandle(size_t bucket_index, size_t element_index);

  // The index of the bucket in the timing wheel where the element is in.
  size_t bucket_index_ = kInvalidIndex;

  // The index of the element in the bucket where the element is in.
  size_t element_index_ = kInvalidIndex;
};

// This class implements a container that acts as timer queue where element are
// associated with a delay. It provides efficient retrieval of earliest
// elements. It also provides constant time element removal. To facilitate this,
// each element has associated with it a TimingWheelHandle (an opaque wrapper
// around the index at which the element is stored), which is maintained by the
// wheel as elements move within it. Only elements whose delay is between
// |time_delta_per_bucket_| and WheelSize*|time_delta_per_bucket_| can be
// inserted in a TimingWheel. |T| is a typename for element. |WheelSize| is the
// number of buckets this TimingWheel has. |TimingWheelHandleAccessor| is the
// type of the object which under the hood manages the TimingWheelHandle.
// |GetDelayedRunTime| is a functor which returns the time when the element is
// due at.
template <typename T,
          size_t WheelSize,
          typename TimingWheelHandleAccessor,
          typename GetDelayedRunTime>
class TimingWheel {
 public:
  // Constructs a TimingWheel instance where each bucket corresponds to a
  // TimeDelta of |time_delta_per_bucket_|.
  explicit TimingWheel(
      TimeDelta time_delta_per_bucket,
      const GetDelayedRunTime& get_delayed_run_time = GetDelayedRunTime())
      : time_delta_per_bucket_(time_delta_per_bucket),
        last_updated_bucket_index_(0),
        time_passed_(Microseconds(0)),
        get_delayed_run_time_(get_delayed_run_time) {}

  TimingWheel(TimingWheel&&) = delete;
  TimingWheel& operator=(TimingWheel&&) = delete;

  TimingWheel(const TimingWheel&) = delete;
  TimingWheel& operator=(const TimingWheel&) = delete;

  ~TimingWheel() = default;

  // Inserts the |element| into the bucket based on its delay. This is the delay
  // relative to a baseline implied by the last call to
  // |AdvanceTimeAndRemoveExpiredElements| method.
  typename std::vector<T>::const_iterator Insert(T element,
                                                 const TimeDelta delay) {
    DCHECK_GE(delay, time_delta_per_bucket_);
    DCHECK_LT(delay, time_delta_per_bucket_ * WheelSize);

    const size_t bucket_index = CalculateBucketIndex(delay);
    auto& bucket = buckets_[bucket_index];
    bucket.push_back(std::move(element));
    const size_t element_index = bucket.size() - 1;

    // Sets the handle for the element.
    timing_wheel_handle_accessor.SetTimingWheelHandle(
        &buckets_[bucket_index][element_index],
        TimingWheelHandle(bucket_index, element_index));

    total_elements_ += 1;
    return bucket.cend() - 1;
  }

  // Removes the element which holds this |handle|.
  void Remove(TimingWheelHandle handle) {
    DCHECK(handle.IsValid());

    const size_t bucket_index = handle.bucket_index();
    const size_t element_index = handle.element_index();
    DCHECK(IsBounded(bucket_index, element_index));

    auto& bucket = buckets_[bucket_index];
    const size_t last_index_of_bucket = bucket.size() - 1;

    // Swaps the element's position with the last element for removal. The
    // swapped element is assigned with an updated handle.
    if (element_index != last_index_of_bucket) {
      timing_wheel_handle_accessor.SetTimingWheelHandle(
          &bucket[last_index_of_bucket],
          TimingWheelHandle(bucket_index, element_index));
      std::swap(bucket[element_index], bucket[last_index_of_bucket]);
    }

    // The handle of the last element doesn't need to be cleared since the
    // element is destroyed right after.
    bucket.pop_back();
    total_elements_ -= 1;
  }

  // Updates the internal state to reflect the latest wakeup and returns the
  // expired elements through an out-parameter so that the caller can keep using
  // the same vector when advancing multiple TimingWheels.
  void AdvanceTimeAndRemoveExpiredElements(const TimeDelta time_delta,
                                           std::vector<T>& expired_elements) {
    const size_t nb_buckets_passed =
        (time_passed_ + time_delta) / time_delta_per_bucket_ + 1;
    const size_t new_bucket_index =
        (last_updated_bucket_index_ + nb_buckets_passed) % WheelSize;
    const TimeDelta new_time_passed =
        (time_passed_ + time_delta) % time_delta_per_bucket_;

    // Ensures each bucket is iterated over at most once if |nb_buckets_passed|
    // is bigger than the total number of buckets.
    const size_t nb_buckets_to_traverse =
        std::min(nb_buckets_passed, WheelSize);
    for (size_t i = 0; i < nb_buckets_to_traverse; i++) {
      last_updated_bucket_index_ = (last_updated_bucket_index_ + 1) % WheelSize;
      ExtractElementsFromBucket(last_updated_bucket_index_, expired_elements);
    }

    last_updated_bucket_index_ = new_bucket_index;
    time_passed_ = new_time_passed;
  }

  // Returns the earliest due element.
  typename std::vector<T>::const_reference Top() {
    DCHECK(total_elements_ != 0);
    for (size_t i = 0; i < WheelSize; i++) {
      const size_t bucket_index = (i + last_updated_bucket_index_) % WheelSize;
      auto& bucket = buckets_[bucket_index];
      if (bucket.size() == 0) {
        continue;
      }

      auto it = std::min_element(
          bucket.begin(), bucket.end(), [this](const T& a, const T& b) {
            return get_delayed_run_time_(a) > get_delayed_run_time_(b);
          });
      return *it;
    }

    NOTREACHED();
    return buckets_[0].back();
  }

  TimeDelta time_delta_per_bucket() { return time_delta_per_bucket_; }
  size_t total_elements() { return total_elements_; }

 private:
  // Checks if the |bucket_index| and |element_index| is bounded.
  bool IsBounded(const size_t bucket_index, const size_t element_index) const {
    return bucket_index < WheelSize &&
           element_index < buckets_[bucket_index].size();
  }

  // Calculates the index at which a task with |delay| should be inserted in.
  size_t CalculateBucketIndex(const TimeDelta delay) const {
    const size_t nb_buckets_passed =
        (delay + time_passed_) / time_delta_per_bucket_;
    const size_t bucket_index = last_updated_bucket_index_ + nb_buckets_passed;

    // |bucket_index| should not be more than |WheelSize|.
    return bucket_index % WheelSize;
  }

  // Removes the elements from the index in |buckets_| and returns the
  // expired elements through an out-parameter.
  void ExtractElementsFromBucket(const size_t bucket_index,
                                 std::vector<T>& expired_elements) {
    std::vector<T>& bucket = buckets_[bucket_index];
    expired_elements.reserve(expired_elements.size() + bucket.size());

    for (auto& element : bucket) {
      timing_wheel_handle_accessor.ClearTimingWheelHandle(&element);
      expired_elements.push_back(std::move(element));
    }

    total_elements_ -= bucket.size();
    bucket.clear();
  }

  TimingWheelHandleAccessor timing_wheel_handle_accessor;

  // The time period each bucket contains.
  const TimeDelta time_delta_per_bucket_;

  // The buckets where the elements are added according to their delay.
  std::array<std::vector<T>, WheelSize> buckets_;

  // The index of the bucket that was last updated. This helps in Inserting and
  // expired elements.
  size_t last_updated_bucket_index_;

  // The time passed unaccounted for after updating
  // |last_updated_bucket_index_|. This will be aggregated with the
  // |time_passed_| at the next wakeup.
  TimeDelta time_passed_;

  // The number of elements in |buckets_|.
  size_t total_elements_ = 0;

  // The functor to get the delayed run time of elements.
  GetDelayedRunTime get_delayed_run_time_;
};

}  // namespace base::sequence_manager::internal

#endif
