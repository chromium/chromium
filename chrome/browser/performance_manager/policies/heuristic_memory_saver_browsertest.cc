// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/features.h"
#include "content/public/test/browser_test.h"

namespace performance_manager {

class HeuristicMemorySaverBrowserTest : public InProcessBrowserTest {
 public:
  HeuristicMemorySaverBrowserTest() {
    features_.InitAndEnableFeature(
        performance_manager::features::kHeuristicMemorySaver);
  }
  ~HeuristicMemorySaverBrowserTest() override = default;

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(HeuristicMemorySaverBrowserTest, StartManager) {
  // This test checks that the UserPerformanceTuningManager can start properly
  // with the kHeuristicMemorySaver feature enabled. Previously, it would
  // attempt to set the discard time for the HighEfficiencyModePolicy which
  // would crash because it is not initialized when kHeuristicMemorySaver is on.
}

}  // namespace performance_manager
