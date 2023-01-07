// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_HIERARCHICAL_TIMING_WHEEL_H_
#define BASE_TASK_SEQUENCE_MANAGER_HIERARCHICAL_TIMING_WHEEL_H_

#include <algorithm>
#include <array>
#include <numeric>
#include <vector>

#include "base/containers/intrusive_heap.h"
#include "base/task/sequence_manager/timing_wheel.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base::sequence_manager {

// A union of |TimingWheelHandle| and |HeapHandle|. At any given
// time it holds a value of one of its alternative types. It can only
// have either. This class is maintained by the hierarchical timing
// wheel as the object moves around within it. It can be used to subsequently
// remove the element.
class BASE_EXPORT HierarchicalTimingWheelHandle {
 public:
  enum : size_t { kInvalidIndex = std::numeric_limits<size_t>::max() };

  HierarchicalTimingWheelHandle();

  HierarchicalTimingWheelHandle(const HierarchicalTimingWheelHandle& other) =
      default;
  HierarchicalTimingWheelHandle(HierarchicalTimingWheelHandle&& other) noexcept;

  HierarchicalTimingWheelHandle& operator=(
      const HierarchicalTimingWheelHandle& other) = default;
  HierarchicalTimingWheelHandle& operator=(
      HierarchicalTimingWheelHandle&& other) noexcept;

  ~HierarchicalTimingWheelHandle();

  // TimingWheel contract
  internal::TimingWheelHandle GetTimingWheelHandle() const;
  void SetTimingWheelHandle(internal::TimingWheelHandle timing_wheel_handle);
  void ClearTimingWheelHandle();

  // IntrusiveHeap contract
  HeapHandle GetHeapHandle();
  void SetHeapHandle(HeapHandle handle);
  void ClearHeapHandle();

  size_t GetHierarchyIndex() const;
  void SetHierarchyIndex(size_t hierarchy_index);
  void ClearHierarchyIndex();

  // Gets a default constructed HierarchicalTimingWheelHandle.
  static HierarchicalTimingWheelHandle Invalid();

  bool IsValid() const;

 private:
  // The handle of the timing wheel in the hierarchical timing wheel where the
  // element is in.
  internal::TimingWheelHandle timing_wheel_handle_;

  // The handle of the heap in the hierarchical timing wheel where the element
  // is in.
  HeapHandle heap_handle_;

  // The index in the hierarchy of timing wheels and heaps, this handle belongs
  // to.
  size_t hierarchy_index_ = kInvalidIndex;
};

// The default HierarchicalTimingWheelHandleAccessor, which simply forwards
// calls to the underlying type. It assumes |T| provides
// HierarchicalTimingWheelHandle storage and will simply forward calls to
// equivalent member function.
template <typename T>
struct DefaultHierarchicalTimingWheelHandleAccessor {
  void SetTimingWheelHandle(T* element,
                            internal::TimingWheelHandle handle) const {
    HierarchicalTimingWheelHandle* htw_handle = element->handle();
    htw_handle->SetTimingWheelHandle(handle);
  }

  void ClearTimingWheelHandle(T* element) const {
    HierarchicalTimingWheelHandle* htw_handle = element->handle();
    htw_handle->ClearTimingWheelHandle();
  }

  HeapHandle GetHeapHandle(const T* element) const {
    HierarchicalTimingWheelHandle* htw_handle = element->handle();
    return htw_handle->GetHeapHandle();
  }

  void SetHeapHandle(T* element, HeapHandle handle) const {
    HierarchicalTimingWheelHandle* htw_handle = element->handle();
    htw_handle->SetHeapHandle(handle);
  }

  void ClearHeapHandle(T* element) const {
    HierarchicalTimingWheelHandle* htw_handle = element->handle();
    htw_handle->ClearHeapHandle();
  }

  void SetHierarchyIndex(T* element, size_t hierarchy_index) const {
    HierarchicalTimingWheelHandle* htw_handle = element->handle();
    htw_handle->SetHierarchyIndex(hierarchy_index);
  }

  void ClearHierarchyIndex(T* element) const {
    HierarchicalTimingWheelHandle* htw_handle = element->handle();
    htw_handle->ClearHierarchyIndex();
  }
};

