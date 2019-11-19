// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/page_almost_idle_decorator.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class PageAlmostIdleDecoratorTest : public GraphTestHarness {
 protected:
  PageAlmostIdleDecoratorTest() = default;
  ~PageAlmostIdleDecoratorTest() override = default;

  void SetUp() override {
    paid_ = new PageAlmostIdleDecorator();
    graph()->PassToGraph(base::WrapUnique(paid_));
  }

  void TestPageAlmostIdleTransitions(bool timeout);

  bool IsIdling(const PageNodeImpl* page_node) const {
    return PageAlmostIdleDecorator::IsIdling(page_node);
  }

  PageAlmostIdleDecorator* paid_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageAlmostIdleDecoratorTest);
};

void PageAlmostIdleDecoratorTest::TestPageAlmostIdleTransitions(bool timeout) {
  static const base::TimeDelta kLoadedAndIdlingTimeout =
      PageAlmostIdleDecorator::kLoadedAndIdlingTimeout;
  static const base::TimeDelta kWaitingForIdleTimeout =
      PageAlmostIdleDecorator::kWaitingForIdleTimeout;

  // Aliasing these here makes this unittest much more legible.
  using Data = PageAlmostIdleDecorator::Data;
  using LIS = Data::LoadIdleState;

  AdvanceClock(base::TimeDelta::FromSeconds(1));

  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* frame_node = mock_graph.frame.get();
  auto* page_node = mock_graph.page.get();
  auto* proc_node = mock_graph.process.get();
  auto* page_data = Data::GetOrCreateForTesting(page_node);

  // Initially the page should be in a loading not started state.
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->load_idle_state_);
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());

  // The state should not transition when a not loading state is explicitly
  // set.
  page_node->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->load_idle_state_);
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());

  // The state should transition to loading when loading starts.
  page_node->SetIsLoading(true);
  EXPECT_EQ(LIS::kLoading, page_data->load_idle_state_);
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());

  // Mark the page as idling. It should transition from kLoading directly
  // to kLoadedAndIdling after this.
  frame_node->SetNetworkAlmostIdle();
  proc_node->SetMainThreadTaskLoadIsLow(true);
  page_node->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state_);
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());

  // Indicate loading is happening again. This should be ignored.
  page_node->SetIsLoading(true);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state_);
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());
  page_node->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state_);
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());

  // Go back to not idling. We should transition back to kLoadedNotIdling, and
  // a timer should still be running.
  frame_node->OnNavigationCommitted(GURL(), false);
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data->load_idle_state_);
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());

  base::TimeTicks start = base::TimeTicks::Now();
  if (timeout) {
    // Let the timeout run down. The final state transition should occur.
    task_env().FastForwardUntilNoTasksRemain();
    base::TimeTicks end = base::TimeTicks::Now();
    base::TimeDelta elapsed = end - start;
    EXPECT_LE(kLoadedAndIdlingTimeout, elapsed);
    EXPECT_LE(kWaitingForIdleTimeout, elapsed);
    EXPECT_FALSE(Data::GetForTesting(page_node));
  } else {
    // Go back to idling.
    frame_node->SetNetworkAlmostIdle();
    EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state_);
    EXPECT_TRUE(page_data->idling_timer_.IsRunning());

    // Let the idle timer evaluate. The final state transition should occur.
    task_env().FastForwardUntilNoTasksRemain();
    base::TimeTicks end = base::TimeTicks::Now();
    base::TimeDelta elapsed = end - start;
    EXPECT_LE(kLoadedAndIdlingTimeout, elapsed);
    EXPECT_GT(kWaitingForIdleTimeout, elapsed);
    EXPECT_FALSE(Data::GetForTesting(page_node));
  }

  // Firing other signals should not change the state at all.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(Data::GetForTesting(page_node));
  frame_node->OnNavigationCommitted(GURL(), false);
  EXPECT_FALSE(Data::GetForTesting(page_node));

  // Post a navigation. The state should reset.
  page_node->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(), 1,
                                            GURL("https://www.example.org"));
  page_data = Data::GetForTesting(page_node);
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->load_idle_state_);
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());
}

TEST_F(PageAlmostIdleDecoratorTest, TestTransitionsNoTimeout) {
  TestPageAlmostIdleTransitions(false);
}

TEST_F(PageAlmostIdleDecoratorTest, TestTransitionsWithTimeout) {
  TestPageAlmostIdleTransitions(true);
}

TEST_F(PageAlmostIdleDecoratorTest, IsLoading) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // The loading property hasn't yet been set. Then IsLoading should return
  // false as the default value.
  EXPECT_FALSE(page_node->is_loading());

  // Once the loading property has been set it should return that value.
  page_node->SetIsLoading(false);
  EXPECT_FALSE(page_node->is_loading());
  page_node->SetIsLoading(true);
  EXPECT_TRUE(page_node->is_loading());
  page_node->SetIsLoading(false);
  EXPECT_FALSE(page_node->is_loading());
}

TEST_F(PageAlmostIdleDecoratorTest, IsIdling) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* frame_node = mock_graph.frame.get();
  auto* page_node = mock_graph.page.get();
  auto* proc_node = mock_graph.process.get();

  // Neither of the idling properties are set, so IsIdling should return false.
  EXPECT_FALSE(IsIdling(page_node));

  // Should still return false after main thread task is low.
  proc_node->SetMainThreadTaskLoadIsLow(true);
  EXPECT_FALSE(IsIdling(page_node));

  // Should return true when network is idle.
  frame_node->SetNetworkAlmostIdle();
  EXPECT_TRUE(IsIdling(page_node));

  // Should toggle with main thread task low.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(IsIdling(page_node));
  proc_node->SetMainThreadTaskLoadIsLow(true);
  EXPECT_TRUE(IsIdling(page_node));

  // Should return false when network is no longer idle.
  frame_node->OnNavigationCommitted(GURL(), false);
  EXPECT_FALSE(IsIdling(page_node));

  // And should stay false if main thread task also goes low again.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(IsIdling(page_node));
}

}  // namespace performance_manager
