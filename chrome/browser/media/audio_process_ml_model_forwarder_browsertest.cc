// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_process_ml_model_forwarder.h"

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/delivery/prediction_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/browser/audio_service.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AudioProcessMlModelForwarderBrowserTest : public InProcessBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      media::kWebRtcAudioNeuralResidualEchoEstimation};
};

IN_PROC_BROWSER_TEST_F(AudioProcessMlModelForwarderBrowserTest,
                       TriggerRegistrationWithOptimizationGuide) {
  if (!media::IsChromeWideEchoCancellationEnabled()) {
    // The feature is a no-op when audio processing does not run in the audio
    // service.
    GTEST_SKIP() << "Test requires Chrome-wide echo cancellation";
  }

  // The feature activates on launching the audio service.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    content::GetAudioService();
  }

  // Verify that the optimization guide receives a registration.
  optimization_guide::PredictionManager& prediction_manager =
      g_browser_process->GetFeatures()
          ->optimization_guide_global_feature()
          ->Get()
          .prediction_manager();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return prediction_manager.GetRegisteredOptimizationTargets().contains(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR);
  }));
}

}  // namespace
