// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_histogram_tester.h"
#include "chrome/browser/glic/test_support/new_glic_api_test.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/skills/features.h"
#include "components/skills/public/skills_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_tab_controller.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

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

class TestExperimentalTriggeringUpdatesHandler
    : public mojom::ExperimentalTriggeringUpdatesHandler {
 public:
  TestExperimentalTriggeringUpdatesHandler(
      mojo::PendingReceiver<mojom::ExperimentalTriggeringUpdatesHandler>
          receiver,
      base::RepeatingCallback<void(mojom::SubscriberObservationType)> callback)
      : receiver_(this, std::move(receiver)), callback_(std::move(callback)) {}

  void OnUpdate(mojom::ExperimentalTriggeringUpdatePtr update,
                mojom::SubscriberObservationType observation) override {
    if (update) {
      last_update_ = std::move(update);
    }
    last_observation_ = observation;
    if (callback_) {
      callback_.Run(observation);
    }
  }

  mojom::ExperimentalTriggeringUpdatePtr GetUpdate() {
    return last_update_.Clone();
  }
  mojom::SubscriberObservationType GetObservation() const {
    return last_observation_;
  }

 private:
  mojo::Receiver<mojom::ExperimentalTriggeringUpdatesHandler> receiver_;
  base::RepeatingCallback<void(mojom::SubscriberObservationType)> callback_;
  mojom::ExperimentalTriggeringUpdatePtr last_update_;
  mojom::SubscriberObservationType last_observation_;
};

namespace {

std::vector<std::string> GetTestSuiteNames() {
  std::vector<std::string> names = {
      "NewGlicApiTest",
      "NewGlicApiTestWithWebContentsWarming",
      "NewGlicApiTestWithPixelOutput",
      "NewGlicApiTestWithGeminiActOnWebPolicy",
      "NewGlicApiMultiProfileTest",
#if !BUILDFLAG(IS_ANDROID)
      "NewGlicApiTestWithSkills",
#endif
  };

  return names;
}

std::string GlicTabId(tabs::TabHandle tab_handle) {
  return base::NumberToString(tab_handle.raw_value());
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
  }

  void SetUpOnMainThread() override {
    GlicApiBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(content::NavigateToURL(
        GetTabListInterface()->GetActiveTab()->GetContents(),
        GetTestUrl("page.html")));
  }

#if !BUILDFLAG(IS_ANDROID)
  void CloseMainBrowserWithIncognitoKeepAlive() {
    CreateIncognitoBrowser();
    CloseBrowserAsynchronously(browser());
  }
#endif

 private:
  logging::ScopedVmoduleSwitches scoped_vmodule_switches_;
  base::test::ScopedFeatureList features_;
};

class NewGlicApiMultiProfileTest : public NewGlicApiTest {
 public:
  BrowserWindowInterface* CreateBrowserWithNewProfile() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    Profile& new_profile =
        profiles::testing::CreateProfileSync(profile_manager, new_path);
    return CreateBrowser(&new_profile);
#else
    NOTREACHED();
#endif
  }
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
    coordinator().GetWebContentsWarmingPoolForTesting().Clear();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class NewGlicApiTestWithPixelOutput : public NewGlicApiTest {
 public:
  NewGlicApiTestWithPixelOutput() {
    // Pixel output is necessary for some tests, but it slows down the tests
    // significantly, and may cause flakes on some platforms.
    EnablePixelOutput(2.0f);
  }
};

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithWebContentsWarming,
                       testWebClientReadyOnPreload) {
  auto container =
      coordinator().GetWebContentsWarmingPoolForTesting().TakeContainer();
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

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testShowClientErrorDialog) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  // Wait for the histogram to be recorded.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetAllSamples("Glic.Api.Client.ErrorDialogShown")
               .size() > 0;
  }));
  histogram_tester.ExpectUniqueSample("Glic.Api.Client.ErrorDialogShown",
                                      /*kDisabledByOrganization*/ 1, 1);

  // Verify that the pref was reset.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return service()->GetAuthController().NeedsSyncForTesting(); }));
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

