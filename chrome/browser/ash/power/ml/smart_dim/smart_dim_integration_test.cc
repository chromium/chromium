// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/power/ml/user_activity_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"

namespace ash {

class SmartDimIntegrationTest : public InteractiveAshTest {
 public:
  SmartDimIntegrationTest() {
    feature_list_.InitAndEnableFeature(features::kSmartDim);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SmartDimIntegrationTest, SmartDim) {
  base::HistogramTester histograms;

  // UserActivityController has the underlying implementation of the
  // MlDecisionProvider D-Bus API used by powerd to make screen dim decisions.
  power::ml::UserActivityController user_activity_controller;

  // Request a screen dimming decision and wait for the ML service to return an
  // answer.
  base::RunLoop run_loop;
  user_activity_controller.ShouldDeferScreenDim(
      base::BindLambdaForTesting([&](bool defer) { run_loop.Quit(); }));
  run_loop.Run();

  // WorkerType 0 is built-in worker. This is emitted before chrome queries the
  // ML service.
  histograms.ExpectBucketCount("PowerML.SmartDimComponent.WorkerType", 0, 1);

  // Bucket 0 is ash. This is emitted before chrome queries the ML service.
  histograms.ExpectBucketCount("PowerML.SmartDimFeature.WebPageInfoSource", 0,
                               1);

  // Bucket 0 is success. This is emitted after the ML Service replies
  // to chrome.
  histograms.ExpectBucketCount("PowerML.SmartDimModel.Result", 0, 1);
}

}  // namespace ash
