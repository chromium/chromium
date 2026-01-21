// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class GlicMetricsBrowserTest : public InProcessBrowserTest {
 public:
  GlicMetricsBrowserTest()
      : GlicMetricsBrowserTest({}, {features::kGlicMultiInstance}) {}

 protected:
  explicit GlicMetricsBrowserTest(
      const std::vector<base::test::FeatureRef>& extra_enabled_features,
      const std::vector<base::test::FeatureRef>& extra_disabled_features = {}) {
    std::vector<base::test::FeatureRef> enabled_features =
        GetDefaultEnabledGlicTestFeatures();
    enabled_features.push_back(features::kGlicTrustFirstOnboarding);
    enabled_features.insert(enabled_features.end(),
                            extra_enabled_features.begin(),
                            extra_enabled_features.end());

    glic_test_environment_ = std::make_unique<GlicTestEnvironment>(
        GlicTestEnvironmentConfig{.fre_status = std::nullopt}, enabled_features,
        extra_disabled_features);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetFRECompletion(browser()->profile(), prefs::FreStatus::kNotStarted);
  }

  std::unique_ptr<GlicTestEnvironment> glic_test_environment_;
};

IN_PROC_BROWSER_TEST_F(GlicMetricsBrowserTest, GlicFreShown_SingleInstance) {
  ASSERT_FALSE(GlicEnabling::IsMultiInstanceEnabled());

  base::UserActionTester user_action_tester;

  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.Shown"), 1);

  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.Dismissed.Onboarding"),
            1);
}

class GlicMetricsMultiInstanceBrowserTest : public GlicMetricsBrowserTest {
 public:
  GlicMetricsMultiInstanceBrowserTest()
      : GlicMetricsBrowserTest({features::kGlicMultiInstance}, {}) {}
};

IN_PROC_BROWSER_TEST_F(GlicMetricsMultiInstanceBrowserTest,
                       GlicFreShown_MultiInstance) {
  ASSERT_TRUE(GlicEnabling::IsMultiInstanceEnabled());

  base::UserActionTester user_action_tester;

  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.Shown"), 1);

  GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile())
      ->ToggleUI(browser(), /*prevent_close=*/false,
                 mojom::InvocationSource::kOsButton);

  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.Dismissed.Onboarding"),
            1);
}

}  // namespace
}  // namespace glic