// TODO(crbug.com/503936424): Re-enable the test.
#if (defined(MEMORY_SANITIZER) && BUILDFLAG(IS_CHROMEOS)) || BUILDFLAG(IS_MAC)
#define MAYBE_testInitializeFailsWindowOpen \
  DISABLED_testInitializeFailsWindowOpen
#else
#define MAYBE_testInitializeFailsWindowOpen testInitializeFailsWindowOpen
#endif
IN_PROC_BROWSER_TEST_P(NewGlicApiTest, MAYBE_testInitializeFailsWindowOpen) {
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
// TODO(b/508340871): Re-enable on Android. Failing because something pops up
// and suppresses the bottom sheet.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
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
                         /*deprecated_prompt_suggestion=*/std::nullopt);

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
IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithPixelOutput,
                       MAYBE_testFaviconLoadsWithGetTabById) {
  auto* tab_0_contents = GetTabListInterface()->GetTab(0)->GetContents();
  ASSERT_TRUE(content::NavigateToURL(tab_0_contents, GetTestUrl("page.html")));
  GetTabListInterface()->OpenTab(GetTestUrl("page2.html"), -1);

  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->sharing_manager().PinTabs(
      {GetTabListInterface()->GetTab(0)->GetHandle(),
       GetTabListInterface()->GetTab(1)->GetHandle()});
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithPixelOutput,
                       testFaviconLoadsWithGetTabFaviconById) {
  auto* tab_0_contents = GetTabListInterface()->GetTab(0)->GetContents();
  ASSERT_TRUE(content::NavigateToURL(tab_0_contents, GetTestUrl("page.html")));

  GetTabListInterface()->OpenTab(GetTestUrl("page2.html"), -1);

  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->sharing_manager().PinTabs(
      {GetTabListInterface()->GetTab(0)->GetHandle(),
       GetTabListInterface()->GetTab(1)->GetHandle()});
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithPixelOutput, testFaviconIsUpdated) {
  ASSERT_OK(OpenGlicForActiveTab());

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithPixelOutput, testFaviconIsRemoved) {
  ASSERT_OK(OpenGlicForActiveTab());

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithPixelOutput,
                       testFaviconIsOmittedWithClientCapabilities) {
  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->sharing_manager().PinTabs(
      {GetTabListInterface()->GetActiveTab()->GetHandle()});
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testInvokeWaitsForNotifyPanelWillOpen) {
  ASSERT_OK(OpenGlicForActiveTab());
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.surface = DefaultSurface{
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface()};
  coordinator().InvokeWithAutoSubmit(GetPassKey(), std::move(options));

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testGetExperimentalTriggeringUpdates) {
  ASSERT_OK(OpenGlicForActiveTab());
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.surface = DefaultSurface{
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface()};

  mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> remote;
  base::RunLoop run_loop;
  TestExperimentalTriggeringUpdatesHandler handler(
      remote.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(
          [](base::RepeatingClosure quit_closure,
             mojom::SubscriberObservationType observation) {
            if (observation == mojom::SubscriberObservationType::kComplete) {
              quit_closure.Run();
            }
          },
          run_loop.QuitClosure()));

  ExecuteJsTest();
  base::test::TestFuture<bool> future;
  coordinator().GetExperimentalTriggeringUpdates(std::move(remote),
                                                 future.GetCallback());
  ContinueJsTest();

  run_loop.Run();
  EXPECT_TRUE(future.Get());

  auto update = handler.GetUpdate();
  ASSERT_TRUE(update);
  EXPECT_EQ(update->type,
            mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion);
  EXPECT_EQ(update->data, "");
  EXPECT_EQ(handler.GetObservation(),
            mojom::SubscriberObservationType::kComplete);
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest,
                       testGetExperimentalTriggeringUpdatesError) {
  ASSERT_OK(OpenGlicForActiveTab());
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.surface = DefaultSurface{
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface()};

  mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> remote;
  base::RunLoop run_loop;
  TestExperimentalTriggeringUpdatesHandler handler(
      remote.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(
          [](base::RepeatingClosure quit_closure,
             mojom::SubscriberObservationType observation) {
            if (observation == mojom::SubscriberObservationType::kError) {
              quit_closure.Run();
            }
          },
          run_loop.QuitClosure()));

  ExecuteJsTest();
  base::test::TestFuture<bool> future;
  coordinator().GetExperimentalTriggeringUpdates(std::move(remote),
                                                 future.GetCallback());
  ContinueJsTest();

  run_loop.Run();
  EXPECT_TRUE(future.Get());

  EXPECT_EQ(handler.GetObservation(), mojom::SubscriberObservationType::kError);
}

