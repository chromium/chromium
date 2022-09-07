// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <vector>

#include "base/task/sequence_manager/timing_wheel.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::sequence_manager::internal {

namespace {

class Element {
 public:
  Element(TimeTicks delayed_run_time)
      : delayed_run_time(delayed_run_time),
        handle_(std::make_unique<TimingWheelHandle>()) {}

  // Implementation of TimingWheel contract.
  void SetTimingWheelHandle(TimingWheelHandle handle) {
    DCHECK(handle.IsValid());
    if (handle_)
      *handle_ = handle;
  }
  void ClearTimingWheelHandle() {
    if (handle_)
      handle_->Reset();
  }

  TimingWheelHandle* handle() const { return handle_.get(); }

  TimeTicks delayed_run_time;

 private:
  std::unique_ptr<TimingWheelHandle> handle_;
};

// The default TimingWheelHandleAccessor, which simply forwards calls to the
// underlying type.
template <typename T>
struct TimingWheelHandleAccessor {
  void SetTimingWheelHandle(T* element, TimingWheelHandle handle) const {
    element->SetTimingWheelHandle(handle);
  }
  void ClearTimingWheelHandle(T* element) const {
    element->ClearTimingWheelHandle();
  }
};

// Gets the delayed run time of the |element|.
struct GetDelayedRunTime {
  template <typename T>
  TimeTicks operator()(const T& element) {
    return element.delayed_run_time;
  }
};

// The total number of buckets that we use for the TimingWheel in all the unit
// tests.
const size_t kWheelSize = 100;

// The time period used for each bucket in the TimingWheel in all the unit
// tests.
const TimeDelta kBucketTimeDelta = Microseconds(500);

// The TimingWheel has an index which points to the last updated bucket. On
// initialization, the index points at bucket 0. The immediate
// insertable bucket is always the bucket after the index. Hence, index 1 is
// considered the first bucket. Suppose, the timing wheel is updated and the
// index now points at Index X. This would mean that while the index is
// pointing at Index X, no element can be inserted at Index X.
const size_t kFirstBucketIndex = 1;

// The index of the last bucket in the TimingWheel
const size_t kLastBucketIndex = kWheelSize - 1;

}  // namespace

// Tests the construction of the object.
TEST(TimingWheelTest, Initialize) {
  TimingWheel<Element, kWheelSize, TimingWheelHandleAccessor<Element>,
              GetDelayedRunTime>
      timing_wheel(kBucketTimeDelta);

  EXPECT_EQ(timing_wheel.time_delta_per_bucket(), kBucketTimeDelta);
  EXPECT_EQ(timing_wheel.total_elements(), 0u);
}

// Tests whether an element can be added to the first bucket.
TEST(TimingWheelTest, InsertElement) {
  TimingWheel<Element, kWheelSize, TimingWheelHandleAccessor<Element>,
              GetDelayedRunTime>
      timing_wheel(kBucketTimeDelta);
  const TimeTicks baseline = TimeTicks::Now();

  // Pick a time delta that ensures the element is added to the first bucket.
  const TimeDelta kTestDelay = kFirstBucketIndex * kBucketTimeDelta;

  const auto it = timing_wheel.Insert({baseline + kTestDelay}, kTestDelay);
  const TimingWheelHandle* handle = it->handle();

  EXPECT_EQ(handle->element_index(), 0u);
  EXPECT_EQ(handle->bucket_index(), kFirstBucketIndex);
  EXPECT_EQ(timing_wheel.total_elements(), 1u);
}

// Tests advancing time by a delta equal to the wheel's complete time delta.
TEST(TimingWheelTest, AdvancingFullWheel) {
  TimingWheel<Element, kWheelSize, TimingWheelHandleAccessor<Element>,
              GetDelayedRunTime>
      timing_wheel(kBucketTimeDelta);
  const TimeTicks baseline = TimeTicks::Now();
  const TimeDelta kTimePassed = kBucketTimeDelta * kLastBucketIndex;

  // Pick time delta that ensures the elements are added to the last bucket.
  const TimeDelta kLastBucketMinDelay = kLastBucketIndex * kBucketTimeDelta;

  timing_wheel.Insert({baseline + kLastBucketMinDelay}, kLastBucketMinDelay);

  std::vector<Element> expired_elements;
  timing_wheel.AdvanceTimeAndRemoveExpiredElements(kTimePassed,
                                                   expired_elements);

  EXPECT_EQ(expired_elements.size(), 1u);
  EXPECT_EQ(timing_wheel.total_elements(), 0u);
}

