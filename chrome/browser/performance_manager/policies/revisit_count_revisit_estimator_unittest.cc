// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/revisit_count_revisit_estimator.h"

#include <map>

#include "components/performance_manager/public/decorators/tab_connectedness_decorator.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/user_tuning/tab_revisit_tracker.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"

namespace performance_manager {

class TestTabRevisitTracker : public TabRevisitTracker {
 public:
  void SetStateBundle(const TabPageDecorator::TabHandle* tab_handle,
                      StateBundle bundle) {
    state_bundles_[tab_handle] = bundle;
  }

 private:
  StateBundle GetStateForTabHandle(
      const TabPageDecorator::TabHandle* tab_handle) override {
    return state_bundles_.at(tab_handle);
  }

  std::map<const TabPageDecorator::TabHandle*, TabRevisitTracker::StateBundle>
      state_bundles_;
};

class RevisitCountRevisitEstimatorTest : public GraphTestHarness {
 public:
  void SetUp() override {
    GraphTestHarness::SetUp();
    // Setup the decorators TabRevisitTracker depends on, because it CHECKs them
    // in its `PassedToGraph()`. They won't be used because the
    // TestTabRevisitTracker allows specifying the `StateBundle` for a given tab
    // directly.
    graph()->PassToGraph(
        std::make_unique<performance_manager::TabPageDecorator>());
    graph()->PassToGraph(std::make_unique<TabConnectednessDecorator>());

    std::unique_ptr<TestTabRevisitTracker> tab_revisit_tracker =
        std::make_unique<TestTabRevisitTracker>();
    tab_revisit_tracker_ = tab_revisit_tracker.get();
    graph()->PassToGraph(std::move(tab_revisit_tracker));

    // Advance the clock so that base::TimeTicks::Now() doesn't return 0
    AdvanceClock(base::Hours(72));
  }

  void InitializeEstimator(
      std::map<int64_t, float> revisit_probabilities,
      std::map<int64_t, ProbabilityDistribution> cdf_containers) {
    estimator_ = std::make_unique<RevisitCountRevisitEstimator>(
        graph(), cdf_containers, std::move(revisit_probabilities));
  }

  void TearDown() override {
    tab_revisit_tracker_ = nullptr;
    GraphTestHarness::TearDown();
  }

  RevisitCountRevisitEstimator* estimator() { return estimator_.get(); }

  TestTabRevisitTracker* tab_revisit_tracker() { return tab_revisit_tracker_; }

  TabRevisitTracker::StateBundle CreateStateBundle(
      base::TimeTicks last_active_time,
      int64_t num_revisits) {
    return {TabRevisitTracker::State::kBackground,
            last_active_time,
            {},
            {},
            num_revisits,
            absl::nullopt};
  }

 private:
  std::unique_ptr<RevisitCountRevisitEstimator> estimator_;
  raw_ptr<TestTabRevisitTracker> tab_revisit_tracker_;
};

TEST_F(RevisitCountRevisitEstimatorTest, ComputesProbability) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());

  InitializeEstimator(
      {
          {0L, 0.3f},
      },
      {
          {0L, ProbabilityDistribution::FromCDFData({
                   {1, 0.1},
                   {10, 0.3},
                   {base::Hours(24).InSeconds(), 1.0},
               })},
      });

  // `num_revisits` should match the distribution that will be selected among
  // the ones passed to `InitializeEstimator`
  tab_revisit_tracker()->SetStateBundle(
      tab_handle, CreateStateBundle(base::TimeTicks::Now() - base::Seconds(1),
                                    /*num_revisits=*/0));

  // The probability is revisit_prob * (revisit_before_24h_prob -
  // revisit_before_time_already_spent_in_background_prob). In this case, these
  // values are:
  //
  // revisit_prob: 0.3
  // revisit_before_24h_prob: 1
  // revisit_before_time_already_spent_in_background_prob: 0.1
  //
  // So 0.3 * (1 - 0.1) = 0.27
  EXPECT_EQ(estimator()->ComputeRevisitProbability(tab_handle), 0.3f * 0.9f);
}

TEST_F(RevisitCountRevisitEstimatorTest, ComputesCorrectlyForFirstLastBuckets) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());

  // Initialize with 2 sets of probability distributions to test that the right
  // one is selected based on `num_revisits`
  InitializeEstimator(
      {

          {0L, 0.3f},
          {1L, 0.5f},
      },
      {
          {0L, ProbabilityDistribution::FromCDFData({
                   {1, 0.1},
                   {10, 0.3},
                   {base::Hours(24).InSeconds(), 1.0},
               })},
          {1L, ProbabilityDistribution::FromCDFData({
                   {1L, 0.1},
                   {10L, 0.3},
                   {100L, 1.0},
               })},
      });

  tab_revisit_tracker()->SetStateBundle(
      tab_handle,
      CreateStateBundle(base::TimeTicks::Now(), /*num_revisits=*/1));

  // revisit_prob: 0.5
  // revisit_before_24h_prob: 1
  // revisit_before_time_already_spent_in_background_prob: 0
  //
  // So 0.5 * (1 - 0) = 0.5
  EXPECT_EQ(estimator()->ComputeRevisitProbability(tab_handle), 0.5f);
}

TEST_F(RevisitCountRevisitEstimatorTest, ComputesCorrectlyForMiddleBuckets) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());

  InitializeEstimator(
      {
          {2L, 1.0f},
      },
      {
          {2L, ProbabilityDistribution::FromCDFData({
                   {1, 0.1},
                   {10, 0.3},
                   {base::Hours(24).InSeconds(), 0.5},
                   {base::Hours(48).InSeconds(), 1.0},
               })},
      });

  tab_revisit_tracker()->SetStateBundle(
      tab_handle, CreateStateBundle(base::TimeTicks::Now() - base::Seconds(10),
                                    /*num_revisits=*/2));

  // revisit_prob: 1
  // revisit_before_24h_prob: 0.5
  // revisit_before_time_already_spent_in_background_prob: 0.3
  //
  // So 1 * (0.5 - 0.3) = 0.2
  EXPECT_EQ(estimator()->ComputeRevisitProbability(tab_handle), 0.5f - 0.3f);
}

TEST_F(RevisitCountRevisitEstimatorTest,
       ComputesCorrectlyIfNumRevisitGreaterThanMax) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(mock_graph.page.get());

  InitializeEstimator(
      {
          {TabRevisitTracker::kMaxNumRevisit - 1, 1.0f},
      },
      {
          {TabRevisitTracker::kMaxNumRevisit - 1,
           ProbabilityDistribution::FromCDFData({
               {1, 0.1},
               {10, 0.2},
               {base::Hours(24).InSeconds(), 0.5},
               {base::Hours(48).InSeconds(), 1.0},
           })},
      });

  tab_revisit_tracker()->SetStateBundle(
      tab_handle, CreateStateBundle(base::TimeTicks::Now() - base::Seconds(10),
                                    /*num_revisits=*/999));

  // revisit_prob: 1
  // revisit_before_24h_prob: 0.5
  // revisit_before_time_already_spent_in_background_prob: 0.2
  //
  // So 1 * (0.5 - 0.2) = 0.3
  EXPECT_EQ(estimator()->ComputeRevisitProbability(tab_handle), 0.5f - 0.2f);
}

}  // namespace performance_manager