IN_PROC_BROWSER_TEST_P(NewGlicApiMultiProfileTest,
                       testPageMetadataCrossProfile) {
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
  GTEST_SKIP() << "Multi-profile tests only supported on Desktop";
#endif
  ASSERT_OK(OpenGlicForActiveTab());
  BrowserWindowInterface* other_browser = CreateBrowserWithNewProfile();
  ASSERT_TRUE(other_browser);
  ASSERT_TRUE(content::NavigateToURL(
      TabListInterface::From(other_browser)->GetActiveTab()->GetContents(),
      GetTestUrl("page.html")));
  auto other_tab_handle =
      TabListInterface::From(other_browser)->GetTab(0)->GetHandle();
  ExecuteJsTest({.params = base::Value(GlicTabId(other_tab_handle))});
}

IN_PROC_BROWSER_TEST_P(NewGlicApiMultiProfileTest, testTabDataCrossProfile) {
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
  GTEST_SKIP() << "Multi-profile tests only supported on Desktop";
#endif
  ASSERT_OK(OpenGlicForActiveTab());
  BrowserWindowInterface* other_browser = CreateBrowserWithNewProfile();
  ASSERT_TRUE(other_browser);
  ASSERT_TRUE(content::NavigateToURL(
      TabListInterface::From(other_browser)->GetActiveTab()->GetContents(),
      GetTestUrl("page.html")));
  auto other_tab_handle =
      TabListInterface::From(other_browser)->GetTab(0)->GetHandle();
  ExecuteJsTest({.params = base::Value(GlicTabId(other_tab_handle))});
}

IN_PROC_BROWSER_TEST_P(NewGlicApiMultiProfileTest, testTabFaviconCrossProfile) {
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
  GTEST_SKIP() << "Multi-profile tests only supported on Desktop";
#endif
  ASSERT_OK(OpenGlicForActiveTab());
  BrowserWindowInterface* other_browser = CreateBrowserWithNewProfile();
  ASSERT_TRUE(other_browser);
  ASSERT_TRUE(content::NavigateToURL(
      TabListInterface::From(other_browser)->GetActiveTab()->GetContents(),
      GetTestUrl("page.html")));
  auto other_tab_handle =
      TabListInterface::From(other_browser)->GetTab(0)->GetHandle();
  ExecuteJsTest({.params = base::Value(GlicTabId(other_tab_handle))});
}

IN_PROC_BROWSER_TEST_P(NewGlicApiMultiProfileTest, testGetContextCrossProfile) {
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
  GTEST_SKIP() << "Multi-profile tests only supported on Desktop";
#endif
  ASSERT_OK(OpenGlicForActiveTab());
  BrowserWindowInterface* other_browser = CreateBrowserWithNewProfile();
  ASSERT_TRUE(other_browser);
  ASSERT_TRUE(content::NavigateToURL(
      TabListInterface::From(other_browser)->GetActiveTab()->GetContents(),
      GetTestUrl("page.html")));
  auto other_tab_handle =
      TabListInterface::From(other_browser)->GetTab(0)->GetHandle();
  ExecuteJsTest({.params = base::Value(GlicTabId(other_tab_handle))});
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithWebContentsWarming,
                       testWebClientReadyOnFullLoad) {
  coordinator().GetWebContentsWarmingPoolForTesting().EnsurePreload();
  ASSERT_OK(RunUntilEqual(
      [&]() {
        return coordinator()
                   .GetWebContentsWarmingPoolForTesting()
                   .GetWarmedContainerForTesting() != nullptr;
      },
      true));
  // Opening the glic window will trigger the bootstrap, which should transition
  // the WebUI state to kReady.
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testGetPageMetadata) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testGetPageMetadataInvalidTabId) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testGetPageMetadataEmptyNames) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest,
                       testGetPageMetadataMultipleSubscriptions) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testGetPageMetadataUpdates) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

