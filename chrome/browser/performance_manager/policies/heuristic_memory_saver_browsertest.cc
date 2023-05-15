// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/policies/heuristic_memory_saver_policy.h"
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class HeuristicMemorySaverBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  HeuristicMemorySaverBrowserTest() {
    features_.InitWithFeatureState(features::kHeuristicMemorySaver, GetParam());
  }

  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HeuristicMemorySaverBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(HeuristicMemorySaverBrowserTest, StartManager) {
  // This test checks that the UserPerformanceTuningManager can start properly
  // with and without the kHeuristicMemorySaver feature enabled. In both states
  // the same policies should be created, so that when the multistate UI is
  // enabled it can switch between them.
  EXPECT_TRUE(policies::HighEfficiencyModePolicy::GetInstance());
  EXPECT_TRUE(policies::HeuristicMemorySaverPolicy::GetInstance());
}

}  // namespace performance_manager
