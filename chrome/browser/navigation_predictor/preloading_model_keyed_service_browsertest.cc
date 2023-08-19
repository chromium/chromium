// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/preloading_model_keyed_service.h"

#include "chrome/browser/navigation_predictor/preloading_model_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

class PreloadingModelKeyedServiceTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  PreloadingModelKeyedServiceTest() {
    bool is_enabled = GetParam();
    if (is_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kPreloadingHeuristicsMLModel);
    }
  }
  ~PreloadingModelKeyedServiceTest() override = default;

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PreloadingModelKeyedServiceTest, FeatureFlagIsWorking) {
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
                         PreloadingModelKeyedServiceTest,
                         testing::Values(true, false));

}  // namespace