// TODO(harringtond): Times out on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_testGetPageMetadataTabDestroyed \
  DISABLED_testGetPageMetadataTabDestroyed
#else
#define MAYBE_testGetPageMetadataTabDestroyed testGetPageMetadataTabDestroyed
#endif
IN_PROC_BROWSER_TEST_P(NewGlicApiTest, MAYBE_testGetPageMetadataTabDestroyed) {
  // Open a second tab and open glic.
  GetTabListInterface()->OpenTab(
      embedded_test_server()->GetURL("/glic/browser_tests/test.html"), -1);
  GetTabListInterface()->ActivateTab(
      GetTabListInterface()->GetTab(0)->GetHandle());
  ASSERT_OK(OpenGlicForActiveTab());

  // Pin both tabs.
  GetOnlyGlicInstance()->sharing_manager().PinTabs(
      {GetTabListInterface()->GetTab(0)->GetHandle(),
       GetTabListInterface()->GetTab(1)->GetHandle()});

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testAdditionalContext) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  // The JS test is now paused.
  glic::mojom::AdditionalContextPtr additional_context =
      glic::mojom::AdditionalContext::New();
  additional_context->name = "part with everything";
  additional_context->tab_id = 1;
  additional_context->frameUrl = GURL("http://example.com");

  // Add a part with data.
  std::vector<uint8_t> data = {'t', 'e', 's', 't'};
  additional_context->parts.push_back(
      glic::mojom::AdditionalContextPart::NewData(glic::mojom::ContextData::New(
          "text/plain", mojo_base::BigBuffer(data))));

  // Add a part with a screenshot.
  std::vector<uint8_t> screenshot_data = {1, 2, 3, 4};
  additional_context->parts.push_back(
      glic::mojom::AdditionalContextPart::NewScreenshot(
          glic::mojom::Screenshot::New(
              10, 20, screenshot_data, "image/png",
              glic::mojom::ImageOriginAnnotations::New())));

  // Add a part with web page data.
  additional_context->parts.push_back(
      glic::mojom::AdditionalContextPart::NewWebPageData(
          glic::mojom::WebPageData::New(glic::mojom::DocumentData::New(
              url::Origin(), "some inner text", false))));

  // Add a part with annotated page data.
  additional_context->parts.push_back(
      glic::mojom::AdditionalContextPart::NewAnnotatedPageData(
          glic::mojom::AnnotatedPageData::New()));

  // Add a part with pdf document data.
  std::vector<uint8_t> pdf_data = {'p', 'd', 'f'};
  additional_context->parts.push_back(
      glic::mojom::AdditionalContextPart::NewPdfDocumentData(
          glic::mojom::PdfDocumentData::New(url::Origin(), pdf_data, false)));

  // Add a part with tab context.
  auto tab_data = glic::mojom::TabData::New();
  tab_data->tab_id = 1;
  tab_data->window_id = 2;
  tab_data->url = GURL("http://example.com");
  tab_data->document_mime_type = "text/html";

  auto tab_context = glic::mojom::TabContext::New();
  tab_context->tab_data = std::move(tab_data);
  additional_context->parts.push_back(
      glic::mojom::AdditionalContextPart::NewTabContext(
          std::move(tab_context)));

  additional_context->parts.push_back(
      glic::mojom::AdditionalContextPart::NewRegion(
          glic::mojom::CapturedRegion::NewRect(gfx::Rect(10, 20, 30, 40))));

  service()->SendAdditionalContext(
      GetTabListInterface()->GetActiveTab()->GetHandle(),
      std::move(additional_context));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTest, testCancelActions) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class NewGlicApiTestWithGeminiActOnWebPolicy : public NewGlicApiTest {
 public:
  NewGlicApiTestWithGeminiActOnWebPolicy() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {{features::kGlicActorEnterprisePrefDefault.name,
          features::kGlicActorEnterprisePrefDefault.GetName(
              features::GlicActorEnterprisePrefDefault::kDisabledByDefault)},
         {features::kGlicActorPolicyControlExemption.name, "false"}});
  }
  ~NewGlicApiTestWithGeminiActOnWebPolicy() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    NewGlicApiTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     &test_url_loader_factory_));

    NewGlicApiTest::SetUpBrowserContextKeyedServices(context);
  }

  void SetUpOnMainThread() override {
    NewGlicApiTest::SetUpOnMainThread();

    GlicEnabling::SetBypassEnablementChecksForTesting(true);

    adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());
    identity_test_env_ = adaptor_->identity_test_env();
    identity_test_env_->SetTestURLLoaderFactory(&test_url_loader_factory_);
    identity_manager_ = IdentityManagerFactory::GetForProfile(GetProfile());

    // Simulate sign-in.
    AccountInfo account_info = identity_test_env_->MakePrimaryAccountAvailable(
        "foo@bar.com", signin::ConsentLevel::kSync);

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_->UpdateAccountInfoForAccount(account_info);
    identity_test_env_->SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        "bar.com", "Full Name", "Given Name", "Locale", "Picture URL");

    policy_provider_.SetupPolicyServiceForPolicyUpdates(
        GetProfile()->GetProfilePolicyConnector()->policy_service());
  }

  void TearDownOnMainThread() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
    identity_manager_ = nullptr;
    identity_test_env_ = nullptr;
    adaptor_.reset();
    policy_provider_.SetupPolicyServiceForPolicyUpdates(nullptr);
    NewGlicApiTest::TearDownOnMainThread();
  }

  void UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState value) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kGeminiActOnWebSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                 base::Value(std::to_underlying(value)), nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithGeminiActOnWebPolicy,
                       testNotifyActOnWebCapabilityChanged) {
  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override(
          policy::ManagementServiceFactory::GetForProfile(GetProfile()),
          policy::EnterpriseManagementAuthority::CLOUD);

  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled);

  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  // Disable the capability.
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kDisabled);
  ContinueJsTest();
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
    ASSERT_OK(OpenGlicForActiveTab());
  }

  void TearDownOnMainThread() override {
    service_ = nullptr;
    NewGlicApiTest::TearDownOnMainThread();
  }

  skills::SkillsService* SkillsService() { return service_; }

  void WaitForSkillsTab(const std::string& path) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      tabs::TabInterface* tab =
          InProcessBrowserTest::browser()->tab_strip_model()->GetActiveTab();
      return tab && base::StartsWith(
                        tab->GetContents()->GetLastCommittedURL().spec(),
                        GURL(chrome::kChromeUISkillsURL).Resolve(path).spec());
    }));
  }

 private:
  raw_ptr<skills::SkillsService> service_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithSkills, testGetSkillSuccess) {
  SkillsService()->AddSkill(/*source_skill_id=*/"source_id_1",
                            /*name=*/"test_skill_1",
                            /*icon=*/"test_icon_1",
                            /*prompt=*/"test_prompt_1");
  SkillsService()->AddSkill(/*source_skill_id=*/"source_id_2",
                            /*name=*/"test_skill_2",
                            /*icon=*/"test_icon_2",
                            /*prompt=*/"test_prompt_2");
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithSkills, testGetSkillPreviewsSuccess) {
  SkillsService()->AddSkill(/*source_skill_id=*/"source_id_1",
                            /*name=*/"test_skill_1",
                            /*icon=*/"test_icon_1",
                            /*prompt=*/"test_prompt_1");
  SkillsService()->AddSkill(/*source_skill_id=*/"source_id_2",
                            /*name=*/"test_skill_2",
                            /*icon=*/"test_icon_2",
                            /*prompt=*/"test_prompt_2");
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithSkills,
                       testDisplaySkillInDialogSuccess) {
#if !BUILDFLAG(IS_ANDROID)  // TODO(harringtond): Enable skills on Android.
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
    auto* controller = static_cast<skills::SkillsUiTabController*>(
        skills::SkillsUiTabControllerInterface::From(tab));
    if (controller && controller->IsShowing()) {
      const auto& skill = controller->GetCurrentSkillForTesting();
      return skill.has_value() && skill->id == "id" && skill->name == "name" &&
             skill->icon == "icon" && skill->prompt == "prompt" &&
             skill->source == sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
    }
    return false;
  }));
