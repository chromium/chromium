// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_logging_settings.h"
#include "base/values.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
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

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

// These are newly failing in test setup on desktop android.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
#define DISABLE_ALL_TESTS
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
      "NewGlicApiTestWithWebContentsWarming",
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


class GlicApiTestPasskeys {
 public:
  static InvokeWithAutoSubmitPasskey GetPassKey() {
    return InvokeWithAutoSubmitPasskeyProvider::GetPassKey();
  }
};

class NewGlicApiTest : public GlicApiBrowserTest,
                       public WithTestParams,
                       public GlicApiTestPasskeys {
 public:
  NewGlicApiTest() : GlicApiBrowserTest("./new_glic_api_browsertest.js") {
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

class NewGlicApiTestWithWebContentsWarming : public NewGlicApiTest {
 public:
  NewGlicApiTestWithWebContentsWarming() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicWebContentsWarming,
          {
              {features::kGlicWebContentsWarmingDelay.name, "200ms"},
          }},
         {features::kGlicWarming,
          {{features::kGlicWarmingDelayMs.name, "0"},
           {features::kGlicWarmingJitterMs.name, "0"}}}},
        {});
  }

  void SetUpOnMainThread() override {
    NewGlicApiTest::SetUpOnMainThread();
    glic::GlicKeyedService::Get(this->GetProfile())
        ->web_contents_warming_pool()
        .Clear();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithWebContentsWarming,
                       testWebClientReadyOnPreload) {
  auto container = glic::GlicKeyedService::Get(this->GetProfile())
                       ->web_contents_warming_pool()
                       .TakeContainer();
  ASSERT_TRUE(container);
  auto* web_contents = container->web_contents();

  // Wait for the WebUI to initialize and reach the kReady state.
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  constexpr char kCheckReadyScript[] = R"js(
    (async () => {
      const controller = window.appRouter.glicController;
      return new Promise((resolve, reject) => {
        const interval = setInterval(() => {
          if (controller.state === 13 /* kWarmed */) {
            clearInterval(interval);
            resolve(true);
          } else if (controller.state === 5 /* kError */ ||
                     controller.state === 6 /* kOffline */ ||
                     controller.state === 7 /* kUnavailable */ ||
                     controller.state === 10 /* kSignIn */ ||
                     controller.state === 11 /* kGuestError */ ||
                     controller.state === 12 /* kDisabledByAdmin */) {
            clearInterval(interval);
            reject(new Error('WebUI entered error state: ' + controller.state));
          }
        }, 10);
      });
    })()
  )js";
  EXPECT_EQ(true, content::EvalJs(web_contents, kCheckReadyScript));
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testFailureForCapturedApiTestError) {
  ASSERT_OK(OpenGlicForActiveTab());
  const std::string expected_failure =
      "Failed at step #1 (single or first) due to (captured error): "
      "Error: Non-throwing test error";
  ExecuteJsTest(
      {.should_fail = true, .should_fail_with_error = expected_failure});
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testLoadWhileWindowClosed) {
  // Open Glic
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicOpen());

  // Close Glic
  CloseAllEmbeddersAndPreventDeletion();
  ASSERT_OK(WaitForGlicClose());

  ExecuteJsTest();

  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testInitializeFailsWindowClosed) {
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicOpen());

  CloseAllEmbeddersAndPreventDeletion();
  ASSERT_OK(WaitForGlicClose());

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testInitializeFailsWindowOpen) {
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicOpen());

  ExecuteJsTest(
      {.params = base::Value(base::DictValue().Set("failWith", "error"))});
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kError));

  GetOnlyGlicInstance()->CloseAllEmbedders();
  ASSERT_OK(WaitForGlicClose());

  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicOpen());

  ExecuteJsTest(
      {.params = base::Value(base::DictValue().Set("failWith", "none"))});
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testReloadWebUi) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kReady));
  GetOnlyGlicInstance()->host().Reload();
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kUninitialized));
  ExecuteJsTest();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetOnlyGlicInstance()->host().GetPageHandlersForTesting().size() ==
           1;
  }));
  ASSERT_TRUE(GetOnlyGlicInstance()->host().GetPrimaryPageHandlerForTesting());
}

// Checks that all tests in new_glic_api_browsertest.ts have a corresponding
// test case in this file.
// TODO(crbug.com/460826483): Enable on CrOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_testAllTestsAreRegistered DISABLED_testAllTestsAreRegistered
#else
#define MAYBE_testAllTestsAreRegistered testAllTestsAreRegistered
#endif
IN_PROC_BROWSER_TEST_P(NewGlicApiTest, MAYBE_testAllTestsAreRegistered) {
  ASSERT_OK(OpenGlicForActiveTab());
  AssertAllTestsRegistered(GetTestSuiteNames());
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testDoNothing) {
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 1);
  ASSERT_EQ(GetTabListInterface()->GetTab(0)->GetContents()->GetURL(),
            GetTestUrl("page.html"));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