// Gets the delayed run time of the |element|. Assumes the |element| has a
// public |delayed_run_time| member variable.
template <typename T>
struct GetDelayedRunTime {
  TimeTicks operator()(const T& element) { return element.delayed_run_time; }
};

// Used for ordering elements in the IntrusiveHeap in the hierarchy.
template <typename T>
struct Compare {
  bool operator()(const T& lhs, const T& rhs) const {
    return lhs.delayed_run_time > rhs.delayed_run_time;
  }
};

// This class is made to optimize the data structure IntrusiveHeap. Timers are
// implemented by scheduling the user task using TaskRunner::PostDelayedTask().
// The elements are then inserted in an InstrusiveHeap. It suffers from its time
// complexity of O(LgN) removal and insertion.
//
// This class is an implementation of timing wheel technique. It contains a
// hierarchy which is a sequence of timing wheels and heaps with different
// granularities used to span a greater range of intervals. There are two heaps
// in the hierarchy, each placed on the two ends of the sequence of timing
// wheels.
//
// |T| is a typename for the intervals that are inserted in this class.
// |TotalWheels| is the number of timing wheels to be constructed in the
// hierarchy. |WheelSize| is the number of buckets in each of the timing wheel.
// |SmallestBucketDeltaInMicroseconds| corresponds to the time delta per
// bucket for the smallest timing wheel in the hierarchy. The time delta per
// bucket for the following timing wheels are WheelSize *
// |time_delta_per_bucket| of previous timing wheel.
// |HierarchicalTimingWheelHandleAccessor| is the type of the object which under
// the hood manages the HierarchicalTimingWheelHandle. |GetDelayedRunTime| is a
// function which returns the time when the element is due at.
//
// Example:
// Note: The number enclosing in the curly brackets "{}" are the data
// structure's hierarchy number. It exists to understand their order in the
// hierarchy.
//
// TotalWheels = 4
// WheelSize = 100
// SmallestBucketDeltaInMicroseconds = 500 microseconds
//
// Heap{0} - all elements with delays below 500 microseconds
//
// Wheel{1} - each bucket of 500microseconds = 0.5ms.
//          bucket0 contains 0 <= delta < 0.5ms
//          bucket1 contains 0.5 <= delta < 1ms
//          Wheel1 contains 0.5 <= delta < 50ms
//
// Wheel{2} - each bucket of 50ms.
//          bucket0 contains 0 <= delta < 50ms
//          bucket1 contains 50ms <= delta < 100ms
//          Wheel1 contains 50ms <= delta < 5s
//
// Wheel{3} - each bucket of 5s.
//          bucket0 contains 0 <= delta < 5s
//          bucket1 contains 5s <= delta < 10s
//          Wheel1 contains 5s <= delta < 500s
//
// Wheel{4} - each bucket of 500s.
//          bucket0 contains 0 <= delta < 500s
//          bucket1 contains 500s <= delta < 1000s
//          Wheel1 contains 500s <= delta < 50000s
//
// Heap{5} - all elements with delay above or equals to 500microseconds *
// (100^4)
//
// This class takes O(1) time to insert and cancel timers. However, if a element
// has a very small or big timer interval, then it's placed in a heap. This
// means, the removal and insertion won't be as efficient. However, the
// expectation is that such elements with very small or very big intervals would
// be very few.

template <typename T,
          size_t TotalWheels,
          size_t WheelSize,
          size_t SmallestBucketDeltaInMicroseconds,
          typename HierarchicalTimingWheelHandleAccessor =
              DefaultHierarchicalTimingWheelHandleAccessor<T>,
          typename GetDelayedRunTime = GetDelayedRunTime<T>,
          typename Compare = Compare<T>>
class HierarchicalTimingWheel {
 public:
  // Construct a HierarchicalTimingWheel instance where |last_wakeup|
  // corresponds to the last time it was updated.
  explicit HierarchicalTimingWheel(
      TimeTicks last_wakeup,
      const HierarchicalTimingWheelHandleAccessor&
          hierarchical_timing_wheel_handle_accessor =
              HierarchicalTimingWheelHandleAccessor(),
      const GetDelayedRunTime& get_delayed_run_time = GetDelayedRunTime(),
      const Compare compare = Compare())
      : small_delay_heap_(compare, hierarchical_timing_wheel_handle_accessor),
        large_delay_heap_(compare, hierarchical_timing_wheel_handle_accessor),
        last_wakeup_(last_wakeup),
        hierarchical_timing_wheel_handle_accessor_(
            hierarchical_timing_wheel_handle_accessor),
        get_delayed_run_time_(get_delayed_run_time) {}

