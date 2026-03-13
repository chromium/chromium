// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/test_support/new_glic_api_test.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

// MIGRATION IN PROGRESS:
// This test will eventually absorb glic_api_browsertest.cc, as it allows
// execution on Android. Migration will take some time, as some tests need
// rewritten to avoid RunTestSequence which is not supported on Android.

namespace glic {
namespace {

std::vector<std::string> GetTestSuiteNames() {
  return {
      "NewGlicApiTest",
  };
}

}  // namespace

// All tests in this file use the same test params here.
struct TestParams {
  // This is only used by one fixture.
  bool enable_scroll_to_pdf = false;
  bool trust_first_onboarding_arm1 = false;
  bool trust_first_onboarding_arm2 = false;
  bool auto_open_pdf = false;
};

class WithTestParams : public testing::WithParamInterface<TestParams> {
 public:
  WithTestParams() {}

  static std::string PrintTestVariant(
      const ::testing::TestParamInfo<TestParams>& info) {
    std::vector<std::string> result;
    if (info.param.enable_scroll_to_pdf) {
      result.push_back("EnableScrollToPdf");
    }
    if (info.param.trust_first_onboarding_arm1) {
      result.push_back("TrustFirstOnboardingArm1");
    }
    if (info.param.trust_first_onboarding_arm2) {
      result.push_back("TrustFirstOnboardingArm2");
    }
    if (info.param.auto_open_pdf) {
      result.push_back("AutoOpenPdf");
    }
    if (result.empty()) {
      return "Default";
    }
    return base::JoinString(result, "_");
  }

 private:
  base::test::ScopedFeatureList test_param_features_;
};

class NewGlicApiTest : public GlicApiBrowserTest, public WithTestParams {
 public:
  NewGlicApiTest() : GlicApiBrowserTest("./new_glic_api_browsertest.js") {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlic, {}},
         {features::kGlicRollout, {}},
         {features::kGlicScrollTo, {}},
         {features::kGlicApiActivationGating, {}},
         {mojom::features::kGlicMultiTab, {}},
         {features::kGlicWebActuationSetting, {}},
         {features::kGlicCaptureRegion, {}},
         {features::kGlicPopupWindowsEnabled, {}},
         {features::kGlicUserStatusCheck,
          {{features::kGlicUserStatusRefreshApi.name, "true"},
           {features::kGlicUserStatusThrottleInterval.name, "2s"}}},
         {features::kGlicOpenPasswordManagerSettingsPageApi, {}},
#if BUILDFLAG(IS_ANDROID)
         {chrome::android::kBrowserWindowInterfaceMobile, {}},
#endif
         {features::kGlicActor,
          {{features::kGlicActorPolicyControlExemption.name, "true"}}}},
        /*disabled_features=*/
        {
            features::kGlicWarming,
            contextual_cueing::kGlicZeroStateSuggestions,
            features::kGlicDaisyChainNewTabs,
            features::kGlicCountryFiltering,
            features::kGlicLocaleFiltering,
        });
  }

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(CreateAndActivateTab(
        embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  }

  base::test::ScopedFeatureList features_;
};

// Checks that all tests in api_test.ts have a corresponding test case in this
// file.
// TODO(crbug.com/460826483): Enable on CrOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_testAllTestsAreRegistered DISABLED_testAllTestsAreRegistered
#else
#define MAYBE_testAllTestsAreRegistered testAllTestsAreRegistered
#endif
IN_PROC_BROWSER_TEST_P(NewGlicApiTest, MAYBE_testAllTestsAreRegistered) {
  ASSERT_TRUE(OpenGlicForActiveTab());
  AssertAllTestsRegistered(GetTestSuiteNames());
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testDoNothing) {
  ASSERT_TRUE(OpenGlicForActiveTab());
  ExecuteJsTest();
}

auto DefaultTestParamSet() {
  return testing::Values(TestParams{});
}

INSTANTIATE_TEST_SUITE_P(,
                         NewGlicApiTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
}  // namespace glic
