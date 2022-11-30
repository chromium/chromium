// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/hierarchical_timing_wheel.h"

#include <math.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::sequence_manager {

namespace {

// Custom comparator for testing.
template <typename T>
struct CustomCompare {
  bool operator()(const T& lhs, const T& rhs) const {
    return std::tie(lhs.delayed_run_time, lhs.name) >
           std::tie(rhs.delayed_run_time, rhs.name);
  }
};

class Task {
 public:
  enum : size_t { kInvalidIndex = std::numeric_limits<size_t>::max() };

  explicit Task(TimeTicks delayed_run_time, std::string name = std::string())
      : delayed_run_time(delayed_run_time),
        name(name),
        handle_(std::make_unique<HierarchicalTimingWheelHandle>()) {}

  HierarchicalTimingWheelHandle* handle() const { return handle_.get(); }

  TimeTicks delayed_run_time;

  // Used as a second comparator key to test the custom comparator
  // functionality.
  std::string name;

 private:
  std::unique_ptr<HierarchicalTimingWheelHandle> handle_;
};

}  // namespace

// Tests the construction of the object.
TEST(HierarchicalTimingWheelTest, SimpleTest) {
  const TimeTicks baseline = TimeTicks::Now();
  HierarchicalTimingWheel<Task, 4, 100, 500> hierarchical_timing_wheel{
      baseline};

  EXPECT_EQ(hierarchical_timing_wheel.Size(), 0u);

  auto* handle =
      hierarchical_timing_wheel.Insert(Task{baseline + Microseconds(100)})
          ->handle();
  EXPECT_EQ(hierarchical_timing_wheel.Size(), 1u);

  hierarchical_timing_wheel.Remove(*handle);
  EXPECT_EQ(hierarchical_timing_wheel.Size(), 0u);
}

// Tests whether an element can be added in all the places in the hierarchy.
TEST(HierarchicalTimingWheelTest, InsertAllDistinctElements) {
  const size_t kHierarchyCount = 6;
  const TimeTicks baseline = TimeTicks::Now();
  HierarchicalTimingWheel<Task, 4, 100, 500> hierarchical_timing_wheel{
      baseline};

  // Create time delta to insert a task in each of the hierarchy. The element's
  // index represents the hierarchy index in which it will be inserted. The
  // delays are chosen as per the example given in the class's header file.
  const TimeDelta kDelay[kHierarchyCount] = {
      Microseconds(100), Microseconds(500), Milliseconds(50),
      Seconds(5),        Seconds(500),      Seconds(50000)};

  HierarchicalTimingWheelHandle* handles[kHierarchyCount];
  for (size_t i = 0; i < kHierarchyCount; i++) {
    handles[i] =
        hierarchical_timing_wheel.Insert(Task{baseline + kDelay[i]})->handle();
  }

  for (size_t i = 0; i < kHierarchyCount; i++) {
    EXPECT_EQ(handles[i]->GetHierarchyIndex(), i);

    const bool is_heap_handle = i == 0 || i == kHierarchyCount - 1;
    EXPECT_EQ(handles[i]->GetHeapHandle().IsValid(), is_heap_handle);
    EXPECT_EQ(handles[i]->GetTimingWheelHandle().IsValid(), !is_heap_handle);
  }
}

// Tests whether multiple elements can be added in the same place in the
// hierarchy.
TEST(HierarchicalTimingWheelTest, InsertSimilarElements) {
  const size_t kTotalElements = 3;
  const size_t kExpectedHierarchyIndex = 1;
  const TimeTicks baseline = TimeTicks::Now();
  HierarchicalTimingWheel<Task, 4, 100, 500> hierarchical_timing_wheel{
      baseline};

  // Create time delta to insert a task in the second hierarchy.
  const TimeDelta kDelay[kTotalElements] = {Microseconds(500), Milliseconds(21),
                                            Milliseconds(49)};

  HierarchicalTimingWheelHandle* handles[kTotalElements];
  for (size_t i = 0; i < kTotalElements; i++) {
    handles[i] =
        hierarchical_timing_wheel.Insert(Task{baseline + kDelay[i]})->handle();
  }

  for (auto* handle : handles) {
    EXPECT_EQ(handle->GetHierarchyIndex(), kExpectedHierarchyIndex);
    EXPECT_EQ(handle->GetHeapHandle().IsValid(), false);
    EXPECT_EQ(handle->GetTimingWheelHandle().IsValid(), true);
  }
}