  HierarchicalTimingWheel(HierarchicalTimingWheel&&) = delete;
  HierarchicalTimingWheel& operator=(HierarchicalTimingWheel&&) = delete;

  HierarchicalTimingWheel(const HierarchicalTimingWheel&) = delete;
  HierarchicalTimingWheel& operator=(const HierarchicalTimingWheel&) = delete;

  ~HierarchicalTimingWheel() = default;

  size_t Size() {
    return small_delay_heap_.size() + large_delay_heap_.size() +
           std::accumulate(std::begin(wheels_), std::end(wheels_), 0,
                           [](size_t i, auto& wheel) {
                             return wheel.total_elements() + i;
                           });
  }

  // Inserts the |element| based on its delayed run time into one of the
  // |wheels_|.
  typename std::vector<T>::const_iterator Insert(T element) {
    DCHECK(get_delayed_run_time_(element) > last_wakeup_);

    const TimeDelta delay = get_delayed_run_time_(element) - last_wakeup_;
    const size_t hierarchy_index = FindHierarchyIndex(delay);

    if (IsHeap(hierarchy_index)) {
      auto& heap = GetHeapForHierarchyIndex(hierarchy_index);
      hierarchical_timing_wheel_handle_accessor_.SetHierarchyIndex(
          &element, hierarchy_index);
      auto it = heap.insert(std::move(element));
      return it;
    } else {
      auto& wheel = GetTimingWheelForHierarchyIndex(hierarchy_index);
      hierarchical_timing_wheel_handle_accessor_.SetHierarchyIndex(
          &element, hierarchy_index);
      auto it = wheel.Insert(std::move(element), delay);
      return it;
    }
  }

  // Updates the hierarchy and reassigns the elements that need to be
  // placed in a different timing wheel or heap to reflect their respective
  // delay. It returns the elements that are expired.
  std::vector<T> Update(TimeTicks now) {
    DCHECK(now >= last_wakeup_);
    std::vector<T> expired_elements;

    // Check for expired elements in the small delay heap.
    while (!small_delay_heap_.empty() &&
           get_delayed_run_time_(small_delay_heap_.top()) <= now) {
      T element = small_delay_heap_.take_top();

      // Clear the hierarchy index since the |element| will be returned.
      hierarchical_timing_wheel_handle_accessor_.ClearHierarchyIndex(&element);

      expired_elements.push_back(std::move(element));
    }

    // Look into the timing wheels for elements which have either expired or
    // need to be moved down the hierarchy.
    std::vector<T> elements;
    const TimeDelta time_delta = now - last_wakeup_;
    const size_t timing_wheels_delay_upperbound =
        SmallestBucketDeltaInMicroseconds * Pow(WheelSize, TotalWheels);
    const TimeTicks timing_wheels_maximum_delayed_run_time =
        now + Milliseconds(timing_wheels_delay_upperbound);
    last_wakeup_ = now;

    for (size_t wheel_index = 0; wheel_index < TotalWheels; wheel_index++) {
      wheels_[wheel_index].AdvanceTimeAndRemoveExpiredElements(time_delta,
                                                               elements);
    }

    // Keep on removing the top elements from the |large_delay_heap_| which
    // could be either moved down the hierarchy or are expired.
    while (!large_delay_heap_.empty() &&
           get_delayed_run_time_(large_delay_heap_.top()) <
               timing_wheels_maximum_delayed_run_time) {
      elements.push_back(std::move(large_delay_heap_.take_top()));
    }

    // Re-insert elements which haven't expired yet.
    for (auto& element : elements) {
      if (now >= get_delayed_run_time_(element)) {
        hierarchical_timing_wheel_handle_accessor_.ClearHierarchyIndex(
            &element);
        expired_elements.emplace_back(std::move(element));
      } else {
        // Doesn't clear hierarchy index since the element will have their
        // hierarchy index overwritten when re-inserted.
        Insert(std::move(element));
      }
    }

    return expired_elements;
  }

