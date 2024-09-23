// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/preloading_model_keyed_service.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "chrome/browser/navigation_predictor/preloading_model_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

class PreloadingModelKeyedServiceTest : public InProcessBrowserTest {
 public:
  PreloadingModelKeyedServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPreloadingHeuristicsMLModel,
         optimization_guide::features::kOptimizationHints,
         optimization_guide::features::kOptimizationTargetPrediction,
         optimization_guide::features::kOptimizationGuideModelDownloading},
        {});

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);

    base::FilePath model_file_path =
        source_root_dir.AppendASCII("chrome")
            .AppendASCII("browser")
            .AppendASCII("navigation_predictor")
            .AppendASCII("test")
            .AppendASCII("preloading_heuristics.tflite");

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        optimization_guide::switches::kModelOverride,
        base::StrCat({
            "OPTIMIZATION_TARGET_PRELOADING_HEURISTICS",
            optimization_guide::ModelOverrideSeparator(),
            optimization_guide::FilePathToString(model_file_path),
        }));
  }
  ~PreloadingModelKeyedServiceTest() override = default;

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class PreloadingModelKeyedServiceFeatureTest
    : public PreloadingModelKeyedServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  PreloadingModelKeyedServiceFeatureTest() {
    bool is_enabled = GetParam();
    if (is_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kPreloadingHeuristicsMLModel);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          blink::features::kPreloadingHeuristicsMLModel);
    }
  }
  ~PreloadingModelKeyedServiceFeatureTest() override = default;

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PreloadingModelKeyedServiceFeatureTest,
                       FeatureFlagIsWorking) {
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents()->GetBrowserContext());
  ASSERT_TRUE(OptimizationGuideKeyedServiceFactory::GetForProfile(profile));

  PreloadingModelKeyedService* model_service =
      PreloadingModelKeyedServiceFactory::GetForProfile(profile);
  bool is_enabled = GetParam();
  if (is_enabled) {
    EXPECT_TRUE(model_service);
  } else {
    EXPECT_FALSE(model_service);
  }
}

INSTANTIATE_TEST_SUITE_P(ParametrizedTests,
                         PreloadingModelKeyedServiceFeatureTest,
                         testing::Values(true, false));

IN_PROC_BROWSER_TEST_F(PreloadingModelKeyedServiceTest, Score) {
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents()->GetBrowserContext());
  ASSERT_TRUE(OptimizationGuideKeyedServiceFactory::GetForProfile(profile));

  PreloadingModelKeyedService* model_service =
      PreloadingModelKeyedServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(model_service);

  {
    base::RunLoop run_loop;
    model_service->AddOnModelUpdatedCallbackForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  base::CancelableTaskTracker tracker;
  // TODO(isaboori): Set the inputs and the expected result to something more
  // meaningful.
  PreloadingModelKeyedService::Inputs inputs;
  inputs.contains_image = false;
  inputs.font_size = 0;
  inputs.has_text_sibling = false;
  inputs.is_bold = false;
  inputs.is_in_iframe = false;
  inputs.is_url_incremented_by_one = false;
  inputs.navigation_start_to_link_logged = base::TimeDelta();
  inputs.path_depth = 0;
  inputs.path_length = 0;
  inputs.percent_clickable_area = 0;
  inputs.percent_vertical_distance = 0;
  inputs.is_same_host = false;
  inputs.is_in_viewport = false;
  inputs.is_pointer_hovering_over = false;
  inputs.entered_viewport_to_left_viewport = base::TimeDelta();
  inputs.hover_dwell_time = base::TimeDelta();
  inputs.pointer_hovering_over_count = 0;

  base::test::TestFuture<PreloadingModelKeyedService::Result> score_future;
  model_service->Score(&tracker, inputs, score_future.GetCallback());
  EXPECT_TRUE(score_future.Get().has_value());
}

}  // namespace
