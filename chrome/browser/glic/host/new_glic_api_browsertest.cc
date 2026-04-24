// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_logging_settings.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/test_support/new_glic_api_test.h"
#include "chrome/common/chrome_features.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "base/strings/string_util.h"
#include "chrome/common/webui_url_constants.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_tab_controller.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/skills/features.h"
#include "components/skills/public/skills_service.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
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
      "NewGlicApiTestWithSkills",
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

// These are newly failing in test setup on desktop android.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
#define MAYBE_NewGlicApiTest DISABLED_NewGlicApiTest
#else
#define MAYBE_NewGlicApiTest NewGlicApiTest
#endif

class GlicApiTestPasskeys {
 public:
  static InvokeWithAutoSubmitPasskey GetPassKey() {
    return InvokeWithAutoSubmitPasskeyProvider::GetPassKey();
  }
};

class MAYBE_NewGlicApiTest : public GlicApiBrowserTest,
                             public WithTestParams,
                             public GlicApiTestPasskeys {
 public:
  MAYBE_NewGlicApiTest() : GlicApiBrowserTest("./new_glic_api_browsertest.js") {
    scoped_vmodule_switches_.InitWithSwitches("*glic*=1");
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlic, {}},
         {features::kGlicWebContentsWarming,
          {// Effectively disable warming in this test, as it can make
           // understanding logs difficult. Note that disabling this feature
           // would enable the older instance warming method.
           {features::kGlicWebContentsWarmingDelay.name, "7d"}}},
         {features::kGlicRollout, {}},
         {features::kGlicScrollTo, {}},
         {features::kGlicApiActivationGating, {}},
         {mojom::features::kGlicMultiTab, {}},
         {features::kGlicWebActuationSetting, {}},
         {features::kGlicCaptureRegion, {}},
         {features::kGlicPopupWindowsEnabled, {}},
         {features::kLogJsConsoleMessages, {}},
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
            kGlicZeroStateSuggestions,
            features::kGlicDaisyChainNewTabs,
            features::kGlicCountryFiltering,
            features::kGlicLocaleFiltering,
        });
    EnablePixelOutput(2.0f);
  }

  void SetUpOnMainThread() override {
    GlicApiBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(content::NavigateToURL(
        GetTabListInterface()->GetActiveTab()->GetContents(),
        GetTestUrl("page.html")));
  }

 private:
  logging::ScopedVmoduleSwitches scoped_vmodule_switches_;
  base::test::ScopedFeatureList features_;
};

// Checks that all tests in new_glic_api_browsertest.ts have a corresponding
// test case in this file.
// TODO(crbug.com/460826483): Enable on CrOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_testAllTestsAreRegistered DISABLED_testAllTestsAreRegistered
#else
#define MAYBE_testAllTestsAreRegistered testAllTestsAreRegistered
#endif
IN_PROC_BROWSER_TEST_P(MAYBE_NewGlicApiTest, MAYBE_testAllTestsAreRegistered) {
  ASSERT_TRUE(OpenGlicForActiveTab());
  AssertAllTestsRegistered(GetTestSuiteNames());
}

IN_PROC_BROWSER_TEST_P(MAYBE_NewGlicApiTest, testDoNothing) {
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 1);
  ASSERT_EQ(GetTabListInterface()->GetTab(0)->GetContents()->GetURL(),
            GetTestUrl("page.html"));
  ASSERT_TRUE(OpenGlicForActiveTab());
  ExecuteJsTest();
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_testFaviconLoadsWithGetTabById \
  DISABLED_testFaviconLoadsWithGetTabById