  // Removes the |element|. This is considered as the element getting cancelled
  // and will never be run.
  void Remove(HierarchicalTimingWheelHandle& handle) {
    DCHECK(handle.IsValid());
    if (handle.GetTimingWheelHandle().IsValid()) {
      auto& wheel = GetTimingWheelForHierarchyIndex(handle.GetHierarchyIndex());
      wheel.Remove(handle.GetTimingWheelHandle());
    } else {
      auto& heap = GetHeapForHierarchyIndex(handle.GetHierarchyIndex());
      heap.erase(handle.GetHeapHandle());
    }
  }

  // Returns the earliest due element in all of the hierarchy. This method
  // should only called when the HierarchicalTimingWheel is not empty.
  typename std::vector<T>::const_reference Top() {
    DCHECK_NE(Size(), 0u);

    // Check for smallest elements heap first.
    if (!small_delay_heap_.empty()) {
      return small_delay_heap_.top();
    }

    // Iterate from smallest to biggest element wheel.
    for (size_t i = 0; i < TotalWheels; i++) {
      if (wheels_[i].total_elements() != 0) {
        return wheels_[i].Top();
      }
    }

    // The result must be in the biggest elements heap.
    return large_delay_heap_.top();
  }

 private:
  bool IsHeap(size_t hierarchy_index) {
    return hierarchy_index == 0 or hierarchy_index == TotalWheels + 1;
  }

  auto& GetHeapForHierarchyIndex(size_t hierarchy_index) {
    DCHECK(hierarchy_index == 0 || hierarchy_index == TotalWheels + 1);
    return hierarchy_index == 0 ? small_delay_heap_ : large_delay_heap_;
  }

  auto& GetTimingWheelForHierarchyIndex(size_t hierarchy_index) {
    DCHECK(hierarchy_index > 0);
    DCHECK(hierarchy_index < TotalWheels + 1);
    return wheels_[hierarchy_index - 1];
  }

  // Calculates the hierarchy index at which a element with |delay| should be
  // appended in.
  size_t FindHierarchyIndex(TimeDelta delay) {
    DCHECK(!delay.is_zero());

    if (delay < Microseconds(SmallestBucketDeltaInMicroseconds))
      return 0;

    for (size_t i = 0; i < TotalWheels; i++) {
      if (delay < (wheels_[i].time_delta_per_bucket() * WheelSize)) {
        return i + 1;
      }
    }

    // Return the index of the heap placed at the end of the hierarchy.
    return TotalWheels + 1;
  }

  // Computes |a| to the power of |b| at compile time. This is used to compute
  // the parameter for |TimingWheel| when generating |wheels_| at compile
  // time.
  constexpr static std::size_t Pow(size_t a, size_t b) {
    size_t res = 1;
    for (size_t i = 0; i < b; i++) {
      res *= a;
    }
    return res;
  }

  using Wheel =
      typename internal::TimingWheel<T,
                                     WheelSize,
                                     HierarchicalTimingWheelHandleAccessor,
                                     GetDelayedRunTime>;

  // Generates |wheels_| at compile time.
  template <size_t... I>
  static std::array<Wheel, TotalWheels> MakeWheels(std::index_sequence<I...>) {
    return {(Wheel(Microseconds(SmallestBucketDeltaInMicroseconds *
                                Pow(WheelSize, I))))...};
  }

  // The timing wheels where the elements are added according to their delay.
  std::array<Wheel, TotalWheels> wheels_ =
      MakeWheels(std::make_index_sequence<TotalWheels>{});

  // There are two heaps enclosing the sequence of timing wheels. The first one
  // contains elements whose delay is too small to enter a timing wheel. The
  // second one contains elements whose delay is too big to enter a timing
  // wheel.
  IntrusiveHeap<T, Compare, HierarchicalTimingWheelHandleAccessor>
      small_delay_heap_;
  IntrusiveHeap<T, Compare, HierarchicalTimingWheelHandleAccessor>
      large_delay_heap_;

  // The last time when the timing wheels were updated.
  TimeTicks last_wakeup_;

  HierarchicalTimingWheelHandleAccessor
      hierarchical_timing_wheel_handle_accessor_;

  GetDelayedRunTime get_delayed_run_time_;
};

}  // namespace base::sequence_manager

#endif