// Tests whether the hierarchy can be updated and cascading take place from one
// hierarchy to another for an element.
TEST(HierarchicalTimingWheelTest, UpdateOneElement) {
  const size_t kHierarchyCount = 6;
  TimeTicks baseline = TimeTicks::Now();
  HierarchicalTimingWheel<Task, 4, 100, 500> hierarchical_timing_wheel{
      baseline};

  // An array of deltas which cascades an element from the biggest
  // hierarchy to the smallest sequentially, and then finally expiring the
  // element.
  const TimeDelta kTimeDelta[] = {Seconds(50000) - Seconds(500),
                                  Seconds(500) - Seconds(5),
                                  Seconds(5) - Milliseconds(50),
                                  Milliseconds(50) - Microseconds(500),
                                  Microseconds(500) - Microseconds(100),
                                  Microseconds(100)};

  // Create time delta to insert a task at the end of the hierarchy.
  const TimeTicks delayed_run_time = baseline + Seconds(50000);

  HierarchicalTimingWheelHandle* handle =
      hierarchical_timing_wheel.Insert(Task{delayed_run_time})->handle();

  std::vector<Task> expired_tasks;
  for (size_t i = 0; i < kHierarchyCount; i++) {
    const size_t expected_hierarchy_index = kHierarchyCount - i - 1;
    EXPECT_EQ(handle->GetHierarchyIndex(), expected_hierarchy_index);
    baseline += kTimeDelta[i];
    expired_tasks = hierarchical_timing_wheel.Update(baseline);

    // An element will be returned as expired on the last Update.
    const bool expired = i == kHierarchyCount - 1;

    // We expect one element to be returned, once.
    EXPECT_EQ(expired_tasks.size() == 0, !expired);
    EXPECT_EQ(expired_tasks.size() == 1, expired);
  }
}

// Tests whether the hierarchy can be updated and cascading take place of
// multiple existing elements in the hierarchy.
TEST(HierarchicalTimingWheelTest, UpdateMultipleElements) {
  const size_t kHierarchyCount = 6;
  TimeTicks baseline = TimeTicks::Now();
  HierarchicalTimingWheel<Task, 4, 100, 500> hierarchical_timing_wheel{
      baseline};

  // Create time delta to insert a task in each of the hierarchy. The element's
  // index represents the hierarchy index in which it will be inserted.
  const TimeDelta kDelay[kHierarchyCount] = {
      Microseconds(100), Microseconds(500), Milliseconds(50),
      Seconds(5),        Seconds(500),      Seconds(50000)};

  HierarchicalTimingWheelHandle* handles[kHierarchyCount];
  for (size_t i = 0; i < kHierarchyCount; i++) {
    handles[i] =
        hierarchical_timing_wheel.Insert(Task{baseline + kDelay[i]})->handle();
  }

  // This update expires all the inserted elements except the last two.
  baseline += Seconds(499);
  std::vector<Task> expired_tasks = hierarchical_timing_wheel.Update(baseline);
  EXPECT_EQ(expired_tasks.size(), 4u);
  EXPECT_EQ(handles[kHierarchyCount - 2]->GetHierarchyIndex(), 2u);
  EXPECT_EQ(handles[kHierarchyCount - 1]->GetHierarchyIndex(), 4u);

  // Expires the last two elements by updating much more than latest delay
  // element.
  baseline += Seconds(100000);
  expired_tasks = hierarchical_timing_wheel.Update(baseline);
  EXPECT_EQ(expired_tasks.size(), 2u);
}

// Tests whether an element can be removed from each hierarchy.
TEST(HierarchicalTimingWheelTest, RemoveElements) {
  const size_t kHierarchyCount = 6;
  const TimeTicks baseline = TimeTicks::Now();
  HierarchicalTimingWheel<Task, 4, 100, 500> hierarchical_timing_wheel{
      baseline};

  // Create time delta to insert a task in each of the hierarchy. The element's
  // index represents the hierarchy index in which it will be inserted.
  const TimeDelta kDelay[kHierarchyCount] = {
      Microseconds(100), Microseconds(500), Milliseconds(50),
      Seconds(5),        Seconds(500),      Seconds(50000)};

  HierarchicalTimingWheelHandle* handles[kHierarchyCount];
  for (size_t i = 0; i < kHierarchyCount; i++) {
    handles[i] =
        hierarchical_timing_wheel.Insert(Task{baseline + kDelay[i]})->handle();
  }

  for (auto* handle : handles) {
    hierarchical_timing_wheel.Remove(*handle);
  }

  // The biggest delay was Seconds(50000). Hence, this would remove any leftover
  // element, which there aren't supposed to be.
  std::vector<Task> expired_tasks =
      hierarchical_timing_wheel.Update(baseline + Seconds(50000));
  EXPECT_EQ(expired_tasks.empty(), true);
}