#endif
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithSkills, testShowManageSkillsUi) {
  ExecuteJsTest();
  WaitForSkillsTab(chrome::kChromeUISkillsYourSkillsPath);
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithSkills, testShowBrowseSkillsUi) {
  ExecuteJsTest();
  WaitForSkillsTab(chrome::kChromeUISkillsBrowsePath);
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithSkills,
                       testSendingContextualSkillsToGlic) {
#if !BUILDFLAG(IS_ANDROID)  // TODO(harringtond): Enable skills on Android.
  SkillsService()->AddSkill(/*source_skill_id=*/"", /*name=*/"user_skill_1",
                            /*icon=*/"user_icon_1",
                            /*prompt=*/"test_prompt_1");
  SkillsService()->AddSkill(/*source_skill_id=*/"", /*name=*/"user_skill_2",
                            /*icon=*/"user_icon_2",
                            /*prompt=*/"user_prompt_2");

  ExecuteJsTest();

  std::vector<mojom::SkillPreviewPtr> skills_batch_1;
  skills_batch_1.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_1", "contextual_skill_1", "contextual_skill_icon_1",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_1",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com")));
  skills_batch_1.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_2", "contextual_skill_2", "contextual_skill_icon_2",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_2",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com")));

  GlicInstance* instance =
      GlicKeyedServiceFactory::GetGlicKeyedService(GetProfile())
          ->instance_coordinator()
          .GetActiveInstance();
  ASSERT_TRUE(instance);
  instance->host().NotifyContextualSkillsChanged(std::move(skills_batch_1));

  ContinueJsTest();

  std::vector<mojom::SkillPreviewPtr> skills_batch_2;
  skills_batch_2.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_3", "contextual_skill_3", "contextual_skill_icon_3",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_3",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com")));
  instance->host().NotifyContextualSkillsChanged(std::move(skills_batch_2));

  ContinueJsTest();
#endif
}