// Tests advancing time by a delta greater than the wheel's complete time delta.
TEST(TimingWheelTest, AdvancingMoreThanFullWheel) {
  TimingWheel<Element, kWheelSize, TimingWheelHandleAccessor<Element>,
              GetDelayedRunTime>
      timing_wheel(kBucketTimeDelta);
  std::vector<Element> expired_elements;
  const TimeDelta kTimePassed =
      kBucketTimeDelta * kWheelSize + kBucketTimeDelta;

  timing_wheel.AdvanceTimeAndRemoveExpiredElements(kTimePassed,
                                                   expired_elements);

  EXPECT_EQ(timing_wheel.total_elements(), 0u);
  EXPECT_EQ(expired_elements.size(), 0u);
}

// Tests whether multiple elements can be added to different buckets.
TEST(TimingWheelTest, InsertMultipleElementsInDifferentBuckets) {
  TimingWheel<Element, kWheelSize, TimingWheelHandleAccessor<Element>,
              GetDelayedRunTime>
      timing_wheel(kBucketTimeDelta);
  const TimeTicks baseline = TimeTicks::Now();
  const size_t kBucketIndex1 = 50;
  const size_t kBucketIndex2 = 75;
  const TimeDelta kTestDelay1 = kBucketIndex1 * kBucketTimeDelta;
  const TimeDelta kTestDelay2 = kBucketIndex2 * kBucketTimeDelta;

  // Insert five elements that are due before |kTestDelay1|.
  timing_wheel.Insert({baseline + kTestDelay1}, kTestDelay1);
  timing_wheel.Insert({baseline + kTestDelay1 - Milliseconds(2)}, kTestDelay1);
  timing_wheel.Insert({baseline + kTestDelay1 - Milliseconds(5)}, kTestDelay1);
  timing_wheel.Insert({baseline + kTestDelay1 - Milliseconds(10)}, kTestDelay1);
  timing_wheel.Insert({baseline + kTestDelay1 - Milliseconds(10)}, kTestDelay1);

  // Insert two elements that are due after |kTestDelay1|.
  timing_wheel.Insert({baseline + kTestDelay2 - Milliseconds(4)}, kTestDelay2);
  timing_wheel.Insert({baseline + kTestDelay2}, kTestDelay2);

  std::vector<Element> expired_elements;

  // Advances time by |kTestDelay1| so that all the elements except the last
  // two can be picked up.
  timing_wheel.AdvanceTimeAndRemoveExpiredElements(kTestDelay1,
                                                   expired_elements);
  EXPECT_EQ(expired_elements.size(), 5u);
  EXPECT_EQ(timing_wheel.total_elements(), 2u);

  // Need to advance time by at least (kTestDelay2 - kTestDelay1) to pick the
  // last elements.
  timing_wheel.AdvanceTimeAndRemoveExpiredElements(kTestDelay2 - kTestDelay1,
                                                   expired_elements);
  EXPECT_EQ(expired_elements.size(), 7u);
  EXPECT_EQ(timing_wheel.total_elements(), 0u);
}

// Tests whether multiple elements that expire at the same time can be added
// to the same buckets.
TEST(TimingWheelTest, InsertMultipleSimilarElementsInSameBucket) {
  TimingWheel<Element, kWheelSize, TimingWheelHandleAccessor<Element>,
              GetDelayedRunTime>
      timing_wheel(kBucketTimeDelta);
  const TimeTicks baseline = TimeTicks::Now();
  const size_t kBucketIndex = 15;
  const TimeDelta kTestDelay = kBucketIndex * kBucketTimeDelta;

  timing_wheel.Insert({baseline + kTestDelay}, kTestDelay);
  timing_wheel.Insert({baseline + kTestDelay}, kTestDelay);

  std::vector<Element> expired_elements;
  timing_wheel.AdvanceTimeAndRemoveExpiredElements(kTestDelay,
                                                   expired_elements);
  EXPECT_EQ(expired_elements.size(), 2u);
  EXPECT_EQ(timing_wheel.total_elements(), 0u);
}