// TODO(harringtond): Flaky on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_testInvocationSource DISABLED_testInvocationSource
#else
#define MAYBE_testInvocationSource testInvocationSource
#endif
IN_PROC_BROWSER_TEST_P(NewGlicApiTest, MAYBE_testInvocationSource) {
  for (const auto source : {
           mojom::InvocationSource::kOsHotkey,
           mojom::InvocationSource::kOsButton,
           mojom::InvocationSource::kNudge,
       }) {
    // Close Glic if it exists.
    auto* instance = GetOnlyGlicInstance();
    if (instance) {
      CloseAllEmbeddersAndPreventDeletion(instance);
      ASSERT_OK(WaitForGlicClose());
    }

    // Toggle Glic from source.
    coordinator().Toggle(GetBrowser(), /*prevent_close=*/false,
                         /*source=*/source,
                         /*deprecated_prompt_suggestion=*/std::nullopt,
                         /*deprecated_auto_send=*/false,
                         /*deprecated_conversation_id=*/std::nullopt);

    ASSERT_OK(WaitForGlicOpen());

    ExecuteJsTest({.params = base::Value(static_cast<int>(source))});
  }
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_testFaviconLoadsWithGetTabById \
  DISABLED_testFaviconLoadsWithGetTabById
#else
#define MAYBE_testFaviconLoadsWithGetTabById testFaviconLoadsWithGetTabById
#endif
IN_PROC_BROWSER_TEST_P(NewGlicApiTest, MAYBE_testFaviconLoadsWithGetTabById) {
  auto* tab_0_contents = GetTabListInterface()->GetTab(0)->GetContents();
  ASSERT_TRUE(content::NavigateToURL(tab_0_contents, GetTestUrl("page.html")));
  GetTabListInterface()->OpenTab(GetTestUrl("page2.html"), -1);

  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->sharing_manager().PinTabs(
      {GetTabListInterface()->GetTab(0)->GetHandle(),
       GetTabListInterface()->GetTab(1)->GetHandle()});
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testFaviconLoadsWithGetTabFaviconById) {
  auto* tab_0_contents = GetTabListInterface()->GetTab(0)->GetContents();
  ASSERT_TRUE(content::NavigateToURL(tab_0_contents, GetTestUrl("page.html")));

  GetTabListInterface()->OpenTab(GetTestUrl("page2.html"), -1);

  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->sharing_manager().PinTabs(
      {GetTabListInterface()->GetTab(0)->GetHandle(),
       GetTabListInterface()->GetTab(1)->GetHandle()});
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testFaviconIsUpdated) {
  ASSERT_OK(OpenGlicForActiveTab());

  ExecuteJsTest();

  ASSERT_TRUE(
      content::ExecJs(GetTabListInterface()->GetTab(0)->GetContents(), R"js(
    var link = document.querySelector("link[rel~='icon']");
    link.href = "./red.ico";
  )js"));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testFaviconIsRemoved) {
  ASSERT_OK(OpenGlicForActiveTab());

  ExecuteJsTest();

  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      GetTestUrl("page_no_favicon.html")));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest,
                       testFaviconIsOmittedWithClientCapabilities) {
  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->sharing_manager().PinTabs(
      {GetTabListInterface()->GetActiveTab()->GetHandle()});
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testInvokeWaitsForNotifyPanelWillOpen) {
  ASSERT_OK(OpenGlicForActiveTab());
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  coordinator().InvokeWithAutoSubmit(
      GetPassKey(), GetTabListInterface()->GetActiveTab(), std::move(options));

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithWebContentsWarming,
                       testWebClientReadyOnFullLoad) {
  service()->web_contents_warming_pool().EnsurePreload();
  ASSERT_OK(RunUntilEqual(
      [&]() {
        return service()
                   ->web_contents_warming_pool()
                   .GetWarmedContainerForTesting() != nullptr;
      },
      true));
  // Opening the glic window will trigger the bootstrap, which should transition
  // the WebUI state to kReady.
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kReady));
}

auto DefaultTestParamSet() {
  return testing::Values(TestParams{});
}

#ifndef DISABLE_ALL_TESTS
INSTANTIATE_TEST_SUITE_P(,
                         NewGlicApiTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         NewGlicApiTestWithWebContentsWarming,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
#else
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(NewGlicApiTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    NewGlicApiTestWithWebContentsWarming);
#endif
}  // namespace glic