IN_PROC_BROWSER_TEST_P(NewGlicApiTestWithSkills,
                       testShowManageSkillsUiNoWindow) {
#if !BUILDFLAG(IS_ANDROID)  // TODO(harringtond): Enable skills on Android.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndDetach());
  BrowserWindowInterface* browser_to_close = GetBrowserWindowInterface();
  CreateIncognitoBrowser();
  CloseBrowserAsynchronously(browser_to_close);

  ui_test_utils::WaitForBrowserToClose(browser_to_close);

  ExecuteJsTest({.instance = instance});

  ASSERT_TRUE(base::test::RunUntil([&]() -> bool {
    auto all_bwis = GetAllBrowserWindowInterfaces();
    for (auto* bwi : all_bwis) {
      for (auto* tab : TabListInterface::From(bwi)->GetAllTabs()) {
        if (tab->GetContents()->GetLastCommittedURL().spec().starts_with(
                chrome::kChromeUISkillsURL)) {
          return true;
        }
      }
    }
    return false;
  }));
#endif
}
#endif

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

INSTANTIATE_TEST_SUITE_P(,
                         NewGlicApiTestWithPixelOutput,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         NewGlicApiTestWithGeminiActOnWebPolicy,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         NewGlicApiMultiProfileTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

// Skills are not supported yet on Android.
#if !BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(,
                         NewGlicApiTestWithSkills,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
#endif
#else
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(NewGlicApiTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    NewGlicApiTestWithWebContentsWarming);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(NewGlicApiTestWithPixelOutput);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    NewGlicApiTestWithGeminiActOnWebPolicy);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(NewGlicApiMultiProfileTest);
#if !BUILDFLAG(IS_ANDROID)
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(NewGlicApiTestWithSkills);
#endif
#endif
}  // namespace glic