// Tests the circular nature of the timing wheel, and ensure that when the
// current index advances, the buckets before it can now be used for elements
// whose deadline is greater than kBucketTimeDelta * kWheelSize.
TEST(TimingWheelTest, InsertToLastBucketButNotLastIndex) {
  TimingWheel<Element, kWheelSize, TimingWheelHandleAccessor<Element>,
              GetDelayedRunTime>
      timing_wheel(kBucketTimeDelta);
  const TimeTicks baseline = TimeTicks::Now();
  const TimeDelta kTimePassed = 2 * kBucketTimeDelta;
  const TimeDelta kTestDelay = kLastBucketIndex * kBucketTimeDelta;
  std::vector<Element> expired_elements;

  // First advance time to change the current index of the wheel.
  timing_wheel.AdvanceTimeAndRemoveExpiredElements(kTimePassed,
                                                   expired_elements);
  timing_wheel.Insert({baseline + kTestDelay + kTimePassed}, kTestDelay);

  // Advance time to pick up the element.
  timing_wheel.AdvanceTimeAndRemoveExpiredElements(kTestDelay,
                                                   expired_elements);
  EXPECT_EQ(expired_elements.size(), 1u);
}

// Tests removal of an element using its handle.
TEST(TimingWheelTest, RemoveElement) {
  TimingWheel<Element, kWheelSize, TimingWheelHandleAccessor<Element>,
              GetDelayedRunTime>
      timing_wheel(kBucketTimeDelta);
  const TimeTicks baseline = TimeTicks::Now();
  const TimeDelta kLastBucketMinDelay = kLastBucketIndex * kBucketTimeDelta;
  const TimeDelta kLastBucketMaxDelay =
      (kLastBucketIndex + 1) * kBucketTimeDelta - Microseconds(1u);
  const TimeDelta kTimePassed = kBucketTimeDelta * kLastBucketIndex;

  timing_wheel.Insert({baseline + kLastBucketMinDelay}, kLastBucketMinDelay);
  const TimingWheelHandle* handle =
      timing_wheel
          .Insert({baseline + kLastBucketMaxDelay}, kLastBucketMaxDelay)
          ->handle();
  timing_wheel.Remove(*handle);
  EXPECT_EQ(timing_wheel.total_elements(), 1u);

  std::vector<Element> expired_elements;
  timing_wheel.AdvanceTimeAndRemoveExpiredElements(kTimePassed,
                                                   expired_elements);

  EXPECT_EQ(expired_elements.size(), 1u);
  EXPECT_EQ(timing_wheel.total_elements(), 0u);
}

// Tests whether we get the earliest due element from two elements in different
// buckets.
TEST(TimingWheelTest, TopElementfromDifferentElements) {
  TimingWheel<Element, kWheelSize, TimingWheelHandleAccessor<Element>,
              GetDelayedRunTime>
      timing_wheel(kBucketTimeDelta);
  const TimeTicks baseline = TimeTicks::Now();
  const TimeDelta kTestDelay1 = kBucketTimeDelta;
  const TimeDelta kTestDelay2 = kBucketTimeDelta * 2;

  Element element1(baseline + kTestDelay1);
  Element element2(baseline + kTestDelay2);
  const TimingWheelHandle* handle = element1.handle();

  timing_wheel.Insert(std::move(element1), kTestDelay1);
  timing_wheel.Insert(std::move(element2), kTestDelay2);

  EXPECT_EQ(handle, timing_wheel.Top().handle());
  EXPECT_EQ(timing_wheel.total_elements(), 2u);
}

// Tests whether we get the earliest due element when two elements are in the
// same bucket.
TEST(TimingWheelTest, TopElementFromSimilarElements) {
  TimingWheel<Element, kWheelSize, TimingWheelHandleAccessor<Element>,
              GetDelayedRunTime>
      timing_wheel(kBucketTimeDelta);
  const TimeTicks baseline = TimeTicks::Now();
  const TimeDelta kTestDelay1 = kBucketTimeDelta;
  const TimeDelta kTestDelay2 = kBucketTimeDelta + Milliseconds(1);
  Element element1(baseline + kTestDelay1);
  Element element2(baseline + kTestDelay2);
  const TimingWheelHandle* handle = element1.handle();

  timing_wheel.Insert(std::move(element1), kTestDelay1);
  timing_wheel.Insert(std::move(element2), kTestDelay2);

  EXPECT_EQ(handle, timing_wheel.Top().handle());
  EXPECT_EQ(timing_wheel.total_elements(), 2u);
}

}  // namespace base::sequence_manager::internal