// Tests whether the top element of the hierarchy returned is correct when all
// distinct elements exist in the hierarchy.
TEST(HierarchicalTimingWheelTest, TopDifferentElements) {
  const size_t kHierarchyCount = 6;
  const TimeTicks baseline = TimeTicks::Now();
  HierarchicalTimingWheel<Task, 4, 100, 500> hierarchical_timing_wheel{
      baseline};

  // Create time delta to insert a task in each of the hierarchy. The element's
  // index represents the hierarchy index in which it will be inserted.
  const TimeDelta kDelay[kHierarchyCount] = {
      Microseconds(100), Microseconds(500), Milliseconds(50),
      Seconds(5),        Seconds(500),      Seconds(50000)};

  HierarchicalTimingWheelHandle* handles[kHierarchyCount];
  for (size_t i = 0; i < kHierarchyCount; i++) {
    handles[i] =
        hierarchical_timing_wheel.Insert(Task{baseline + kDelay[i]})->handle();
    const Task& task = hierarchical_timing_wheel.Top();
    EXPECT_EQ(task.delayed_run_time, baseline + kDelay[0]);
  }

  for (size_t i = 0; i < kHierarchyCount; i++) {
    const Task& task = hierarchical_timing_wheel.Top();
    EXPECT_EQ(task.delayed_run_time, baseline + kDelay[i]);
    hierarchical_timing_wheel.Remove(*handles[i]);
  }
}

// Tests whether the top element of the hierarchy returned is correct when
// multiple similar elements are in the hierarchy.
TEST(HierarchicalTimingWheelTest, TopSimilarElements) {
  const size_t kTotalElements = 3;
  const TimeTicks baseline = TimeTicks::Now();
  HierarchicalTimingWheel<Task, 4, 100, 500> hierarchical_timing_wheel{
      baseline};

  // Create time delta to insert a task in the first hierarchy.
  const TimeDelta kDelay[kTotalElements] = {
      Microseconds(100), Microseconds(200), Microseconds(300)};

  HierarchicalTimingWheelHandle* handles[kTotalElements];
  for (size_t i = 0; i < kTotalElements; i++) {
    handles[i] =
        hierarchical_timing_wheel.Insert(Task{baseline + kDelay[i]})->handle();
    const Task& task = hierarchical_timing_wheel.Top();
    EXPECT_EQ(task.delayed_run_time, baseline + kDelay[0]);
  }

  for (size_t i = 0; i < kTotalElements; i++) {
    const Task& task = hierarchical_timing_wheel.Top();
    EXPECT_EQ(task.delayed_run_time, baseline + kDelay[i]);
    hierarchical_timing_wheel.Remove(*handles[i]);
  }
}

// Tests whether the |Compare| functor is correctly used.
TEST(HierarchicalTimingWheelTest, CustomComparator) {
  const std::string expectedTopTaskName = "a";
  const TimeTicks baseline = TimeTicks::Now();
  HierarchicalTimingWheel<Task, 4, 100, 500,
                          DefaultHierarchicalTimingWheelHandleAccessor<Task>,
                          GetDelayedRunTime<Task>, CustomCompare<Task>>
      hierarchical_timing_wheel{baseline};

  // Create time delta to insert a task in the first hierarchy.
  const TimeDelta kDelay = Microseconds(100);

  // Inserts two elements in the same bucket.
  hierarchical_timing_wheel.Insert(Task{baseline + kDelay, "z"});
  hierarchical_timing_wheel.Insert(Task{baseline + kDelay, "a"});
  const Task& task = hierarchical_timing_wheel.Top();

  // The custom comparator orders by the name's lexicographical order,
  // since both the elements have the same delayed run time.
  EXPECT_EQ(task.name, expectedTopTaskName);
}

}  // namespace base::sequence_manager