#else
#define MAYBE_testFaviconLoadsWithGetTabById testFaviconLoadsWithGetTabById
#endif
IN_PROC_BROWSER_TEST_P(MAYBE_NewGlicApiTest,
                       MAYBE_testFaviconLoadsWithGetTabById) {
  auto* tab_0_contents = GetTabListInterface()->GetTab(0)->GetContents();
  ASSERT_TRUE(content::NavigateToURL(tab_0_contents, GetTestUrl("page.html")));
  GetTabListInterface()->OpenTab(GetTestUrl("page2.html"), -1);

  ASSERT_TRUE(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->sharing_manager().PinTabs(
      {GetTabListInterface()->GetTab(0)->GetHandle(),
       GetTabListInterface()->GetTab(1)->GetHandle()});
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(MAYBE_NewGlicApiTest,
                       testFaviconLoadsWithGetTabFaviconById) {
  auto* tab_0_contents = GetTabListInterface()->GetTab(0)->GetContents();
  ASSERT_TRUE(content::NavigateToURL(tab_0_contents, GetTestUrl("page.html")));

  GetTabListInterface()->OpenTab(GetTestUrl("page2.html"), -1);

  ASSERT_TRUE(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->sharing_manager().PinTabs(
      {GetTabListInterface()->GetTab(0)->GetHandle(),
       GetTabListInterface()->GetTab(1)->GetHandle()});
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(MAYBE_NewGlicApiTest, testFaviconIsUpdated) {
  ASSERT_TRUE(OpenGlicForActiveTab());

  ExecuteJsTest();

  ASSERT_TRUE(
      content::ExecJs(GetTabListInterface()->GetTab(0)->GetContents(), R"js(
    var link = document.querySelector("link[rel~='icon']");
    link.href = "./red.ico";
  )js"));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(MAYBE_NewGlicApiTest, testFaviconIsRemoved) {
  ASSERT_TRUE(OpenGlicForActiveTab());

  ExecuteJsTest();

  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      GetTestUrl("page_no_favicon.html")));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(MAYBE_NewGlicApiTest,
                       testFaviconIsOmittedWithClientCapabilities) {
  ASSERT_TRUE(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->sharing_manager().PinTabs(
      {GetTabListInterface()->GetActiveTab()->GetHandle()});
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(MAYBE_NewGlicApiTest,
                       testInvokeWaitsForNotifyPanelWillOpen) {
  ASSERT_TRUE(OpenGlicForActiveTab());
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  coordinator().InvokeWithAutoSubmit(
      GetPassKey(), GetTabListInterface()->GetActiveTab(), std::move(options));

  ExecuteJsTest();
}

#if !BUILDFLAG(IS_ANDROID)
class NewGlicApiTestWithSkills : public NewGlicApiTest {
 public:
  NewGlicApiTestWithSkills() {
    scoped_feature_list_.InitAndEnableFeature(::features::kSkillsEnabled);
  }

  void SetUpOnMainThread() override {
    NewGlicApiTest::SetUpOnMainThread();
#if !BUILDFLAG(IS_ANDROID)
    service_ = skills::SkillsServiceFactory::GetForProfile(GetProfile());
    ASSERT_TRUE(service_);
    service_->SetServiceStatusForTesting(
        skills::SkillsService::ServiceStatus::kReady);
#else
    NOTREACHED();
#endif
    ASSERT_TRUE(OpenGlicForActiveTab());
  }

  void TearDownOnMainThread() override {
    service_ = nullptr;
    NewGlicApiTest::TearDownOnMainThread();
  }

  skills::SkillsService* SkillsService() { return service_; }

  void WaitForSkillsTab(const std::string& path) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
      return tab && base::StartsWith(
                        tab->GetContents()->GetLastCommittedURL().spec(),
                        GURL(chrome::kChromeUISkillsURL).Resolve(path).spec());
    }));
  }

 private:
  raw_ptr<skills::SkillsService> service_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithSkills, testShowBrowseSkillsUi) {
  ExecuteJsTest();
  WaitForSkillsTab(chrome::kChromeUISkillsBrowsePath);
}
#endif

auto DefaultTestParamSet() {
  return testing::Values(TestParams{});
}

INSTANTIATE_TEST_SUITE_P(,
                         MAYBE_NewGlicApiTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
// Skills are not supported yet on Android.
#if !BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(,
                         NewGlicApiTestWithSkills,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
#endif
}  // namespace glic
