// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/quick_insert_search_debouncer.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class QuickInsertSearchDebouncerTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(QuickInsertSearchDebouncerTest,
       RequestSearchDoesNotTriggerSearchUntilDelay) {
  base::test::TestFuture<void> future;
  PickerSearchDebouncer debouncer(base::Milliseconds(100));

  debouncer.RequestSearch(future.GetCallback());
  task_environment().FastForwardBy(base::Milliseconds(99));

  EXPECT_FALSE(future.IsReady());
}

TEST_F(QuickInsertSearchDebouncerTest, RequestSearchTriggersSearchAfterDelay) {
  base::test::TestFuture<void> future;
  PickerSearchDebouncer debouncer(base::Milliseconds(100));

  debouncer.RequestSearch(future.GetCallback());
  task_environment().FastForwardBy(base::Milliseconds(100));

  EXPECT_TRUE(future.IsReady());
}

TEST_F(QuickInsertSearchDebouncerTest, NewRequestSearchCancelsPreviousRequest) {
  base::test::TestFuture<void> future;
  PickerSearchDebouncer debouncer(base::Milliseconds(100));

  debouncer.RequestSearch(future.GetCallback());
  task_environment().FastForwardBy(base::Milliseconds(99));
  debouncer.RequestSearch(future.GetCallback());
  task_environment().FastForwardBy(base::Milliseconds(99));

  EXPECT_FALSE(future.IsReady());
}

TEST_F(QuickInsertSearchDebouncerTest,
       NewRequestSearchTriggersSearchAfterDelay) {
  base::test::TestFuture<void> future;
  PickerSearchDebouncer debouncer(base::Milliseconds(100));

  debouncer.RequestSearch(future.GetCallback());
  task_environment().FastForwardBy(base::Milliseconds(99));
  debouncer.RequestSearch(future.GetCallback());
  task_environment().FastForwardBy(base::Milliseconds(100));

  EXPECT_TRUE(future.IsReady());
}

}  // namespace
}  // namespace ash
