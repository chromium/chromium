// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <iterator>

#include "base/memory/ref_counted.h"
#include "base/pending_task.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/heap_profiler.h"
#include "base/trace_event/heap_profiler_allocation_context.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

const char kThreadName[] = "TestThread";

// Asserts that the fixed-size array |expected_backtrace| matches the backtrace
// in |AllocationContextTracker::GetContextSnapshot|.
template <size_t N>
void AssertBacktraceEquals(const StackFrame(&expected_backtrace)[N]) {
  AllocationContext ctx;
  ASSERT_TRUE(AllocationContextTracker::GetInstanceForCurrentThread()
                  ->GetContextSnapshot(&ctx));

  auto* actual = std::begin(ctx.backtrace.frames);
  auto* actual_bottom = actual + ctx.backtrace.frame_count;
  auto expected = std::begin(expected_backtrace);
  auto expected_bottom = std::end(expected_backtrace);

  // Note that this requires the pointers to be equal, this is not doing a deep
  // string comparison.
  for (; actual != actual_bottom && expected != expected_bottom;
       actual++, expected++)
    ASSERT_EQ(*expected, *actual);

  // Ensure that the height of the stacks is the same.
  ASSERT_EQ(actual, actual_bottom);
  ASSERT_EQ(expected, expected_bottom);
}

void AssertBacktraceContainsOnlyThreadName() {
  StackFrame t = StackFrame::FromThreadName(kThreadName);
  AllocationContext ctx;
  ASSERT_TRUE(AllocationContextTracker::GetInstanceForCurrentThread()
                  ->GetContextSnapshot(&ctx));

  ASSERT_GE(ctx.backtrace.frame_count, 1u);
  ASSERT_EQ(t, ctx.backtrace.frames[0]);
}

class AllocationContextTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    AllocationContextTracker::SetCaptureMode(
        AllocationContextTracker::CaptureMode::NATIVE_STACK);
    AllocationContextTracker::SetCurrentThreadName(kThreadName);
  }

  void TearDown() override {
    AllocationContextTracker::SetCaptureMode(
        AllocationContextTracker::CaptureMode::DISABLED);
  }
};

TEST_F(AllocationContextTrackerTest, StackContainsThreadName) {
  AssertBacktraceContainsOnlyThreadName();
}

TEST_F(AllocationContextTrackerTest, IgnoreAllocationTest) {
  HEAP_PROFILER_SCOPED_IGNORE;
  AllocationContext ctx;
  ASSERT_FALSE(AllocationContextTracker::GetInstanceForCurrentThread()
                   ->GetContextSnapshot(&ctx));
}

}  // namespace trace_event
}  // namespace base
