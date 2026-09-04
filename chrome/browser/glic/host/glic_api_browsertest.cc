// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <optional>
#include <string>

#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_manager.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_hotkey.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/context/glic_tab_favicon_observer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/host/glic_skills_manager.h"
#include "chrome/browser/glic/host/glic_web_client_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_api_metrics.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/metrics/glic_instance_helper_metrics.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/fake_contextual_cueing_service.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_histogram_tester.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/permissions/system/mock_platform_handle.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/metrics/metrics_service.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_content_test_utils.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/skills/features.h"
#include "components/skills/proto/skill.pb.h"
#include "components/skills/public/skills_prefs.h"
#include "components/skills/public/skills_service.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/net_errors.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/profiles/profile_picker.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/media/audio_ducker.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/skills/skills_ui_tab_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"  // nogncheck
#include "chrome/test/base/ui_test_utils.h"
#include "ui/display/screen.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
#define SLOW_BINARY
#endif

#if BUILDFLAG(IS_ANDROID)
// Used to disable tests for android which have not yet been vetted for android.
// These should be temporary, either the test should be enabled on android or
// explicitly disabled for android later.
#define NOT_VETTED_ON_ANDROID
#endif

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
using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

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
  bool enable_no_web_ui_loader = false;
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
    if (info.param.enable_no_web_ui_loader) {
      result.push_back("EnableNoWebUiLoader");
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

std::unique_ptr<net::test_server::HttpResponse> SorryPageRequestHandler(
    const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_GET ||
      !base::StartsWith(request.relative_url, "/sorry/index.html")) {
    return nullptr;
  }
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HttpStatusCode::HTTP_OK);
  result->set_content_type("text/html");
  result->set_content("Sorry!");
  return result;
}

class GlicApiTest : public GlicApiBrowserTest,
                    public WithTestParams,
                    public GlicApiTestPasskeys {
 public:
  GlicApiTest()
      : GlicApiBrowserTest(GlicTestJsPath("./glic_api_browsertest.js")) {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&SorryPageRequestHandler));
    scoped_vmodule_switches_.InitWithSwitches("*glic*=1");
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlic, {}},
         {features::kGlicProcessCounterAbuseVerdict, {}},
         {features::kGlicWebContentsWarming,
          {// Effectively disable warming in this test, as it can make
           // understanding logs difficult. Note that disabling this feature
           // would enable the older instance warming method.
           {features::kGlicWebContentsWarmingDelay.name, "7d"}}},
         {features::kGlicRollout, {}},
         {features::kGlicScrollTo, {}},
         {mojom::features::kGlicMultiTab, {}},
         {features::kGlicWebActuationSetting, {}},
         {features::kGlicCaptureRegion, {}},
         {features::kGlicPopupWindowsEnabled, {}},
         {features::kLogJsConsoleMessages, {}},
         {features::kGlicUserStatusCheck,
          {{features::kGlicUserStatusRefreshApi.name, "true"},
           {features::kGlicUserStatusThrottleInterval.name, "2s"}}},
         {features::kGlicOpenPasswordManagerSettingsPageApi, {}},
         {features::kGlicOpenContactInfoSettingsPageApi, {}},
         {features::kGlicActor,
          {{features::kGlicActorPolicyControlExemption.name, "true"}}},
         {blink::features::kAIPageContentTrackedElementsIframe, {}}},
        /*disabled_features=*/
        {
            features::kGlicWarming,
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
    PlatformBrowserTest::CreateIncognitoBrowser();
    CloseBrowserAsynchronously(GetBrowserWindowInterface());
  }

  GlicWidget* GetGlicWidget() {
    GlicInstanceImpl* instance = GetOnlyGlicInstance();
    if (!instance) {
      return nullptr;
    }
    views::View* view = instance->GetActiveEmbedderGlicViewForTesting();
    if (!view) {
      return nullptr;
    }
    return static_cast<GlicWidget*>(view->GetWidget());
  }

  TestResult<> WaitUntilCanResize(bool can_resize) {
    return RunUntilEqual(
        [&]() {
          auto* widget = GetGlicWidget();
          return widget
                     ? (widget->widget_delegate()->CanResize() ? "CanResize"
                                                               : "CannotResize")
                     : "NoWidget";
        },
        can_resize ? std::string("CanResize") : std::string("CannotResize"));
  }
#endif

  int GetPopupCount() {
    int popup_count = 0;
    ProfileBrowserCollection::GetForProfile(GetProfile())
        ->ForEach([&popup_count](BrowserWindowInterface* browser) {
          if (browser->GetType() == BrowserWindowInterface::TYPE_POPUP) {
            popup_count++;
          }
          return true;
        });
    return popup_count;
  }

 private:
  logging::ScopedVmoduleSwitches scoped_vmodule_switches_;
  base::test::ScopedFeatureList features_;
};

class GlicApiTestNoFloatyOrLiveMode : public GlicApiTest {
 public:
  GlicApiTestNoFloatyOrLiveMode() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {},
        /*disabled_features=*/
        {features::kGlicLiveMode});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicApiTestWithFastTimeout : public GlicApiTest {
 public:
  GlicApiTestWithFastTimeout() {
    features_fast_timeout_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{
            features::kGlicWebClientLoadTimes,
            {
#if defined(SLOW_BINARY)
                {features::kGlicMaxLoadingTimeMs.name, "6000"},
#else
                {features::kGlicMaxLoadingTimeMs.name, "2000"},
#endif
            },
        }},
        /*disabled_features=*/
        {});
  }

 private:
  base::test::ScopedFeatureList features_fast_timeout_;
};

class GlicApiTestWithNewTabDaisyChain : public GlicApiTest {
 public:
  GlicApiTestWithNewTabDaisyChain() {
    daisy_chain_features_.InitAndEnableFeature(
        features::kGlicDaisyChainNewTabs);
  }

  void SetUpOnMainThread() override {
    // Daisy chaining side panels across tabs is only supported on Desktop
    // platforms (including Desktop Android).
    SKIP_TEST_FOR_NON_DESKTOP_ANDROID();
    GlicApiTest::SetUpOnMainThread();
    GetProfile()->GetPrefs()->SetBoolean(
        prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, true);
  }

 private:
  base::test::ScopedFeatureList daisy_chain_features_;
};

class GlicApiMultiProfileTest : public GlicApiTest {
 public:
  BrowserWindowInterface* CreateBrowserWithNewProfile() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    Profile& new_profile =
        profiles::testing::CreateProfileSync(profile_manager, new_path);
    return PlatformBrowserTest::CreateBrowser(&new_profile);
#else
    NOTREACHED();
#endif
  }
};

class GlicApiTestWithWebContentsWarming : public GlicApiTest {
 public:
  GlicApiTestWithWebContentsWarming() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicWebContentsWarming,
          {
              {features::kGlicWebContentsWarmingDelay.name, "200ms"},
          }},
         {features::kGlicWarming, {{features::kGlicWarmingDelayMs.name, "0"}}}},
        {});
  }

  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();
    coordinator().GetWebContentsWarmingPoolForTesting().Shutdown();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class GlicApiTestWithPixelOutput : public GlicApiTest {
 public:
  GlicApiTestWithPixelOutput() {
    // Pixel output is necessary for some tests, but it slows down the tests
    // significantly, and may cause flakes on some platforms.
    EnablePixelOutput();
  }
};

class GlicApiTestWithDefaultTabContextDisabled : public GlicApiTest {
 public:
  GlicApiTestWithDefaultTabContextDisabled() {
    feature_list_.InitWithFeatures({},
                                   {features::kGlicDefaultTabContextSetting});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class GlicApiTestWithBlankInstanceDelay : public GlicApiTest {
 public:
  GlicApiTestWithBlankInstanceDelay() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kGlicRemoveBlankInstancesOnClose, {{"delay", "100ms"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class GlicApiTestWithExperimentalTriggeringScreenshot : public GlicApiTest {
 public:
  GlicApiTestWithExperimentalTriggeringScreenshot() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicExperimentalTriggeringScreenshot, {}},
         {features::kGlicActor,
          {{features::kGlicActorPolicyControlExemption.name, "true"}}}},
        {});
  }

 protected:
  base::expected<actor::TaskId, std::string> CreateActorTaskObservingActiveTab(
      GlicInstance* instance) {
    ASSIGN_OR_RETURN(actor::TaskId task_id,
                     GlicApiTest::CreateActorTask(instance));
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(GetProfile());
    actor::ActorTask* task =
        actor_service ? actor_service->GetTask(task_id) : nullptr;
    if (!task) {
      return base::unexpected("ActorTask not found in ActorKeyedService");
    }
    tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
    if (!active_tab) {
      return base::unexpected("No active tab found in TabListInterface");
    }
    task->ObserveTabOnce(active_tab->GetHandle());
    return task_id;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextDisabled,
                       testDefaultTabContextApiIsUndefinedWhenFeatureDisabled) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testCanAttachPanelDetachedTabClosed \
  DISABLED_testCanAttachPanelDetachedTabClosed
#else
#define MAYBE_testCanAttachPanelDetachedTabClosed \
  testCanAttachPanelDetachedTabClosed
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextDisabled,
                       MAYBE_testCanAttachPanelDetachedTabClosed) {
  ASSERT_OK(OpenGlicForActiveTab());
  // Save first tab.
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();

  // Runs the JS test until the first `advanceToNextStep()`.
  ExecuteJsTest();

  // The JS test is now paused.
  // Add a new tab to keep the browser alive before closing the active tab.
  CreateAndActivateTab(
      embedded_test_server()->GetURL("/glic/browser_tests/test.html"));
  first_tab->Close();

  // Continue the JS test to verify canAttachPanel becomes false.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextDisabled,
                       testPinTabsHaveNoEffectOnFocusedTab) {
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  const int first_tab_id = first_tab->GetHandle().raw_value();

  tabs::TabInterface* second_tab = CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  const int second_tab_id = second_tab->GetHandle().raw_value();

  ASSERT_OK(OpenGlicForActiveTab());

  ExecuteJsTest({.params = base::Value(
                     base::DictValue()
                         .Set("tabId1", base::NumberToString(first_tab_id))
                         .Set("tabId2", base::NumberToString(second_tab_id)))});
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testAttachPanel DISABLED_testAttachPanel
#else
#define MAYBE_testAttachPanel testAttachPanel
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testAttachPanel) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicApiTestForNoWebUiLoader : public GlicApiTest {
 public:
  GlicApiTestForNoWebUiLoader() {
    if (GetParam().enable_no_web_ui_loader) {
      feature_list_.InitWithFeatures({features::kGlicNoWebUiLoader}, {});
    } else {
      feature_list_.InitWithFeatures({}, {features::kGlicNoWebUiLoader});
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestForNoWebUiLoader, testNoWebUiLoader) {
  ToggleGlicForActiveTab(/*prevent_close=*/true);
  auto* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);

  WebUIStateListener listener(&instance->host());

  ASSERT_OK(WaitForGlicOpen());

  auto* web_contents = instance->host().webui_contents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // Wait for kReady state to ensure loading is complete.
  ASSERT_TRUE(WaitForWebUiState(mojom::WebUiState::kReady).has_value());

  bool expect_loading = !GetParam().enable_no_web_ui_loader;
  EXPECT_EQ(expect_loading, listener.SawState(mojom::WebUiState::kShowLoading));

  ExecuteJsTest();
}

class GlicOnboardingApiTest : public GlicApiTest {
 public:
  GlicOnboardingApiTest() {
    glic_test_environment().SetFreStatusForNewProfiles(
        prefs::FreStatus::kNotStarted);
  }

  void TearDownOnMainThread() override {
    ForceConnectionTypeForTesting(std::nullopt);
    GlicApiTest::TearDownOnMainThread();
  }
};

class GlicApiTestSystemSettingsTest : public GlicApiTest {
 public:
  GlicApiTestSystemSettingsTest() {
    system_permission_settings::SetInstanceForTesting(&mock_platform_handle);
    // Glic initialization queries initial state for various system permissions
    // (such as Geolocation, Microphone, and Camera). Return default values
    // for all unspecified permissions to prevent unexpected mock call failures.
    EXPECT_CALL(mock_platform_handle, IsAllowed(testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(mock_platform_handle, IsDenied(testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(mock_platform_handle, CanPrompt(testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(true));
  }

  ~GlicApiTestSystemSettingsTest() override {
    system_permission_settings::SetInstanceForTesting(nullptr);
  }

  testing::NiceMock<system_permission_settings::MockPlatformHandle>
      mock_platform_handle;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestSystemSettingsTest,
                       testOpenOsMediaPermissionSettings) {
  ASSERT_OK(OpenGlicForActiveTab());
  base::test::TestFuture<void> signal;
  EXPECT_CALL(
      mock_platform_handle,
      OpenSystemSettings(testing::_, ContentSettingsType::MEDIASTREAM_MIC))
      .WillOnce(base::test::InvokeFuture(signal));

  // Trigger the openOsPermissionSettingsMenu API with 'media'.
  ExecuteJsTest();
  // Wait for OpenSystemSettings to be called.
  EXPECT_TRUE(signal.Wait());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestSystemSettingsTest,
                       testOpenOsGeoPermissionSettings) {
  ASSERT_OK(OpenGlicForActiveTab());
  base::test::TestFuture<void> signal;
  EXPECT_CALL(mock_platform_handle,
              OpenSystemSettings(testing::_, ContentSettingsType::GEOLOCATION))
      .WillOnce(base::test::InvokeFuture(signal));

  // Trigger the openOsPermissionSettingsMenu API with 'geolocation'.
  ExecuteJsTest();
  // Wait for OpenSystemSettings to be called.
  EXPECT_TRUE(signal.Wait());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestSystemSettingsTest,
                       testGetOsMicrophonePermissionStatusAllowed) {
  EXPECT_CALL(mock_platform_handle,
              IsAllowed(ContentSettingsType::MEDIASTREAM_MIC))
      .WillRepeatedly(testing::Return(true));
  ASSERT_OK(OpenGlicForActiveTab());

  // Trigger the GetOsMicrophonePermissionStatus API and check if it returns
  // true as mocked by this test.
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestSystemSettingsTest,
                       testGetOsMicrophonePermissionStatusNotAllowed) {
  EXPECT_CALL(mock_platform_handle,
              IsAllowed(ContentSettingsType::MEDIASTREAM_MIC))
      .WillRepeatedly(testing::Return(false));
  ASSERT_OK(OpenGlicForActiveTab());

  // Trigger the GetOsMicrophonePermissionStatus API and check if it returns
  // false as mocked by this test.
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicOnboardingApiTest, testIsOnboardingCompleted) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  glic::SetFRECompletion(GetProfile(), prefs::FreStatus::kCompleted);
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicOnboardingApiTest, testSetOnboardingCompleted) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  ASSERT_FALSE(GlicEnabling::HasConsentedForProfile(GetProfile()));

// Android doesn't use global hotkeys or a taskbar launcher
#if !BUILDFLAG(IS_ANDROID)
  base::test::TestFuture<void> default_browser_check_called;
  // Ensure that CheckDefaultBrowserToEnableLauncher was called.
  GlicLauncherConfiguration::SetCheckDefaultBrowserCallbackForTesting(
      default_browser_check_called.GetRepeatingCallback());
#endif

  base::UserActionTester user_action_tester;
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("Glic.Onboarding.OptInAccept"));

  ContinueJsTest();

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GlicEnabling::HasConsentedForProfile(GetProfile()); }));

// Android doesn't use global hotkeys or a taskbar launcher
#if !BUILDFLAG(IS_ANDROID)
  // Wait for the default browser check to be called.
  EXPECT_TRUE(default_browser_check_called.Wait());
  GlicLauncherConfiguration::SetCheckDefaultBrowserCallbackForTesting(
      base::RepeatingClosure());
#endif

  EXPECT_EQ(1,
            user_action_tester.GetActionCount("Glic.Onboarding.OptInAccept"));

  ContinueJsTest();
}

class GlicApiTestWithDefaultTabContextEnabled : public GlicApiTest {
 public:
  GlicApiTestWithDefaultTabContextEnabled() {
    feature_list_.InitWithFeatures({features::kGlicDefaultTabContextSetting},
                                   {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class GlicApiTestGeminiEnterpriseSettingsOverride : public GlicApiTest {
 public:
  GlicApiTestGeminiEnterpriseSettingsOverride() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicGeminiEnterpriseSettingsEnabled);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kGlicGeminiEnterpriseSettingsOverride,
        "{\"project_id\": \"switch-project\", \"app_id\": \"switch-engine\", "
        "\"location\": \"switch-location\"}");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestGeminiEnterpriseSettingsOverride,
                       testGeminiEnterpriseSettings) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicApiTestGeminiEnterpriseSettingsDisabled
    : public GlicApiTestGeminiEnterpriseSettingsOverride {
 public:
  GlicApiTestGeminiEnterpriseSettingsDisabled() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kGlicGeminiEnterpriseSettingsEnabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestGeminiEnterpriseSettingsDisabled,
                       testGeminiEnterpriseSettingsDisabled) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

// TODO(b/544838006): Enterprise policies are not currently supported
// on Android. Re-enable these tests once support is added.
#if !BUILDFLAG(IS_ANDROID)
class GlicApiTestGeminiEnterpriseSettingsPolicy : public GlicApiTest {
 public:
  GlicApiTestGeminiEnterpriseSettingsPolicy() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicGeminiEnterpriseSettingsEnabled);
  }

  void SetUpInProcessBrowserTestFixture() override {
    GlicApiTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();

    // Set hosted domain to enterprise.com.
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile());
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    AccountInfo account_info =
        identity_manager->FindExtendedAccountInfo(primary_account);
    AccountInfo::Builder builder(account_info);
    builder.SetHostedDomain("enterprise.com");
    signin::UpdateAccountInfoForAccount(identity_manager, builder.Build());

    policy_provider_.SetupPolicyServiceForPolicyUpdates(
        GetProfile()->GetProfilePolicyConnector()->policy_service());

    base::DictValue enterprise_settings;
    enterprise_settings.Set("project_id", "policy-project");
    enterprise_settings.Set("app_id", "policy-engine");
    enterprise_settings.Set("location", "policy-location");
    policy::PolicyMap policies =
        policy_provider_.policies()
            .Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                         std::string()))
            .Clone();
    policies.Set(policy::key::kGeminiEnterpriseSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(std::move(enterprise_settings)), nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

  void TearDownOnMainThread() override {
    policy_provider_.SetupPolicyServiceForPolicyUpdates(nullptr);
    GlicApiTest::TearDownOnMainThread();
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestGeminiEnterpriseSettingsPolicy,
                       testGeminiEnterpriseSettingsPolicy) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicApiTestGeminiEnterpriseSettingsPolicyUnset
    : public GlicApiTestGeminiEnterpriseSettingsPolicy {
 public:
  void SetUpOnMainThread() override {
    GlicApiTestGeminiEnterpriseSettingsPolicy::SetUpOnMainThread();
    // Unset the policy.
    policy::PolicyMap policies =
        policy_provider_.policies()
            .Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                         std::string()))
            .Clone();
    policies.Erase(policy::key::kGeminiEnterpriseSettings);
    policy_provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_P(GlicApiTestGeminiEnterpriseSettingsPolicyUnset,
                       testGeminiEnterpriseSettingsPolicyUnset) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}
#endif

IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextEnabled,
                       testGetDefaultTabContextPermissionState) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                       false);
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextEnabled, testPinOnBind) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextEnabled,
                       testNoPinOnBindWhenSettingOff) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                       false);

  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetPinCandidatesSingleTab) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  // The tab is automatically pinned. Unpin it now.
  GetOnlyGlicInstance()->GetSharingManagerInternal().UnpinAllTabs();
  ExecuteJsTest();
}

// Flaky on Android and MSan.
// TODO(crbug.com/554636751): Flaky on Linux.
#if BUILDFLAG(IS_ANDROID) || defined(MEMORY_SANITIZER) || BUILDFLAG(IS_LINUX)
#define MAYBE_testGetPinCandidatesWithPanelClosed \
  DISABLED_testGetPinCandidatesWithPanelClosed
#else
#define MAYBE_testGetPinCandidatesWithPanelClosed \
  testGetPinCandidatesWithPanelClosed
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testGetPinCandidatesWithPanelClosed) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  PreventDeletionOnClose();

  // Save first tab.
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();

  ExecuteJsTest();

  CreateAndActivateTab(
      embedded_test_server()->GetURL("/glic/browser_tests/test.html"));
  ContinueJsTest();

  // Activate the first tab again to reuse GlicInstance 1.
  ActivateTab(first_tab);

  // Opens the panel again.
  ASSERT_OK(OpenGlicForActiveTab());
  ContinueJsTest();
}

// TODO(crbug.com/530946737): Failing on tablet builders.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_testGetFormFactor DISABLED_testGetFormFactor
#else
#define MAYBE_testGetFormFactor testGetFormFactor
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testGetFormFactor) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetFocusedTabStateV2) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testClosePanel) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(WaitForGlicClose());
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testClosePanelAndShutdown) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(WaitForGlicClose());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testShowProfilePicker) {
  base::test::TestFuture<void> profile_picker_opened;
  ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
      profile_picker_opened.GetCallback());
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_TRUE(profile_picker_opened.Wait());
}
#endif

#if !BUILDFLAG(IS_ANDROID)
// TODO(https://crbug.com/512641949): Fix flakes.
#if BUILDFLAG(IS_CHROMEOS) || !defined(NDEBUG)
#define MAYBE_testPanelActive DISABLED_testPanelActive
#else
#define MAYBE_testPanelActive testPanelActive
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testPanelActive) {
  ASSERT_OK(OpenGlicForActiveTab());
  PreventDeletionOnClose();
  ExecuteJsTest();

  // Opening a new browser window will deactivate the previous one, and make
  // the panel not active.
  auto params = std::make_unique<NavigateParams>(
      InProcessBrowserTest::browser()->GetProfile(), GURL("about:blank"),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params->disposition = WindowOpenDisposition::NEW_WINDOW;
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      glic::Navigate(std::move(params));

  ContinueJsTest();
}
#endif

// OS-global launcher hotkeys are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetOsHotkeyState) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest({.params = base::Value(base::DictValue().Set(
                     "expectedHotkey", GetHotkeyString()))});
  g_browser_process->local_state()->SetString(prefs::kGlicLauncherHotkey,
                                              "Ctrl+Shift+1");
  ContinueJsTest({.params = base::Value(base::DictValue().Set(
                      "expectedHotkey", GetHotkeyString()))});
  g_browser_process->local_state()->SetString(prefs::kGlicLauncherHotkey, "");
  ContinueJsTest({.params = base::Value(base::DictValue().Set(
                      "expectedHotkey", GetHotkeyString()))});
}
#endif

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetFocusedTabStateV2WithNavigation) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  // Navigate the active tab.
  auto* active_tab = GetTabListInterface()->GetActiveTab();
  NavigateTab(*active_tab, GetTestUrl("page2.html"));
  ContinueJsTest();

  // Create and activate a second tab.
  auto* second_tab = CreateAndActivateTab(
      embedded_test_server()->GetURL("/glic/browser_tests/test.html"));
  // Pin the tab so that it is eligible for sharing and focused.
  GetOnlyGlicInstance()->GetSharingManager()->PinTabs(
      {second_tab->GetHandle()});
  ContinueJsTest();
}
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testGetFocusedTabStateV2WithNavigationWhenInactive) {
  ASSERT_OK(OpenGlicForActiveTab());
  // Prevent the instance from being deleted when closed.
  PreventDeletionOnClose();
  ExecuteJsTest();

  // Close the panel.
  auto* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK(CloseGlicForTabAndWait(tab));

  // Navigate the tab.
  NavigateTab(*tab, GetTestUrl("page2.html"));
  ContinueJsTest();

  // Reopen Glic.
  ASSERT_OK(OpenGlicForActiveTab());
  ContinueJsTest();
}
// TODO(b/548051210): Flaky on Android and Linux due to debounce timing.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_testSingleFocusedTabUpdatesOnTabEvents \
  testSingleFocusedTabUpdatesOnTabEvents
#else
#define MAYBE_testSingleFocusedTabUpdatesOnTabEvents \
  DISABLED_testSingleFocusedTabUpdatesOnTabEvents
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       MAYBE_testSingleFocusedTabUpdatesOnTabEvents) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  // Step 2: Navigate the active tab.
  auto* first_tab = GetTabListInterface()->GetActiveTab();
  NavigateTab(*first_tab, GetTestUrl("page2.html"));
  ContinueJsTest();

  // Step 3: Create and activate a second tab.
  auto* second_tab = CreateAndActivateTab(GetTestUrl("page.html"));
  GetOnlyGlicInstance()->GetSharingManager()->PinTabs(
      {second_tab->GetHandle()});
  ContinueJsTest();
}
// TODO(crbug.com/554315364): Fix flaky test.
IN_PROC_BROWSER_TEST_P(GlicApiTest, DISABLED_testGetZoomLevel) {
  // Confirm that the observer is notified through getZoomLevel of the initial
  // state, i.e. zoom level of 1.0.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ExecuteJsTest();

  // Zoom in and confirm that the observer is notified of the new state, i.e.
  // zoom level of 1.1.
  instance->host().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetPanelStateAttached) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testGetPanelStateAttachedHidden \
  DISABLED_testGetPanelStateAttachedHidden
#else
#define MAYBE_testGetPanelStateAttachedHidden testGetPanelStateAttachedHidden
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testGetPanelStateAttachedHidden) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  // Save first tab.
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();

  ExecuteJsTest();

  // Open and select a second tab. This should result in panel state hidden.
  CreateAndActivateTab(
      embedded_test_server()->GetURL("/glic/browser_tests/test.html"));
  ContinueJsTest();
  ASSERT_OK(RunUntilEqual(
      [&]() { return instance->host().web_client_contents()->GetVisibility(); },
      content::Visibility::HIDDEN));

  // Open the first tab again, it should send the attached state.
  ActivateTab(first_tab);
#if BUILDFLAG(IS_ANDROID)
  // On Android, activating a tab puts Glic in peeked state. We need to
  // explicitly show/expand it to make it attached.
  ASSERT_OK(OpenGlicForActiveTab());
#endif
  ContinueJsTest();
  EXPECT_EQ(instance->host().web_client_contents()->GetVisibility(),
            content::Visibility::VISIBLE);
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testCanAttachPanelSidePanel DISABLED_testCanAttachPanelSidePanel
#else
#define MAYBE_testCanAttachPanelSidePanel testCanAttachPanelSidePanel
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testCanAttachPanelSidePanel) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testCanAttachPanelDetached DISABLED_testCanAttachPanelDetached
#else
#define MAYBE_testCanAttachPanelDetached testCanAttachPanelDetached
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testCanAttachPanelDetached) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testDetachPanel DISABLED_testDetachPanel
#else
#define MAYBE_testDetachPanel testDetachPanel
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testDetachPanel) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testMultiplePanelsAttachedAndDetached \
  DISABLED_testMultiplePanelsAttachedAndDetached
#else
#define MAYBE_testMultiplePanelsAttachedAndDetached \
  testMultiplePanelsAttachedAndDetached
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       MAYBE_testMultiplePanelsAttachedAndDetached) {
  // Save first tab (already active and navigated in SetUpOnMainThread).
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();

  // Create a second tab.
  tabs::TabInterface* second_tab = CreateAndActivateTab(
      embedded_test_server()->GetURL("/glic/browser_tests/test.html"));

  // Go back to the first tab.
  ActivateTab(first_tab);

  // Open Glic for the first tab.
  ASSERT_OK(OpenGlicForActiveTab());
  GlicInstanceImpl* tab0_instance = GetInstanceForTab(first_tab);
  ASSERT_TRUE(tab0_instance);

  // Execute test on the first tab instance.
  ExecuteJsTest({.params = base::Value("first"), .instance = tab0_instance});

  // Select the second tab and open glic.
  ActivateTab(second_tab);
  ASSERT_OK(OpenGlicForActiveTab());
  GlicInstanceImpl* tab1_instance = GetInstanceForTab(second_tab);
  ASSERT_TRUE(tab1_instance);

  // Execute test on the second instance.
  ExecuteJsTest({.params = base::Value("second"), .instance = tab1_instance});

  // Continue on the first tab.
  ContinueJsTest({.instance = tab0_instance});
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testThereCanOnlyBeOneFloaty DISABLED_testThereCanOnlyBeOneFloaty
#else
#define MAYBE_testThereCanOnlyBeOneFloaty testThereCanOnlyBeOneFloaty
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testThereCanOnlyBeOneFloaty) {
  // Save first tab (already active and navigated in SetUpOnMainThread).
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();

  // Create a second tab.
  tabs::TabInterface* second_tab = CreateAndActivateTab(
      embedded_test_server()->GetURL("/glic/browser_tests/test.html"));

  // Go back to the first tab.
  ActivateTab(first_tab);

  // Open Glic for the first tab.
  ASSERT_OK(OpenGlicForActiveTab());
  GlicInstanceImpl* tab0_instance = GetInstanceForTab(first_tab);
  ASSERT_TRUE(tab0_instance);

  // Execute test on the first tab instance.
  ExecuteJsTest({.params = base::Value("first"), .instance = tab0_instance});
  // Verify that the first tab instance is detached before opening the second
  // tab.
  ASSERT_EQ(mojom::PanelStateKind::kDetached,
            tab0_instance->GetPanelState().kind);

  // Select the second tab, open Floaty, and execute the test on the second
  // instance.
  ActivateTab(second_tab);
  ASSERT_OK(OpenGlicForActiveTab());
  GlicInstanceImpl* tab1_instance = GetInstanceForTab(second_tab);
  ASSERT_TRUE(tab1_instance);
  ExecuteJsTest({.params = base::Value("second"), .instance = tab1_instance});

  // Continue on the first tab. Verify there's only one Floaty.
  ContinueJsTest({.instance = tab0_instance});

  ASSERT_EQ(mojom::PanelStateKind::kDetached,
            tab1_instance->GetPanelState().kind);
  ASSERT_EQ(mojom::PanelStateKind::kHidden,
            tab0_instance->GetPanelState().kind);
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testSwitchConversationWithEmptyId \
  DISABLED_testSwitchConversationWithEmptyId
#else
#define MAYBE_testSwitchConversationWithEmptyId \
  testSwitchConversationWithEmptyId
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testSwitchConversationWithEmptyId) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());

  ExecuteJsTest({.params = base::Value("initiateSwitch")});

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.Interaction.SwitchConversationTarget",
               GlicSwitchConversationTarget::kStartNewConversation) == 1 &&
           coordinator().GetInstances().size() == 1u;
  }));

  // Verify that the active instance now has no conversation ID (std::nullopt)
  // because it switched to a new conversation with an empty ID.
  ASSERT_FALSE(GetOnlyGlicInstance()->conversation_id());

  // Verify that GetConversationInfo() returns the info for the new
  // conversation.
  mojom::ConversationInfoPtr retrieved_info =
      GetOnlyGlicInstance()->GetConversationInfo();
  EXPECT_EQ("", retrieved_info->conversation_id);
  EXPECT_EQ("Empty Switched Title", retrieved_info->conversation_title);
  EXPECT_EQ("test_client_data_from_ts", retrieved_info->client_data);

  // Verify that client data was received by the new client.
  ExecuteJsTest({.params = base::Value("verifyNewInstance")});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testSwitchConversationToOldConversationInPlace) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ASSERT_OK(WaitForInstanceActive());
  ExecuteJsTest();
  ASSERT_OK(histogram_tester.WaitForBucketCount(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToNewInstance, 1));
  EXPECT_EQ("A", GetOnlyGlicInstance()->conversation_id().value_or(""));
  EXPECT_EQ("Title A", GetOnlyGlicInstance()->conversation_title());
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testSwitchConversationToNewConversationInPlace) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ASSERT_OK(WaitForInstanceActive());
  ExecuteJsTest();
  ASSERT_OK(histogram_tester.WaitForBucketCount(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kStartNewConversation, 1));
  EXPECT_EQ("", GetOnlyGlicInstance()->conversation_id().value_or(""));
  EXPECT_EQ("", GetOnlyGlicInstance()->conversation_title());
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testSwitchConversationToOldConversationNewInstance) {
  glic::GlicHistogramTester histogram_tester;
  auto* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* initial_instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForInstanceActive());
  ExecuteJsTest();
  ASSERT_OK(histogram_tester.WaitForBucketCount(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToNewInstance, 1));
  EXPECT_EQ("initial_id", initial_instance->conversation_id().value_or(""));
  EXPECT_EQ("Initial Title", initial_instance->conversation_title());

  auto initial_id = initial_instance->id();
  ContinueJsTest(
      {.expect_guest_frame_destroyed = true, .instance = initial_instance});
  ASSERT_OK(histogram_tester.WaitForBucketCount(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToNewInstance, 2));
  ASSERT_OK_AND_ASSIGN(auto* new_instance,
                       WaitForInstanceWithConversationId(active_tab, "A"));
  EXPECT_NE(new_instance->id(), initial_id);
  EXPECT_EQ("Title A", new_instance->conversation_title());
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testSwitchConversationToNewConversationNewInstance) {
  glic::GlicHistogramTester histogram_tester;
  auto* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* initial_instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForInstanceActive());
  ExecuteJsTest();
  ASSERT_OK(histogram_tester.WaitForBucketCount(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToNewInstance, 1));
  EXPECT_EQ("initial_id", initial_instance->conversation_id().value_or(""));
  EXPECT_EQ("Initial Title", initial_instance->conversation_title());

  auto initial_id = initial_instance->id();
  ContinueJsTest(
      {.expect_guest_frame_destroyed = true, .instance = initial_instance});
  ASSERT_OK(histogram_tester.WaitForBucketCount(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kStartNewConversation, 1));
  ASSERT_OK_AND_ASSIGN(auto* new_instance,
                       WaitForInstanceWithConversationId(active_tab, ""));
  EXPECT_NE(new_instance->id(), initial_id);
  EXPECT_EQ("", new_instance->conversation_title());
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testTabSwitchDoesNotLogActivationMetric \
  DISABLED_testTabSwitchDoesNotLogActivationMetric
#else
#define MAYBE_testTabSwitchDoesNotLogActivationMetric \
  testTabSwitchDoesNotLogActivationMetric
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       MAYBE_testTabSwitchDoesNotLogActivationMetric) {
  glic::GlicHistogramTester histogram_tester;
  // Save first tab (already active and navigated in SetUpOnMainThread).
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();

  // Open Glic for the first tab.
  ASSERT_OK(OpenGlicForActiveTab());
  GlicInstanceImpl* tab0_instance = GetInstanceForTab(first_tab);
  ASSERT_TRUE(tab0_instance);

  ExecuteJsTest({.params = base::Value("first"), .instance = tab0_instance});

  // Open a second tab and navigate it.
  tabs::TabInterface* second_tab =
      CreateAndActivateTab(GetTestUrl("page.html"));

  // Open Glic for the second tab.
  ASSERT_OK(OpenGlicForActiveTab());
  GlicInstanceImpl* tab1_instance = GetInstanceForTab(second_tab);
  ASSERT_TRUE(tab1_instance);

  ExecuteJsTest({.params = base::Value("second"), .instance = tab1_instance});

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return coordinator().GetInstances().size() == 1u; }));
  ASSERT_EQ("A", GetOnlyGlicInstance()->conversation_id());

  // Switch back to the first tab.
  ActivateTab(first_tab);

  // The original switch to tab 2 deactivated the first instance so this is
  // expected to log once when we reactivate the first instance in tab 2.
  histogram_tester.ExpectTotalCount("Glic.Instance.TimeSinceLastActive", 1);

  // Switch back to the second tab.
  ActivateTab(second_tab);

  // active instance switching to the same instance in a new tab DOES NOT log
  // the TimeSinceLastActive metric.
  histogram_tester.ExpectTotalCount("Glic.Instance.TimeSinceLastActive", 1);

  ContinueJsTest({.instance = GetOnlyGlicInstance()});
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testDetachDoesNotLogActivationMetric \
  DISABLED_testDetachDoesNotLogActivationMetric
#else
#define MAYBE_testDetachDoesNotLogActivationMetric \
  testDetachDoesNotLogActivationMetric
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       MAYBE_testDetachDoesNotLogActivationMetric) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest({.params = base::Value("registerAndDetach")});

  // Verify conversation ID.
  ASSERT_EQ("A", GetOnlyGlicInstance()->conversation_id());

  // Verify no spurious activation metric.
  histogram_tester.ExpectTotalCount("Glic.Instance.TimeSinceLastActive", 0);
}

#if defined(NOT_VETTED_ON_ANDROID)
#define MAYBE_testDetachPanelNoFloatyOrLiveMode \
  DISABLED_testDetachPanelNoFloatyOrLiveMode
#else
#define MAYBE_testDetachPanelNoFloatyOrLiveMode \
  testDetachPanelNoFloatyOrLiveMode
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestNoFloatyOrLiveMode,
                       MAYBE_testDetachPanelNoFloatyOrLiveMode) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicApiTestWithWebActuationSettingEnabled : public GlicApiTest {
 public:
  GlicApiTestWithWebActuationSettingEnabled() {
    feature_list_.InitWithFeatures({features::kGlicWebActuationSetting}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithWebActuationSettingEnabled,
                       testGetWebActuationSetting) {
  service()->enabling().SetUserEnabledActuationOnWeb(false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  service()->enabling().SetUserEnabledActuationOnWeb(true);
  ContinueJsTest();
}

class GlicApiTestWithWebActuationSettingDisabled : public GlicApiTest {
 public:
  GlicApiTestWithWebActuationSettingDisabled() {
    feature_list_.InitWithFeatures({}, {features::kGlicWebActuationSetting});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithWebActuationSettingDisabled,
                       testWebActuationSettingIsUndefinedWhenFeatureDisabled) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicApiTestWithProcessCounterAbuseVerdictDisabled : public GlicApiTest {
 public:
  GlicApiTestWithProcessCounterAbuseVerdictDisabled() {
    feature_list_.InitAndDisableFeature(
        features::kGlicProcessCounterAbuseVerdict);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    GlicApiTestWithProcessCounterAbuseVerdictDisabled,
    testProcessCounterAbuseVerdictIsUndefinedWhenFeatureDisabled) {
  ASSERT_OK(OpenGlicForActiveTab());
}

class GlicApiTestWithMqlsIdGetterDisabled : public GlicApiTest {
 public:
  GlicApiTestWithMqlsIdGetterDisabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {},
        /*disabled_features=*/
        {mojom::features::kGlicAppendModelQualityClientId});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithMqlsIdGetterDisabled,
                       testGetModelQualityClientIdFeatureDisabled) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicApiTestWithMqlsIdGetterEnabled : public GlicApiTest {
 public:
  GlicApiTestWithMqlsIdGetterEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {mojom::features::kGlicAppendModelQualityClientId},
        /*disabled_features=*/
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithMqlsIdGetterEnabled,
                       testGetModelQualityClientIdFeatureEnabled) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicApiTestWithCachedUserProfile : public GlicApiTest {
 public:
  GlicApiTestWithCachedUserProfile() {
    feature_list_.InitAndEnableFeature(
        features::kGlicEnableCachedGetUserProfileInfo);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithCachedUserProfile,
                       testGetUserProfileInfoCached) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicApiTestRuntimeFeatureOff : public GlicApiTest {
 public:
  GlicApiTestRuntimeFeatureOff() {
    with_feature_off_.InitAndDisableFeature(
        mojom::features::kGlicAppendModelQualityClientId);
  }

 private:
  base::test::ScopedFeatureList with_feature_off_;
};

// This tests what happens when a mojom RuntimeFeature method is called by
// the host.
// DONT DELETE THIS TEST when the method being called here is removed,
// but instead update this test to call any other RuntimeFeature-protected
// method.
IN_PROC_BROWSER_TEST_P(GlicApiTestRuntimeFeatureOff,
                       testErrorShownOnMojoPipeError) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ExecuteJsTest();

  auto* web_contents = instance->host().webui_contents();
  ASSERT_TRUE(web_contents);

  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kError));

  // Verify the reload button works.
  ASSERT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('#reload').click();"));

  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kReady));
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testGetUserProfileInfoDoesNotDeferWhenInactive) {
  ASSERT_OK(OpenGlicForActiveTab());
  PreventDeletionOnClose();
  ExecuteJsTest();
}
IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetTabById) {
  ASSERT_OK(OpenGlicForActiveTab());
  tabs::TabInterface* new_tab = CreateBackgroundTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  auto tab_id = new_tab->GetHandle();
  ExecuteJsTest(
      {.params = base::Value(base::NumberToString(tab_id.raw_value()))});

  // Navigate the tab.
  GURL::Replacements replacements;
  replacements.SetQueryStr("q=hi");
  ASSERT_TRUE(content::NavigateToURL(new_tab->GetContents(),
                                     embedded_test_server()
                                         ->GetURL("/browser_tests/test.html")
                                         .ReplaceComponents(replacements)));
  ContinueJsTest();

  // Close the tab.
  GetTabListInterface()->CloseTab(new_tab->GetHandle());
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetTabByIdWithDiscard) {
  ASSERT_OK(OpenGlicForActiveTab());
  tabs::TabInterface* new_tab = CreateBackgroundTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  auto tab_id = new_tab->GetHandle();
  ExecuteJsTest(
      {.params = base::Value(base::NumberToString(tab_id.raw_value()))});

  // Discard the tab.
  content::WebContents* new_contents_ptr =
      GetTabListInterface()->DiscardTab(new_tab->GetHandle());
  ASSERT_TRUE(new_contents_ptr);

  // Navigate the new contents.
  GURL::Replacements replacements;
  replacements.SetQueryStr("q=hi");
  ASSERT_TRUE(content::NavigateToURL(new_contents_ptr,
                                     embedded_test_server()
                                         ->GetURL("/browser_tests/test.html")
                                         .ReplaceComponents(replacements)));
  ContinueJsTest();

  // Close the tab.
  GetTabListInterface()->CloseTab(new_tab->GetHandle());
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testPinTabs) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testOpenPinnedTabPicker) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testPinTabsFailsWhenDoesNotExist) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testPinTabsStatePersistWhenClientRestarts) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  GlicClientConnectionObserver connection_observer(instance);
  // Open second tab in background.
  tabs::TabInterface* second_tab = CreateBackgroundTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  const int tab_id = second_tab->GetHandle().raw_value();

  ExecuteJsTest(
      {.params = base::Value(base::DictValue()
                                 .Set("tabId", base::NumberToString(tab_id))
                                 .Set("isFirstRun", true))});

  instance->host().Reload();
  ASSERT_OK(connection_observer.WaitForDisconnected());

  ExecuteJsTest(
      {.params = base::Value(base::DictValue().Set("isFirstRun", false))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testPinTabsStatePersistWhenClosePanelAndReopen) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  PreventDeletionOnClose(instance);
  // Open second tab in background.
  tabs::TabInterface* second_tab = CreateBackgroundTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  const int tab_id = second_tab->GetHandle().raw_value();

  ExecuteJsTest({.params = base::Value(base::DictValue().Set(
                     "tabId", base::NumberToString(tab_id)))});

  // Glic UI was closed by JS, wait for it.
  ASSERT_OK(WaitForGlicClose());

  // Re-open Glic.
  ASSERT_OK(OpenGlicForActiveTab());

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testUnpinTabsFailsWhenNotPinned) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  // Open second tab in background.
  tabs::TabInterface* second_tab = CreateBackgroundTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  instance->GetSharingManager()->PinTabs({second_tab->GetHandle()});
  const int tab_id = second_tab->GetHandle().raw_value();

  ExecuteJsTest({.params = base::Value(base::DictValue().Set(
                     "tabId", base::NumberToString(tab_id)))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testUnpinAllTabs) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  // Open second tab in background.
  tabs::TabInterface* second_tab = CreateBackgroundTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  instance->GetSharingManager()->PinTabs({second_tab->GetHandle()});

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testUnpinTabsThatNavigateInBackground) {
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  const int first_tab_id = first_tab->GetHandle().raw_value();

  // Navigate first_tab to a.com to set up the starting origin.
  NavigateTab(*first_tab,
              embedded_test_server()->GetURL("a.com", "/test_data/page.html"));

  // Open second tab and activate it.
  CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));

  // Open Glic on the active tab (second_tab).
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  PreventDeletionOnClose(instance);

  // Run the JS test, passing first_tab_id.
  ExecuteJsTest({.params = base::Value(base::DictValue().Set(
                     "tabId", base::NumberToString(first_tab_id)))});

  // Navigate first_tab (background) to b.com.
  // Glic window is open, so it should stay pinned.
  NavigateTab(*first_tab, embedded_test_server()->GetURL(
                              "b.com", "/browser_tests/test.html"));

  // Continue to close panel.
  ContinueJsTest();

  // Glic UI was closed by JS, wait for it to be fully closed.
  ASSERT_OK(WaitForGlicClose());

  // Navigate first_tab (background) to a.com.
  // Glic window is closed, so it should be unpinned.
  NavigateTab(*first_tab,
              embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Continue to check results.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextDisabled,
                       testGetContextFromTabIgnorePermissionWhenPinned) {
  ASSERT_OK(OpenGlicForActiveTab());
  glic::GlicHistogramTester histogram_tester;
  ExecuteJsTest();
  histogram_tester.ExpectBucketCount(
      "Glic.Api.GetContextFromTab.Error.Text",
      GlicGetContextFromTabError::kPermissionDeniedContextPermissionNotEnabled,
      1);
  histogram_tester.ExpectTotalCount("Glic.Api.GetContextFromTab.Error.Text", 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextDisabled,
                       testGetContextFromTabFailDifferentlyBasedOnPermission) {
  // For unfocused unpinned tabs, getTabContext calls fail with different error
  // messages based on context sharing permission state.
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  const int first_tab_id = first_tab->GetHandle().raw_value();

  // Create second tab and activate it, so first_tab goes to background.
  CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));

  ASSERT_OK(OpenGlicForActiveTab());
  glic::GlicHistogramTester histogram_tester;

  ExecuteJsTest({.params = base::Value(base::DictValue().Set(
                     "tabId", base::NumberToString(first_tab_id)))});

  // Two different permission errors should have been reported.
  histogram_tester.ExpectBucketCount(
      "Glic.Api.GetContextFromTab.Error.Text",
      GlicGetContextFromTabError::kPermissionDeniedContextPermissionNotEnabled,
      1);
  histogram_tester.ExpectBucketCount(
      "Glic.Api.GetContextFromTab.Error.Text",
      GlicGetContextFromTabError::kPermissionDenied, 1);
  histogram_tester.ExpectTotalCount("Glic.Api.GetContextFromTab.Error.Text", 2);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextDisabled,
                       testGetContextFromTabFailsIfNotPinned) {
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  const int first_tab_id = first_tab->GetHandle().raw_value();

  // Create second tab and activate it, so first_tab goes to background.
  CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));

  ASSERT_OK(OpenGlicForActiveTab());
  glic::GlicHistogramTester histogram_tester;

  ExecuteJsTest({.params = base::Value(base::DictValue().Set(
                     "tabId", base::NumberToString(first_tab_id)))});

  // Should have one error logged for tab context permission not granted.
  histogram_tester.ExpectBucketCount(
      "Glic.Api.GetContextFromTab.Error.Text",
      GlicGetContextFromTabError::kPermissionDeniedContextPermissionNotEnabled,
      1);
  histogram_tester.ExpectTotalCount("Glic.Api.GetContextFromTab.Error.Text", 1);
}

// Glic floaty/detached and live (Audio) modes are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextDisabled,
                       testGetContextFromFocusedTabWithoutPermission) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndDetach());
  // Wait for the panel opening handshake to complete. Otherwise, the delayed
  // startup mode (TEXT) response from the guest can race with and overwrite
  // the test's runtime mode switch (AUDIO), causing metrics flakiness.
  ASSERT_OK(WaitForPanelWillOpenComplete(instance));
  glic::GlicHistogramTester histogram_tester;
  ExecuteJsTest();

  ASSERT_OK(histogram_tester.WaitForBucketCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Audio",
      GlicGetContextFromTabError::kPermissionDeniedContextPermissionNotEnabled,
      1));
  histogram_tester.ExpectTotalCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Audio", 1);
}
#endif

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetContextFromTabFailsIfDoesNotExist) {
  ASSERT_OK(OpenGlicForActiveTab());
  glic::GlicHistogramTester histogram_tester;
  ExecuteJsTest();
  histogram_tester.ExpectBucketCount("Glic.Api.GetContextFromTab.Error.Text",
                                     GlicGetContextFromTabError::kTabNotFound,
                                     1);
  histogram_tester.ExpectTotalCount("Glic.Api.GetContextFromTab.Error.Text", 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testGetContextFromPinnedTabWithoutPermission) {
  ASSERT_OK(OpenGlicForActiveTab());
  glic::GlicHistogramTester histogram_tester;
  ExecuteJsTest();
  EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromTab.Error"),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testGetContextForActorFromTabWithoutPermission) {
  ASSERT_OK(OpenGlicForActiveTab());
  glic::GlicHistogramTester histogram_tester;
  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix(
                  "Glic.Api.GetContextForActorFromTab.Error"),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testGetContextForActorFromTabWithRestrictedUrl) {
  // Navigate to an un-focusable internal page.
  tabs::TabInterface* tab0 = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab0);
  NavigateTab(*tab0, GURL(chrome::kChromeUIVersionURL));
  ASSERT_OK(OpenGlicForActiveTab());
  glic::GlicHistogramTester histogram_tester;
  ExecuteJsTest();

  // Checks that the correct error was reported.
  histogram_tester.ExpectBucketCount(
      "Glic.Api.GetContextForActorFromTab.Error.Text",
      GlicGetContextFromTabError::kPermissionDenied, 1);
  histogram_tester.ExpectTotalCount(
      "Glic.Api.GetContextForActorFromTab.Error.Text", 1);
}

// Note: PDF support is a necessary precondition for this test.
#if BUILDFLAG(ENABLE_PDF)
#define MAYBE_testGetContextFromFocusedTabWithPdfFile \
  testGetContextFromFocusedTabWithPdfFile
#else
#define MAYBE_testGetContextFromFocusedTabWithPdfFile \
  DISABLED_testGetContextFromFocusedTabWithPdfFile
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       MAYBE_testGetContextFromFocusedTabWithPdfFile) {
  tabs::TabInterface* tab0 = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab0);
  NavigateTab(*tab0, embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_OK(OpenGlicForActiveTab());
  glic::GlicHistogramTester histogram_tester;
  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromFocusedTab.Error"),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testGetContextFromFocusedTabWithUnFocusablePage) {
  tabs::TabInterface* tab0 = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab0);
  NavigateTab(*tab0, GURL(chrome::kChromeUIVersionURL));
  ASSERT_OK(OpenGlicForActiveTab());
  glic::GlicHistogramTester histogram_tester;
  ExecuteJsTest();

  // Checks that the correct error was reported.
  histogram_tester.ExpectBucketCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Text",
      GlicGetContextFromTabError::kPermissionDenied, 1);
  histogram_tester.ExpectTotalCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Text", 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testGetContextFromFocusedTabWithNoRequestedData) {
  tabs::TabInterface* tab0 = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab0);
  NavigateTab(*tab0,
              embedded_test_server()->GetURL("/glic/browser_tests/test.html"));
  ASSERT_OK(OpenGlicForActiveTab());
  glic::GlicHistogramTester histogram_tester;
  ExecuteJsTest();

  // No error should be logged to the text histogram.
  histogram_tester.ExpectTotalCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Text", 0);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testPinTabsFailsWhenIncognitoWindow) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());

  // Open a new incognito window.
  BrowserWindowInterface* incognito =
      PlatformBrowserTest::CreateIncognitoBrowser();
  tabs::TabInterface* incognito_tab =
      TabListInterface::From(incognito)->OpenTab(
          embedded_test_server()->GetURL("/browser_tests/test.html"), 1,
          /*foreground=*/true);
  ASSERT_TRUE(incognito_tab);
  auto incognito_tab_id = incognito_tab->GetHandle().raw_value();

  ExecuteJsTest(
      {.params = base::Value(base::DictValue().Set(
           "incognitoTabId", base::NumberToString(incognito_tab_id)))});
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
class GlicApiTestWithFileUploadPolicyEnabled : public GlicApiTest {
 public:
  GlicApiTestWithFileUploadPolicyEnabled() {
    feature_list_.InitWithFeatures({features::kGlicDragAndDropFileUpload,
                                    features::kGlicWebDragAndDropFileUpload},
                                   {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithFileUploadPolicyEnabled,
                       testGetFileUploadAllowedCapability) {
  // Set default state to allowed
  GetProfile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicFileUploadAllowed,
      std::to_underlying(glic::prefs::GlicFileUploadPolicyState::kEnabled));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  // Change pref to disabled
  GetProfile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicFileUploadAllowed,
      std::to_underlying(glic::prefs::GlicFileUploadPolicyState::kDisabled));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetFocusedTabStateV2BrowserClosed) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  ExecuteJsTest();
  CloseMainBrowserWithIncognitoKeepAlive();
  ContinueJsTest();
}

// TODO(b/545187457): Figure out if this test really needs detached mode.
IN_PROC_BROWSER_TEST_P(GlicApiTest, testUnpinTabsWhileClosing) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  ExecuteJsTest();
}

// TODO(b/545187457): Figure out if this test really needs detached mode.
IN_PROC_BROWSER_TEST_P(GlicApiTest, testPinTabsWithTwoTabs) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  ExecuteJsTest();

  // Step 2: Open a second tab in foreground.
  tabs::TabInterface* second_tab = CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  ContinueJsTest();

  // Step 3: Close the second tab.
  GetTabListInterface()->CloseTab(second_tab->GetHandle());
  ContinueJsTest();
}
#endif

class GlicApiTestWithContextualCueing : public GlicApiTest {
 public:
  GlicApiTestWithContextualCueing() {
    contextual_cueing_features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicWebClientLoadTimes,
          {
              // Shorten load timeouts.
              {features::kGlicPreLoadingTimeMs.name, "20"},
              {features::kGlicMinLoadingTimeMs.name, "40"},
          }},
         {kGlicZeroStateSuggestions, {}},
         {kContextualCueing, {}}},
        /*disabled_features=*/
        {});
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* browser_context) override {
#if BUILDFLAG(IS_CHROMEOS)
    if (!ash::IsUserBrowserContext(browser_context)) {
      return;
    }
#endif
    fake_cueing_service_ = static_cast<FakeContextualCueingService*>(
        ContextualCueingServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeContextualCueingService>();
            })));

    GlicApiTest::SetUpBrowserContextKeyedServices(browser_context);
  }

  void TearDownOnMainThread() override {
    fake_cueing_service_ = nullptr;
    GlicApiTest::TearDownOnMainThread();
  }

  FakeContextualCueingService* fake_cueing_service() {
    return fake_cueing_service_;
  }

 private:
  raw_ptr<FakeContextualCueingService> fake_cueing_service_;
  base::test::ScopedFeatureList contextual_cueing_features_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithContextualCueing,
                       testGetZeroStateSuggestionsForFocusedTabApi) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  EXPECT_GE(fake_cueing_service()->focused_tab_call_count(), 1);
}

IN_PROC_BROWSER_TEST_P(
    GlicApiTestWithContextualCueing,
    testGetZeroStateSuggestionsForFocusedTabFailsWhenHidden) {
  ASSERT_OK(OpenGlicForActiveTab());
  PreventDeletionOnClose();
  ExecuteJsTest();
}

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/533085229): Re-enable on Android once close flakiness is
// fixed.
#define MAYBE_testNoZssWarmingStateMachine DISABLED_testNoZssWarmingStateMachine
#else
#define MAYBE_testNoZssWarmingStateMachine testNoZssWarmingStateMachine
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithContextualCueing,
                       MAYBE_testNoZssWarmingStateMachine) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  // 1. Initial Open via Blocked Source (kPromotionPage) -> disables warming.
  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kPromotionPage);
  ASSERT_OK(WaitForGlicOpen());

  GlicInstanceImpl* instance = coordinator().GetInstanceImplForTab(tab1);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 0);

  // 2. Simulate showing Glic again after closing via an explicit source
  // (kTopChromeButton) -> resets zss_warming_enabled_ = true and runs
  // warming.
  PreventBlankDeletionOnClose(instance);
  instance->CloseAllEmbedders();
  ASSERT_OK(WaitForGlicClose());

  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kTopChromeButton);
  ASSERT_OK(WaitForGlicOpen());
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 1);

  // 3. If the web client explicitly requests ZSS, it should still get
  // results.
  ExecuteJsTest();
  // The JS request makes another call to the backend service, bringing the
  // total call count to 2.
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 2);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithContextualCueing,
                       testNoZssWarmingStateMachineImplicitPreservesDisabled) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  // 1. Initial Open via Blocked Source (kPromotionPage) -> disables warming.
  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kPromotionPage);
  ASSERT_OK(WaitForGlicOpen());

  GlicInstanceImpl* instance = coordinator().GetInstanceImplForTab(tab1);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 0);

  // 2. Simulate showing Glic again after closing via an implicit source
  // (kTabRestore) -> preserves disabled state (zss_warming_enabled_ == false).
  PreventBlankDeletionOnClose(instance);
  instance->CloseAllEmbedders();
  ASSERT_OK(WaitForGlicClose());

  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kTabRestore);
  ASSERT_OK(WaitForGlicOpen());
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 0);

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithContextualCueing,
                       testNoZssWarmingStateMachineImplicitPreservesEnabled) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  // 1. Initial Open via Explicit Source (kTopChromeButton) -> warming enabled.
  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kTopChromeButton);
  ASSERT_OK(WaitForGlicOpen());

  GlicInstanceImpl* instance = coordinator().GetInstanceImplForTab(tab1);
  ASSERT_NE(instance, nullptr);
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 1);

  // 2. Simulate showing Glic again after closing via an implicit source
  // (kTabRestore) -> preserves enabled state (zss_warming_enabled_ == true)
  // and runs warming on open.
  PreventBlankDeletionOnClose(instance);
  instance->CloseAllEmbedders();
  ASSERT_OK(WaitForGlicClose());

  coordinator().Toggle(GetBrowser(), /*prevent_close=*/true,
                       mojom::InvocationSource::kTabRestore);
  ASSERT_OK(WaitForGlicOpen());
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), 2);

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithContextualCueing,
                       testGetZeroStateSuggestionsApi) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  EXPECT_EQ(fake_cueing_service()->pinned_tabs_call_count(), 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithContextualCueing,
                       testGetZeroStateSuggestionsUnsubscribeAndResubscribe) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  EXPECT_EQ(fake_cueing_service()->pinned_tabs_call_count(), 1);
}

// This test doesn't work for multi-instance.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithContextualCueing,
                       testGetZeroStateSuggestionsMultipleNavigations) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

// This test doesn't work for multi-instance.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithContextualCueing,
                       testGetZeroStateSuggestionsFailsWhenHidden) {
  ASSERT_OK(OpenGlicForActiveTab());
  PreventDeletionOnClose(nullptr);
  ExecuteJsTest();

  int initial_calls = fake_cueing_service()->focused_tab_call_count();

  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      GetTestUrl("page.html?new")));

  ContinueJsTest();
  EXPECT_EQ(fake_cueing_service()->focused_tab_call_count(), initial_calls);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithWebContentsWarming,
                       testWebClientReadyOnPreload) {
  auto container =
      coordinator().GetWebContentsWarmingPoolForTesting().TakeContainer();
  ASSERT_TRUE(container);
  auto* web_contents = container->active_web_contents();

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

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCreateTabSimple) {
  ASSERT_OK(OpenGlicForActiveTab());
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 1);

  ExecuteJsTest();
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 2);

  // The new tab should be active.
  EXPECT_EQ(GetTabListInterface()->GetActiveIndex(), 1);
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(active_tab);
  ASSERT_THAT(active_tab->GetContents()->GetURL().spec(),
              testing::EndsWith("#simple"));
}

// TODO(harringtond): Fix and re-enable on Android.
// Redundant tab deactivation and reactivation during test tab setup forces
// Glic to kPeeked/inactive state on Android, which disables Glic API's
// foreground requests (background request gating intercepts and returns empty
// response). Also, tab group inheritance is not supported by default on
// Android.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_testActivateTabWithUrl DISABLED_testActivateTabWithUrl
#else
#define MAYBE_testActivateTabWithUrl testActivateTabWithUrl
#endif

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_testCreateTab DISABLED_testCreateTab
#define MAYBE_testCreateTabInBackground DISABLED_testCreateTabInBackground
#else
#define MAYBE_testCreateTab testCreateTab
#define MAYBE_testCreateTabInBackground testCreateTabInBackground
#endif

IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testActivateTabWithUrl) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testCreateTab) {
  ASSERT_OK(OpenGlicForActiveTab());
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 1);

  // Add a tab after the active tab to ensure we're not just appending.
  tabs::TabInterface* second_tab =
      GetTabListInterface()->OpenTab(GURL("about:blank"), -1);
  ASSERT_TRUE(second_tab);

  tabs::TabInterface* first_tab = GetTabListInterface()->GetTab(0);
  ASSERT_TRUE(first_tab);
  GetTabListInterface()->ActivateTab(first_tab->GetHandle());

  std::optional<tab_groups::TabGroupId> group_id =
      GetTabListInterface()->CreateTabGroup({first_tab->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  ExecuteJsTest();
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 3);

  // The new tab should be at index 1 (next to the active tab).
  EXPECT_EQ(GetTabListInterface()->GetActiveIndex(), 1);

  // The new tab should inherit the tab group.
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(active_tab);
  EXPECT_EQ(active_tab->GetGroup(), group_id);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCreateTabFailsWithUnsupportedScheme) {
  ASSERT_OK(OpenGlicForActiveTab());
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 1);
  ExecuteJsTest();
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithBlankInstanceDelay,
                       testNoRemoveBlankInstanceOnCloseIfInputSubmitted) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ExecuteJsTest();

  // Close Glic to trigger the blank instance removal check.
  instance->CloseAllEmbedders();
  ASSERT_OK(WaitForGlicClose());

  // Wait for the blank instance removal timer (configured to 100ms in this
  // suite).
  {
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, loop.QuitClosure(), base::Milliseconds(200));
    loop.Run();
  }

  // The instance should still exist and match the original.
  EXPECT_EQ(GetOnlyGlicInstance(), instance);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetImageBytesFromTab) {
  // 1. Open glic.
  ASSERT_OK(OpenGlicForActiveTab());

  content::WebContents* web_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(web_contents);

  // 2. Inject image into the page and wait for load.
  const std::string kImageBase64 =
      "R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";
  ASSERT_TRUE(
      content::EvalJs(web_contents, base::StringPrintf(R"js(
    new Promise(resolve => {
      const img = document.createElement('img');
      img.src = 'data:image/gif;base64,%s';
      img.alt = 'test_image_bytes';
      img.onload = () => resolve(true);
      document.body.appendChild(img);
      if (img.complete) {
        resolve(true);
      }
    });
  )js",
                                                       kImageBase64.c_str()))
          .ExtractBool());

  // Wait for rendering to sync.
  {
    base::test::TestFuture<bool> future;
    web_contents->GetPrimaryMainFrame()
        ->GetRenderWidgetHost()
        ->InsertVisualStateCallback(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // 3. Fetch AIPageContent to extract the DOM node ID of the image.
  base::test::TestFuture<optimization_guide::AIPageContentResultOrError>
      content_future;
  optimization_guide::GetAIPageContent(
      web_contents,
      optimization_guide::ActionableAIPageContentOptions(
          /*on_critical_path =*/true),
      content_future.GetCallback());

  auto result = content_future.Take();
  ASSERT_TRUE(result.has_value());

  const optimization_guide::proto::ContentNode* image_node =
      optimization_guide::FindFirstNodeWithAttributeType(
          result->proto.root_node(),
          optimization_guide::proto::CONTENT_ATTRIBUTE_IMAGE);
  ASSERT_TRUE(image_node);
  int32_t dom_node_id =
      image_node->content_attributes().common_ancestor_dom_node_id();
  ASSERT_NE(dom_node_id, 0);

  // 4. Get the document identifier.
  std::optional<std::string> document_identifier =
      optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
          web_contents->GetPrimaryMainFrame()->GetGlobalFrameToken());
  ASSERT_TRUE(document_identifier.has_value());

  const int tab_id = GetTabId(web_contents);

  GlicHistogramTester histogram_tester;

  ExecuteJsTest(
      {.params = base::Value(base::DictValue()
                                 .Set("tabId", base::NumberToString(tab_id))
                                 .Set("documentId", *document_identifier)
                                 .Set("domNodeId", dom_node_id))});

  histogram_tester.ExpectBucketCount("Glic.Api.GetImageBytesFromTab.Error",
                                     GlicGetContextFromTabError::kUnknown, 1);
  histogram_tester.ExpectBucketCount("Glic.Api.GetImageBytesFromTab.Error",
                                     GlicGetContextFromTabError::kTabNotFound,
                                     1);
  histogram_tester.ExpectTotalCount("Glic.Api.GetImageBytesFromTab.Error", 2);
}

class GlicApiScrollToTest : public GlicApiTest {
 protected:
  TestResult<std::string> GetDocumentId() {
    content::RenderFrameHost* rfh = GetTabListInterface()
                                        ->GetActiveTab()
                                        ->GetContents()
                                        ->GetPrimaryMainFrame();
    std::optional<std::string> document_id =
        optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
            rfh->GetGlobalFrameToken());
    if (!document_id.has_value()) {
      return base::unexpected("No document ID found");
    }
    return base::ok(document_id.value());
  }
};

IN_PROC_BROWSER_TEST_P(GlicApiScrollToTest, testScrollToFindsText) {
  ASSERT_OK(OpenGlicForActiveTab());
  ASSERT_OK_AND_ASSIGN(std::string document_id, GetDocumentId());
  ExecuteJsTest({.params = base::Value(
                     base::DictValue().Set("documentId", document_id))});
}

IN_PROC_BROWSER_TEST_P(GlicApiScrollToTest,
                       testScrollToFindsTextNoTabContextPermission) {
  ASSERT_OK(OpenGlicForActiveTab());
  ASSERT_OK_AND_ASSIGN(std::string document_id, GetDocumentId());
  ExecuteJsTest({.params = base::Value(
                     base::DictValue().Set("documentId", document_id))});
}

IN_PROC_BROWSER_TEST_P(GlicApiScrollToTest, testScrollToFailsWhenInactive) {
  ASSERT_OK(OpenGlicForActiveTab());
  PreventDeletionOnClose();
  ASSERT_OK_AND_ASSIGN(std::string document_id, GetDocumentId());
  ExecuteJsTest({.params = base::Value(
                     base::DictValue().Set("documentId", document_id))});
}

IN_PROC_BROWSER_TEST_P(GlicApiScrollToTest, testScrollToNoMatchFound) {
  ASSERT_OK(OpenGlicForActiveTab());
  ASSERT_OK_AND_ASSIGN(std::string document_id, GetDocumentId());
  ExecuteJsTest({.params = base::Value(
                     base::DictValue().Set("documentId", document_id))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testCreateTabInBackground) {
  ASSERT_OK(OpenGlicForActiveTab());
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 1);

  // Add a tab after the active tab to ensure we're not just appending.
  tabs::TabInterface* second_tab =
      GetTabListInterface()->OpenTab(GURL("about:blank"), -1);
  ASSERT_TRUE(second_tab);

  tabs::TabInterface* first_tab = GetTabListInterface()->GetTab(0);
  ASSERT_TRUE(first_tab);
  GetTabListInterface()->ActivateTab(first_tab->GetHandle());

  std::optional<tab_groups::TabGroupId> group_id =
      GetTabListInterface()->CreateTabGroup({first_tab->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  // Creating a new tab via the glic API in the foreground should change the
  // active tab.
  ExecuteJsTest();
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 3);

  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(active_tab);
  ASSERT_THAT(active_tab->GetContents()->GetURL().spec(),
              testing::EndsWith("#foreground"));

  EXPECT_EQ(GetTabListInterface()->GetActiveIndex(), 1);
  EXPECT_EQ(active_tab->GetGroup(), group_id);

  // Creating a new tab via the glic API in the background should not change the
  // active tab.
  ContinueJsTest();
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 4);

  active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(active_tab);
  ASSERT_THAT(active_tab->GetContents()->GetURL().spec(),
              testing::EndsWith("#foreground"));

  EXPECT_EQ(GetTabListInterface()->GetActiveIndex(), 1);
  tabs::TabInterface* background_tab = GetTabListInterface()->GetTab(2);
  ASSERT_TRUE(background_tab);
  EXPECT_EQ(background_tab->GetGroup(), group_id);
}

// TODO(crbug.com/469210106): Re-enable this test on ChromeOS.
// TODO(crbug.com/508123456): Re-enable this test on Android once tab group
// inheritance is supported on Android's tab creation.
// TODO(crbug.com/515495117): Fix and re-enable this test on Mac.
// TODO(crbug.com/517282139): Fix and re-enable this test on Linux Msan.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER))
#define MAYBE_testCreateTabByClickingOnLink \
  DISABLED_testCreateTabByClickingOnLink
#else
#define MAYBE_testCreateTabByClickingOnLink testCreateTabByClickingOnLink
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testCreateTabByClickingOnLink) {
  ASSERT_OK(OpenGlicForActiveTab());
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 1);

  // Add a tab after the active tab to ensure we're not just appending.
  tabs::TabInterface* second_tab =
      GetTabListInterface()->OpenTab(GURL("about:blank"), -1);
  ASSERT_TRUE(second_tab);

  tabs::TabInterface* first_tab = GetTabListInterface()->GetTab(0);
  ASSERT_TRUE(first_tab);
  GetTabListInterface()->ActivateTab(first_tab->GetHandle());

  std::optional<tab_groups::TabGroupId> group_id =
      GetTabListInterface()->CreateTabGroup({first_tab->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  // Have the test track this tab's glic instance.
  ASSERT_OK_AND_ASSIGN(auto* guest_frame, WaitForGuest());
  ExecuteJsTest({.wait_for_guest = false});
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetTabListInterface()->GetTabCount() == 3;
  })) << "Timed out waiting for tab count to increase. Tab count = "
      << GetTabListInterface()->GetTabCount();
  // The guest frame shouldn't change.
  ASSERT_EQ(guest_frame, FindGlicGuestMainFrame());

  // Link click opens next to opener and inherits group.
  EXPECT_EQ(GetTabListInterface()->GetActiveIndex(), 1);
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(active_tab);
  EXPECT_EQ(active_tab->GetGroup(), group_id);

  // This test is a regression test for b/416464184.
  // Audio ducking should still work after clicking a link.
#if !BUILDFLAG(IS_ANDROID)
  AudioDucker* audio_ducker =
      AudioDucker::GetForPage(FindGlicGuestMainFrame()->GetPage());
  ASSERT_TRUE(audio_ducker);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return audio_ducker->GetAudioDuckingState() ==
           AudioDucker::AudioDuckingState::kDucking;
  }));
#endif

  ContinueJsTest();

#if !BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return audio_ducker->GetAudioDuckingState() ==
           AudioDucker::AudioDuckingState::kNoDucking;
  }));
#endif
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTestWithNewTabDaisyChain,
                       testCreateTabByClickingOnLinkDaisyChains) {
  ASSERT_OK(OpenGlicForActiveTab());
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), 1);

  ExecuteJsTest();
}
#endif

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTestWithNewTabDaisyChain,
                       testCanAttachPanelToFallbackEmbedder) {
  ASSERT_OK(OpenGlicForActiveTab());

  ExecuteJsTest();

  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(active_tab);
  GetTabListInterface()->CloseTab(active_tab->GetHandle());

  ContinueJsTest();
}
#endif

IN_PROC_BROWSER_TEST_P(GlicApiTestWithNewTabDaisyChain,
                       testDaisyChainRecursiveAndInput) {
  ASSERT_OK_AND_ASSIGN(auto* tab0_instance, OpenGlicForActiveTab());
  base::HistogramTester histogram_tester;

  // 1. Trigger "createTab" from the first tab's Glic panel.
  ExecuteJsTest(
      {.params = base::Value("createTab"), .instance = tab0_instance});

  // 2. Verify new tab opened and switch to it.
  ASSERT_OK(
      RunUntilEqual([&]() { return GetTabListInterface()->GetTabCount(); }, 2));
  tabs::TabInterface* tab1 = GetTabListInterface()->GetTab(1);
  ActivateTab(tab1);

  // 3. Wait for Glic to open in the new (second) tab.
  ASSERT_OK_AND_ASSIGN(auto* tab1_instance, WaitForGlicOpen(tab1));

  // 4. Verify no action yet.
  histogram_tester.ExpectTotalCount(
      "Glic.Instance.AutoOpenedPanel.FirstAction.GlicContents", 0);

  // 5. Trigger "createTab" (recursive) from the second tab's panel.
  ExecuteJsTest(
      {.params = base::Value("createTab"), .instance = tab1_instance});

  // 6. Verify third tab opened.
  ASSERT_OK(
      RunUntilEqual([&]() { return GetTabListInterface()->GetTabCount(); }, 3));
  tabs::TabInterface* tab2 = GetTabListInterface()->GetTab(2);
  ActivateTab(tab2);

  // 7. Verify recursive metric for the second tab (which was daisy chained).
  ASSERT_OK(RunUntilEqual(
      [&]() {
        return histogram_tester.GetBucketCount(
            "Glic.Instance.AutoOpenedPanel.FirstAction.GlicContents",
            DaisyChainFirstAction::kRecursiveDaisyChain);
      },
      1));

  // 8. Open Glic in the new (third) tab.
  ASSERT_OK_AND_ASSIGN(auto* tab2_instance, WaitForGlicOpen(tab2));

  // 9. Trigger "inputSubmitted" in the third tab's panel.
  ExecuteJsTest(
      {.params = base::Value("inputSubmitted"), .instance = tab2_instance});

  // 10. Verify inputSubmitted metric for the third tab.
  ASSERT_OK(RunUntilEqual(
      [&]() {
        return histogram_tester.GetBucketCount(
            "Glic.Instance.AutoOpenedPanel.FirstAction.GlicContents",
            DaisyChainFirstAction::kInputSubmitted);
      },
      1));
}
IN_PROC_BROWSER_TEST_P(GlicApiTestWithNewTabDaisyChain, testNewTabMetrics) {
  // 1. Open Glic in first tab.
  ASSERT_OK(OpenGlicForActiveTab());
  base::HistogramTester histogram_tester;

  // 2. Open a new tab (Ctrl+T equivalent).
  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("chrome://newtab/"));
  ASSERT_TRUE(tab1);

  // 3. Verify Glic is open in the new tab.
  ASSERT_OK_AND_ASSIGN(auto* tab1_instance, WaitForGlicOpen(tab1));

  // 4. Trigger "inputSubmitted".
  ExecuteJsTest(
      {.params = base::Value("inputSubmitted"), .instance = tab1_instance});

  // 5. Verify Metric.
  ASSERT_OK(RunUntilEqual(
      [&]() {
        return histogram_tester.GetBucketCount(
            "Glic.Instance.AutoOpenedPanel.FirstAction.NewTab",
            DaisyChainFirstAction::kInputSubmitted);
      },
      1));
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/520959831): Fix flaky test.
IN_PROC_BROWSER_TEST_P(GlicApiTest, DISABLED_testEnableDragResize) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  ASSERT_OK(WaitForGlicClient());
  ASSERT_OK(WaitUntilCanResize(false));
  ExecuteJsTest();
  ASSERT_OK(WaitUntilCanResize(true));
}
#endif

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/520824542): Fix flaky test.
IN_PROC_BROWSER_TEST_P(GlicApiTest, DISABLED_testDisableDragResize) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  ASSERT_OK(WaitUntilCanResize(true));
  ExecuteJsTest();
  ASSERT_OK(WaitUntilCanResize(false));
}
#endif

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testInitiallyNotResizable) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  ExecuteJsTest();
  ASSERT_OK(WaitUntilCanResize(false));
}
#endif

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testSetMinimumWidgetSize) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  ExecuteJsTest();
  ASSERT_TRUE(step_data().has_value() && step_data()->is_dict());
  const auto& min_size = step_data()->GetDict();
  const int width = min_size.FindInt("width").value();
  const int height = min_size.FindInt("height").value();

  auto expected_size = glic::GlicWidget::GetInitialSize();
  expected_size.SetToMax(gfx::Size(width, height));
  GlicWidget* glic_widget = GetGlicWidget();
  ASSERT_TRUE(glic_widget);
  ASSERT_OK(RunUntilEqual([&]() { return glic_widget->GetMinimumSize(); },
                          expected_size));

  ContinueJsTest();
}
#endif

// Detached floating window mode (Floaty) is not supported on Android, where
// Glic runs exclusively attached to tabs or side panels.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testPanelActiveWithMicrophone) {
  tabs::TabInterface* background_tab = CreateBackgroundTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndDetach());

  ExecuteJsTest();

  instance->host().OnMicrophoneStatusChanged(
      mojom::MicrophoneStatus::kListening);

  // Activating the other tab should take focus away from Floaty. Floaty should
  // still remain active because the microphone is listening.
  GetTabListInterface()->ActivateTab(background_tab->GetHandle());
  GetBrowserWindowInterface()->GetWindow()->Activate();
  EXPECT_TRUE(instance->IsActive());

  ContinueJsTest();

  // Pause the microphone and focus on the other tab. Floaty should not be
  // considered active.
  instance->host().OnMicrophoneStatusChanged(
      mojom::MicrophoneStatus::kNotListening);
  GetTabListInterface()->ActivateTab(background_tab->GetHandle());
  GetBrowserWindowInterface()->GetWindow()->Activate();

  ASSERT_TRUE(base::test::RunUntil([&]() { return !instance->IsActive(); }));

  ContinueJsTest();
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testManualResizeChanged) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  GlicWidget* glic_widget = GetGlicWidget();
  ASSERT_TRUE(glic_widget);
  glic_widget->OnNativeWidgetUserResizeStarted();

  // Check that the web client is notified of the beginning of the user
  // initiated resizing event.
  ExecuteJsTest();

  glic_widget->OnNativeWidgetUserResizeEnded();

  // Check that the web client is notified of the ending of the user
  // initiated resizing event.
  ContinueJsTest();
}
#endif

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testResizeWindowTooSmall) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  // Web client requests the window to be resized to 0x0, below the minimum
  // dimensions, so it gets discarded in favor of the initial size.
  gfx::Size expected_size = GlicWidget::GetInitialSize();
  GlicWidget* glic_widget = GetGlicWidget();
  ASSERT_TRUE(glic_widget);

  ExecuteJsTest();

  gfx::Rect final_widget_bounds = glic_widget->GetWindowBoundsInScreen();
  ASSERT_EQ(expected_size,
            glic_widget->WidgetToVisibleBounds(final_widget_bounds).size());
}
#endif

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testResizeWindowTooLarge) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  // Web client requests the window to be resized to 20000x20000, above the
  // maximum dimensions, so it gets discarded in favor of the max size. This max
  // size is still larger than the display work area so we clamp the dimensions
  // down to fit on screen.
  ExecuteJsTest();
  gfx::Rect display_bounds =
      display::Screen::Get()->GetPrimaryDisplay().work_area();
  GlicWidget* glic_widget = GetGlicWidget();
  ASSERT_TRUE(glic_widget);
  gfx::Rect final_widget_bounds = glic_widget->GetWindowBoundsInScreen();

  ASSERT_TRUE(display_bounds.Contains(final_widget_bounds));
}
#endif

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testResizeWindowWithinBounds) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  // Web client requests the window to be resized to 800x700, which are valid
  // dimensions.
  gfx::Size expected_size = gfx::Size(800, 700);
  ExecuteJsTest(
      {.params = base::Value(base::DictValue()
                                 .Set("width", expected_size.width())
                                 .Set("height", expected_size.height()))});
  GlicWidget* glic_widget = GetGlicWidget();
  ASSERT_TRUE(glic_widget);
  gfx::Rect final_widget_bounds = glic_widget->GetWindowBoundsInScreen();
  ASSERT_EQ(expected_size,
            glic_widget->WidgetToVisibleBounds(final_widget_bounds).size());
}
#endif

IN_PROC_BROWSER_TEST_P(GlicApiTest, testFailureForCapturedApiTestError) {
  ASSERT_OK(OpenGlicForActiveTab());
  const std::string expected_failure =
      "Failed at step #1 (single or first) due to (captured error): "
      "Error: Non-throwing test error";
  ExecuteJsTest(
      {.should_fail = true, .should_fail_with_error = expected_failure});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testShowClientErrorDialog) {
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

  if (service()->GetAuthController()) {
    if (base::FeatureList::IsEnabled(features::kGlicCookieSyncOnError) &&
        base::FeatureList::IsEnabled(features::kGlicCookieSyncOnTokenChange)) {
      // Sync will happen automatically if kGlicCookieSyncOnError is enabled.
      ASSERT_TRUE(base::test::RunUntil([&]() {
        return !service()->GetAuthController()->NeedsSyncForTesting();
      }));
    } else {
      // Verify that the pref was set to true.
      ASSERT_TRUE(base::test::RunUntil([&]() {
        return service()->GetAuthController()->NeedsSyncForTesting();
      }));
    }
  }
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testReportClientTransientError) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  // Wait for the histogram to be recorded.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetAllSamples("Glic.Api.Client.TransientError")
               .size() > 0;
  }));
  histogram_tester.ExpectUniqueSample("Glic.Api.Client.TransientError",
                                      /*kUnauthenticated*/ 16, 1);

  if (service()->GetAuthController()) {
    if (base::FeatureList::IsEnabled(features::kGlicCookieSyncOnError) &&
        base::FeatureList::IsEnabled(features::kGlicCookieSyncOnTokenChange)) {
      // Sync will happen automatically if kGlicCookieSyncOnError is enabled.
      ASSERT_TRUE(base::test::RunUntil([&]() {
        return !service()->GetAuthController()->NeedsSyncForTesting();
      }));
    } else {
      // Verify that the pref was set to true.
      ASSERT_TRUE(base::test::RunUntil([&]() {
        return service()->GetAuthController()->NeedsSyncForTesting();
      }));
    }
  }
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testLoadWhileWindowClosed) {
  // Open Glic
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicOpen());

  // Close Glic
  CloseAllEmbeddersAndPreventDeletion();
  ASSERT_OK(WaitForGlicClose());

  ExecuteJsTest();

  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testInitializeFailsWindowClosed) {
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicOpen());

  CloseAllEmbeddersAndPreventDeletion();
  ASSERT_OK(WaitForGlicClose());

  ExecuteJsTest();
}

// TODO(https://crbug.com/503936424): Flaky.
IN_PROC_BROWSER_TEST_P(GlicApiTest, DISABLED_testInitializeFailsWindowOpen) {
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

// TODO(crbug.com/409042450): This is a flaky on MSAN.
#if defined(SLOW_BINARY)
#define MAYBE_testReload DISABLED_testReload
#else
#define MAYBE_testReload testReload
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testReload) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  GlicClientConnectionObserver connection_observer(instance);
  ExecuteJsTest({
      .params = base::Value(
          base::DictValue().Set("failWith", "reloadAfterInitialize")),
  });
  ASSERT_OK(connection_observer.WaitForConnected());
  ASSERT_OK(connection_observer.WaitForDisconnected());
  ExecuteJsTest({
      .params = base::Value(base::DictValue().Set("failWith", "none")),
  });
}

#define MAYBE_testSorryPageBeforeInitialize testSorryPageBeforeInitialize
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testSorryPageBeforeInitialize) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ExecuteJsTest({
      .params = base::Value(base::DictValue().Set(
          "failWith", "navigateToSorryPageBeforeInitialize")),
  });
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kGuestError));
  ASSERT_TRUE(instance->IsShowing());

  auto* old_frame = FindGlicGuestMainFrame();
  ASSERT_TRUE(old_frame);
  content::GlobalRenderFrameHostId old_frame_id = old_frame->GetGlobalId();
  // Simulate completing a captcha, navigating back.
  ASSERT_EQ(true,
            content::EvalJs(old_frame,
                            std::string("(()=>{window.location = '") +
                                GetGuestURL().spec() + "'; return true;})()"));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    auto* frame = FindGlicGuestMainFrame();
    return frame && frame->GetGlobalId() != old_frame_id;
  }));
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kFinishLoading));
  ExecuteJsTest({
      .params = base::Value(base::DictValue().Set("failWith", "none")),
  });
}

#if defined(SLOW_BINARY)
// Flaky on slow builds.
#define MAYBE_testSorryPageAfterInitialize DISABLED_testSorryPageAfterInitialize
#else
#define MAYBE_testSorryPageAfterInitialize testSorryPageAfterInitialize
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testSorryPageAfterInitialize) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ExecuteJsTest({
      .params = base::Value(base::DictValue().Set(
          "failWith", "navigateToSorryPageAfterInitialize")),
  });
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kGuestError));
  ASSERT_TRUE(instance->IsShowing());

  auto* old_frame = FindGlicGuestMainFrame();
  ASSERT_TRUE(old_frame);
  content::GlobalRenderFrameHostId old_frame_id = old_frame->GetGlobalId();
  // Simulate completing a captcha, navigating back.
  ASSERT_EQ(true,
            content::EvalJs(old_frame,
                            std::string("(()=>{window.location = '") +
                                GetGuestURL().spec() + "'; return true;})()"));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    auto* frame = FindGlicGuestMainFrame();
    return frame && frame->GetGlobalId() != old_frame_id;
  })) << "Glic guest frame never changed.";
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kFinishLoading));
  ExecuteJsTest({
      .params = base::Value(base::DictValue().Set("failWith", "none")),
  });
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testInitializeFailsAfterReload) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  GlicClientConnectionObserver connection_observer(instance);
  ExecuteJsTest({
      .params = base::Value(
          base::DictValue().Set("failWith", "reloadAfterInitialize")),
  });
  ASSERT_OK(connection_observer.WaitForDisconnected());
  ExecuteJsTest({
      .params = base::Value(base::DictValue().Set("failWith", "error")),
  });
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kError));
}

// TODO(https://crbug.com/516659596): Re-enable on Linux debug builds.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || !defined(NDEBUG)
#define MAYBE_testNoClientCreated DISABLED_testNoClientCreated
#else
#define MAYBE_testNoClientCreated testNoClientCreated
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout, MAYBE_testNoClientCreated) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kError));
#endif
}

#if BUILDFLAG(IS_ANDROID) || defined(SLOW_BINARY) || BUILDFLAG(IS_LINUX)
#define MAYBE_testNoBootstrap DISABLED_testNoBootstrap
#else
#define MAYBE_testNoBootstrap testNoBootstrap
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout, MAYBE_testNoBootstrap) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kError));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout,
                       DISABLED_testInitializeTimesOut) {
#if defined(SLOW_BINARY) || !BUILDFLAG(IS_LINUX)
  GTEST_SKIP() << "skip timeout test is flaky on most bots";
#else
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest({
      .params = base::Value(base::DictValue().Set("failWith", "timeout")),
  });
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kError));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.Host.WebClientLifecycleEvent",
               GlicWebClientLifecycleEvent::kDisconnectedBeforeInitialization) >
           0;
  }));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount("Glic.PanelWebUiState.Error",
                                           5 /*TIMEOUT_WARMED*/) > 0;
  }));
  histogram_tester.ExpectTotalCount("Glic.PanelWebUiState.Error", 1);
#endif
}

// TODO(crbug.com/410881522): Re-enable this test
IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout,
                       DISABLED_testNavigateToBadPage) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  // Client loads, and navigates to a new URL. We try to load the client again,
  // but it fails.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  PreventDeletionOnClose(instance);
  WebUIStateListener listener(&instance->host());
  listener.WaitForWebUiState(mojom::WebUiState::kReady);

  ExecuteJsTest({.params = base::Value(
                     base::DictValue().Set("step", "trigger_navigation"))});
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  listener.WaitForWebUiState(mojom::WebUiState::kError);

  // Close Glic.
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));

  // Open Glic again. This time the client should load, falling back to the
  // original URL.
  ASSERT_OK_AND_ASSIGN(auto* reopened_instance, OpenGlicForActiveTab());
  ASSERT_EQ(reopened_instance, instance);
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(
                     base::DictValue().Set("step", "verify_fallback"))});
#endif
}

// TODO(crbug.com/508719420): Flaky time out.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout,
                       DISABLED_testNavigateToAboutBlank) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  WebUIStateListener listener(&instance->host());
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest();
  listener.WaitForWebUiState(mojom::WebUiState::kError);
#endif
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testPanelWillOpenBeforeClientReady) {
  // Open Glic to create an instance bound to the active tab, then hibernate it
  // to destroy the guest WebContents and disconnect the web client.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  instance->Hibernate();

  // Register conversation metadata while the instance is offline/hibernated.
  auto info = mojom::ConversationInfo::New();
  info->conversation_id = "test_conversation_id";
  info->conversation_title = "Test Conversation Title";
  info->client_data = "test_client_data_from_cc";
  instance->RegisterConversation(std::move(info), base::DoNothing());

  // Re-open Glic. This awakens the host and triggers NotifyPanelWillOpen with
  // the registered conversation before the newly created guest web client is
  // connected. ExecuteJsTest verifies the web client receives the queued data.
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicGetHostCapabilityApiTest : public GlicApiBrowserTest,
                                     public WithTestParams,
                                     public GlicApiTestPasskeys {
 public:
  GlicGetHostCapabilityApiTest()
      : GlicApiBrowserTest(GlicTestJsPath("./glic_api_browsertest.js")) {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlic, {}},
        {features::kGlicProcessCounterAbuseVerdict, {}},
        {features::kGlicWebContentsWarming,
         {{features::kGlicWebContentsWarmingDelay.name, "7d"}}},
        {features::kGlicRollout, {}},
        {mojom::features::kGlicMultiTab, {}},
        {features::kGlicWebActuationSetting, {}},
        {features::kGlicCaptureRegion, {}},
        {features::kGlicPopupWindowsEnabled, {}},
        {features::kLogJsConsoleMessages, {}},
        {features::kGlicUserStatusCheck,
         {{features::kGlicUserStatusRefreshApi.name, "true"},
          {features::kGlicUserStatusThrottleInterval.name, "2s"}}},
        {features::kGlicOpenPasswordManagerSettingsPageApi, {}},
        {features::kGlicOpenContactInfoSettingsPageApi, {}},
        {features::kGlicActor,
         {{features::kGlicActorPolicyControlExemption.name, "true"}}},
        {blink::features::kAIPageContentTrackedElementsIframe, {}},
    };

    std::vector<base::test::FeatureRef> disabled_features = {
        features::kGlicWarming,
        features::kGlicDaisyChainNewTabs,
        features::kGlicCountryFiltering,
        features::kGlicLocaleFiltering,
    };

    if (GetParam().enable_scroll_to_pdf) {
      enabled_features.push_back(
          {features::kGlicScrollTo, {{"glic-scroll-to-pdf", "true"}}});
    } else {
      disabled_features.push_back(features::kGlicScrollTo);
    }

    if (GetParam().auto_open_pdf) {
      enabled_features.push_back(
          {features::kAutoOpenGlicForPdf,
           {{"AutoOpenGlicForPdfWithOnboarding", "true"}}});
    }

    features_.InitWithFeaturesAndParameters(enabled_features,
                                            disabled_features);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(GlicGetHostCapabilityApiTest, testGetHostCapabilities) {
  tabs::TabInterface* tab0 = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab0);
  NavigateTab(*tab0, GetTestUrl("page.html"));

  base::ListValue expected_capabilities;
  if (GetParam().enable_scroll_to_pdf) {
#if BUILDFLAG(ENABLE_PDF)
    expected_capabilities.Append(
        std::to_underlying(mojom::HostCapability::kScrollToPdf));
#endif
  }
  if (GetParam().trust_first_onboarding_arm2) {
    expected_capabilities.Append(
        std::to_underlying(mojom::HostCapability::kTrustFirstOnboardingArm2));
    service()->enabling().SetCompletedFre(prefs::FreStatus::kNotStarted);
  }
  if (GetParam().auto_open_pdf) {
    expected_capabilities.Append(
        std::to_underlying(mojom::HostCapability::kPdfZeroState));
  }
  expected_capabilities.Append(
      std::to_underlying(mojom::HostCapability::kInvoke));
  if (!base::FeatureList::IsEnabled(features::kGlicLiveMode)) {
    expected_capabilities.Append(
        std::to_underlying(mojom::HostCapability::kNoLiveMode));
  }
  if (base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin)) {
    expected_capabilities.Append(
        std::to_underlying(mojom::HostCapability::kAutoLoginSignInWithGoogle));
  }
  if (base::FeatureList::IsEnabled(
          features::kGlicActorAutofillOneTimePassword)) {
    expected_capabilities.Append(
        std::to_underlying(mojom::HostCapability::kAttemptOtpFilling));
  }
  if (base::FeatureList::IsEnabled(features::kGlicWebDragAndDropFileUpload)) {
    expected_capabilities.Append(
        std::to_underlying(mojom::HostCapability::kImgWebDragDrop));
  }
  if (base::FeatureList::IsEnabled(features::kGlicDynamicChromeTools)) {
    expected_capabilities.Append(
        std::to_underlying(mojom::HostCapability::kChromeTools));
  }

  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest({.params = base::Value(std::move(expected_capabilities))});
}

namespace {
void UpdatePrimaryAccountToBeManaged(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info);
  account_info =
      AccountInfo::Builder(account_info)
          .SetHostedDomain(gaia::ExtractDomainName(account_info.email))
          .Build();
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
}
}  // namespace

class GlicApiTestUserStatusCheckTest : public GlicApiTest {
 protected:
  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();
    service()->enabling().SetUserStatusFetchOverrideForTest(
        base::BindRepeating(&GlicApiTestUserStatusCheckTest::UserStatusFetch,
                            base::Unretained(this)));
  }

  void UserStatusFetch(
      base::OnceCallback<void(const CachedUserStatus&)> callback) {
    user_status_fetch_count_++;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), user_status_));
  }

  CachedUserStatus user_status_;
  unsigned int user_status_fetch_count_ = 0;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestUserStatusCheckTest,
                       testMaybeRefreshUserStatus) {
  Profile* profile = GetProfile();
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile),
      policy::EnterpriseManagementAuthority::CLOUD);
  UpdatePrimaryAccountToBeManaged(profile);

  ASSERT_FALSE(GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin());
  user_status_.user_status_code = UserStatusCode::DISABLED_BY_ADMIN;

  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin();
  }));
  EXPECT_GE(user_status_fetch_count_, 1u);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestUserStatusCheckTest,
                       testMaybeRefreshUserStatusThrottled) {
  // As previous, but requests several updates (e.g., as though many errors
  // were processed around the same time). An "enabled" status is assumed as
  // otherwise the client will be unloaded.
  //
  // These expectations are a little loose, because we can't use mock time in
  // browser tests yet, but they should be sufficient to catch a total lack of
  // throttling, at least.

  Profile* profile = GetProfile();
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile),
      policy::EnterpriseManagementAuthority::CLOUD);
  UpdatePrimaryAccountToBeManaged(profile);

  ASSERT_FALSE(GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin());
  user_status_.user_status_code = UserStatusCode::ENABLED;

  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return user_status_fetch_count_ >= 2;
  })) << "There should be at least two fetches (initial and delayed)";
  {
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, loop.QuitClosure(), base::Seconds(5));
    loop.Run();
  }
  // The client code requests 10 updates in quick succession, but the throttling
  // interval in the test parameters is set to 2 seconds. A total of 3 fetches
  // is expected (1 initial, 1 delayed, 1 at the end of the script loops). If
  // throttling is not working we will see up to 12 fetches.
  EXPECT_LE(user_status_fetch_count_, 4u);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testInitializeFails) {
  service()->enabling().SetCompletedFre(prefs::FreStatus::kNotStarted);
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest({
      .params = base::Value(base::DictValue().Set("failWith", "error")),
  });
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kError));
  // Verify non-FRE error metric is recorded immediately.
  EXPECT_THAT(histogram_tester.GetAllSamples("Glic.PanelWebUiState.Error"),
              BucketsAre(Bucket(6 /*CLIENT_ERROR*/, 1)));

  // Verify WebUiState transitions and error metrics update immediately during
  // the session before the panel closes.
  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix("Glic.Fre.PanelWebUiState"),
      UnorderedElementsAre(
          Pair("Glic.Fre.PanelWebUiState",
               BucketsAre(Bucket(mojom::WebUiState::kBeginLoad, 1),
                          Bucket(mojom::WebUiState::kShowLoading, 1),
                          Bucket(mojom::WebUiState::kFinishLoading, 1),
                          Bucket(mojom::WebUiState::kWarmed, 1),
                          Bucket(mojom::WebUiState::kError, 1))),
          Pair("Glic.Fre.PanelWebUiState.Error",
               BucketsAre(Bucket(6 /*CLIENT_ERROR*/, 1)))));

  // Close Glic and verify FinishState is recorded.
  CloseAllEmbeddersAndPreventDeletion();
  ASSERT_OK(WaitForGlicClose());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Glic.Fre.PanelWebUiState.FinishState"),
      BucketsAre(Bucket(5 /*kError*/, 1)));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCloseAndOpenWhileOpening) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(OpenGlicForActiveTab());
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testReloadWebUi) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  GlicClientConnectionObserver connection_observer(instance);
  ExecuteJsTest();
  ASSERT_OK(connection_observer.WaitForConnected());
  instance->host().Reload();
  ASSERT_OK(connection_observer.WaitForDisconnected());
  ExecuteJsTest();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return instance->host().GetPageHandlersForTesting().size() == 1;
  }));
  ASSERT_TRUE(instance->host().GetPrimaryPageHandlerForTesting());
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testDoNothing) {
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 1);
  ASSERT_EQ(GetTabListInterface()->GetTab(0)->GetContents()->GetURL(),
            GetTestUrl("page.html"));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testRecordUseCounter) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(histogram_tester.WaitForBucketCount("Glic.Api.UseCounter", 1, 1));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testDefaultInvocationSource) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

// This test is only useful for platforms that support floaty.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testIsBrowserOpen) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  ExecuteJsTest();

  CloseMainBrowserWithIncognitoKeepAlive();

  ContinueJsTest();
}
#endif

IN_PROC_BROWSER_TEST_P(GlicApiTest, testNavigateToDifferentClientPage) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  WebUIStateListener listener(&instance->host());
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(0)});  // test run count: 0.
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(1)});  // test run count: 1.
  histogram_tester.ExpectBucketCount(
      "Glic.Host.WebClientLifecycleEvent",
      GlicWebClientLifecycleEvent::kDisconnectedOnNavigation, 1);
}

// TODO(b/544866316): Consider moving this to a different test suite
// since it does not use the JS test runner.
IN_PROC_BROWSER_TEST_P(GlicApiTest, testCookieSyncFails) {
  glic::GlicHistogramTester histogram_tester;
  GlicTestEnvironment::GetService(GetProfile())
      ->SetResultForFutureCookieSync(false);

  ToggleGlicForActiveTab(/*prevent_close=*/false);

  auto* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);

  ASSERT_OK(
      RunUntilEqual([&]() { return instance->host().GetPrimaryWebUiState(); },
                    mojom::WebUiState::kError));

  histogram_tester.ExpectBucketCount("Glic.PanelWebUiState.Error",
                                     2 /*COOKIE_SYNC_ERROR*/, 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testUnallowedOriginNavigationBlocked) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  GURL initial_url = guest_contents->GetLastCommittedURL();

  GURL unallowed_url =
      embedded_test_server()->GetURL("b.com", "/test_data/page.html");

  content::TestNavigationObserver observer(guest_contents);
  ASSERT_TRUE(content::ExecJs(
      guest_contents,
      content::JsReplace("location.href = $1;", unallowed_url.spec())));
  observer.Wait();

  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(initial_url, guest_contents->GetLastCommittedURL());
  // The client should still be connected, as no navigation took place.
  ASSERT_OK(WaitForGlicClient(instance));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetUserProfileInfo) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  histogram_tester.ExpectBucketCount(
      "Glic.Api.RequestCounts.GetUserProfileInfo",
      glic::mojom::GlicRequestEvent::kRequestReceived, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.Api.RequestCounts.GetUserProfileInfo",
      glic::mojom::GlicRequestEvent::kResponseSent, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.Api.StatusCounts.Received",
      glic::GlicHostApiRequestId::kGetUserProfileInfo, 1);
  // Confirm that this response-receiving request gets latency metrics recorded.
  histogram_tester.ExpectTotalCount(
      "Glic.Api.RequestHostLatency.GetUserProfileInfo", 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testRequestHeader) {
  ASSERT_OK(OpenGlicForActiveTab());
  const GURL cross_origin_rpc_url =
      embedded_test_server()->GetURL("b.com", "/fake-rpc/cors");
  base::ListValue rpc_urls;
  rpc_urls.Append("/fake-rpc");
  rpc_urls.Append(cross_origin_rpc_url.spec());
  ExecuteJsTest({.params = base::Value(
                     base::DictValue().Set("rpcUrls", std::move(rpc_urls)))});

  auto request_header_matcher = testing::AllOf(
      testing::Contains(testing::Pair(testing::StrCaseEq("x-glic"), "1")),
      testing::Contains(testing::Pair(
          testing::StrCaseEq("x-glic-chrome-channel"),
          testing::AnyOf("unknown", "canary", "dev", "beta", "stable"))),
      testing::Contains(
          testing::Pair(testing::StrCaseEq("x-glic-chrome-version"),
                        version_info::GetVersionNumber())));

  auto find_request = [&](std::string_view path) {
    const auto it = std::ranges::find_if(
        embedded_test_server_requests_, [&](const auto& request) {
          return request.GetURL().GetPath() == path &&
                 request.method == net::test_server::METHOD_GET;
        });
    return it == embedded_test_server_requests_.end() ? nullptr : &(*it);
  };

  auto* main_request = find_request(GetGuestURL().GetPath());
  ASSERT_TRUE(main_request);
  EXPECT_THAT(main_request->headers, request_header_matcher);

  auto* rpc_request = find_request("/fake-rpc");
  ASSERT_TRUE(rpc_request);
  EXPECT_THAT(rpc_request->headers, request_header_matcher);

  auto* cross_origin_rpc_request = find_request("/fake-rpc/cors");
  ASSERT_TRUE(cross_origin_rpc_request);
  EXPECT_THAT(cross_origin_rpc_request->headers, request_header_matcher);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testDialogResponseCallOrder) {
  ASSERT_OK(OpenGlicForActiveTab());
  auto* actor_service = actor::ActorKeyedService::Get(GetProfile());
  ASSERT_TRUE(actor_service);

  base::test::TestFuture<actor::TaskId> task_created;
  base::CallbackListSubscription subscription =
      actor_service->AddTaskStateChangedCallback(
          base::BindLambdaForTesting([&](actor::ActorTask& task) {
            if (task.GetState() == actor::ActorTask::State::kCreated) {
              task_created.SetValue(task.id());
            }
          }));

  // Client side subscribes to the observable returned from
  // selectUserConfirmationDialogRequestHandler and it creates an actor task.
  ExecuteJsTest();

  // Wait for the task to be created. Put it in an interrupted state.
  actor::ActorTask* task = actor_service->GetTask(task_created.Get());
  ASSERT_TRUE(task);

  // TODO(bokan): This shouldn't be necessary but the task is kCreated state
  // from which we cannot interrupt.
  task->SetState(actor::ActorTask::State::kReflecting);

  task->Interrupt();
  ASSERT_EQ(task->GetState(), actor::ActorTask::State::kWaitingOnUser);

  // Request a user dialog to show and record the state of the task when the
  // response from it is received.
  base::test::TestFuture<actor::ActorTask::State>
      state_when_dialog_response_received;
  GetOnlyGlicInstance()
      ->GetActorTaskManager()
      ->GetClientSessionForTesting()
      ->RequestToShowUserConfirmationDialog(
          task->id(), url::Origin(), /*for_sensitive_origin=*/false,
          base::BindLambdaForTesting(
              [&](actor::webui::mojom::UserConfirmationDialogResponsePtr) {
                state_when_dialog_response_received.SetValue(task->GetState());
              }));

  // The client side will respond to the dialog then uninterrupt the task.
  // Ensure the dialog response is received before the task has been
  // uninterrupted.
  ContinueJsTest();

  EXPECT_EQ(state_when_dialog_response_received.Get(),
            actor::ActorTask::State::kWaitingOnUser);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testPopupOpens) {
  ASSERT_OK(OpenGlicForActiveTab());
  EXPECT_EQ(GetPopupCount(), 0);
  ExecuteJsTest();
  ASSERT_OK(RunUntilEqual([&]() { return GetPopupCount(); }, 1));
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testOpenGlicSettingsPage) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(
      RunUntilEqual([&]() { return GetTabListInterface()->GetTabCount(); }, 2));
  EXPECT_EQ(
      GetTabListInterface()->GetActiveTab()->GetContents()->GetVisibleURL(),
      chrome::GetSettingsUrl(chrome::kGlicSettingsSubpage));
  histogram_tester.ExpectTotalCount(
      "Glic.Api.RequestHostLatency.OpenGlicSettingsPage", 0);
}
#endif

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testOpenPasswordManagerSettingsPage) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(
      RunUntilEqual([&]() { return GetTabListInterface()->GetTabCount(); }, 2));
  const GURL settings_url =
      base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin)
          ? chrome::GetSettingsUrl(chrome::kGlicLoginSettingsSubpage)
          : GURL(GetGooglePasswordManagerSubPageURLStr());
  EXPECT_EQ(
      GetTabListInterface()->GetActiveTab()->GetContents()->GetVisibleURL(),
      settings_url);
}
#endif

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest, testOpenContactInfoSettingsPage) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(
      RunUntilEqual([&]() { return GetTabListInterface()->GetTabCount(); }, 2));
  EXPECT_EQ(
      GetTabListInterface()->GetActiveTab()->GetContents()->GetVisibleURL(),
      chrome::GetSettingsUrl(chrome::kContactInfoSubPage));
}
#endif

IN_PROC_BROWSER_TEST_P(GlicApiTest, testClosedCaptioning) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testRefreshSignInCookies) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testSignInPauseState) {
  ASSERT_OK(OpenGlicForActiveTab());
  // Check that Glic web client is open and can retrieve the user's info.
  ExecuteJsTest();

  // Pause the sign-in.
  auto* const identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);

  // The guest frame should be destroyed, and the WebUI should show the sign-in
  // panel.
  ASSERT_OK(RunUntilNull([&]() { return FindGlicGuestMainFrame(); }));
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kSignIn));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testInvoke) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient());
  auto options = mojom::InvokeOptions::New();
  options->invocation_source = mojom::InvocationSource::kTopChromeButton;
  instance->host().GetPrimaryWebClient()->Invoke(std::move(options),
                                                 base::DoNothing());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetContextFromFocusedTabWithIframe) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/browser_tests/test_iframe.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testProcessCounterAbuseVerdict) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  content::WebContents* active_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();

  // Wait for the Safe Browsing interstitial to appear.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return chrome_browser_interstitials::IsShowingInterstitial(active_contents);
  }));

  histogram_tester.ExpectUniqueSample(
      "Glic.Api.ProcessCounterAbuseVerdict.Result",
      static_cast<int>(glic::GlicProcessCounterAbuseVerdictResult::kSuccess),
      1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Api.ProcessCounterAbuseVerdict.ThreatType",
      static_cast<int>(glic::mojom::SbThreatType::kSocialEngineering), 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testProcessCounterAbuseVerdictWhenSafeBrowsingDisabled) {
  glic::GlicHistogramTester histogram_tester;
  GetBrowser()->GetProfile()->GetPrefs()->SetBoolean(
      ::prefs::kSafeBrowsingEnabled, false);
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  content::WebContents* active_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingInterstitial(active_contents));
  histogram_tester.ExpectTotalCount(
      "Glic.Api.ProcessCounterAbuseVerdict.Result", 0);
}

IN_PROC_BROWSER_TEST_P(
    GlicApiTest,
    testProcessCounterAbuseVerdictWhenUrlAllowlistedByPolicy) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* active_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  base::ListValue allowlist;
  allowlist.Append(active_contents->GetVisibleURL().host());
  GetBrowser()->GetProfile()->GetPrefs()->SetList(
      ::prefs::kSafeBrowsingAllowlistDomains, std::move(allowlist));

  ExecuteJsTest();

  // Wait for GlicPageHandler::ProcessCounterAbuseVerdict to run and record the
  // verdict.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.Api.ProcessCounterAbuseVerdict.Result",
               static_cast<int>(glic::GlicProcessCounterAbuseVerdictResult::
                                    kInterstitialSkippedAllowlist)) == 1;
  }));

  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingInterstitial(active_contents));
  histogram_tester.ExpectUniqueSample(
      "Glic.Api.ProcessCounterAbuseVerdict.Result",
      static_cast<int>(glic::GlicProcessCounterAbuseVerdictResult::
                           kInterstitialSkippedAllowlist),
      1);
}

// TODO(harringtond): Flaky on windows.
// TODO(b/508340871): Re-enable on Android. Failing because something pops up
// and suppresses the bottom sheet.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#define MAYBE_testInvocationSource DISABLED_testInvocationSource
#else
#define MAYBE_testInvocationSource testInvocationSource
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testInvocationSource) {
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
                         /*source=*/source);

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
IN_PROC_BROWSER_TEST_P(GlicApiTestWithPixelOutput,
                       MAYBE_testFaviconLoadsWithGetTabById) {
  auto* tab_0_contents = GetTabListInterface()->GetTab(0)->GetContents();
  ASSERT_TRUE(content::NavigateToURL(tab_0_contents, GetTestUrl("page.html")));
  GetTabListInterface()->OpenTab(GetTestUrl("page2.html"), -1);

  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->GetSharingManagerInternal().PinTabs(
      {GetTabListInterface()->GetTab(0)->GetHandle(),
       GetTabListInterface()->GetTab(1)->GetHandle()});
  ExecuteJsTest();
}

// Note: Win-ASAN is flaky.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_testGetContextFromFocusedTabWithAllRequestedData \
  DISABLED_testGetContextFromFocusedTabWithAllRequestedData
#else
#define MAYBE_testGetContextFromFocusedTabWithAllRequestedData \
  testGetContextFromFocusedTabWithAllRequestedData
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithPixelOutput,
                       MAYBE_testGetContextFromFocusedTabWithAllRequestedData) {
  tabs::TabInterface* tab0 = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab0);
  NavigateTab(*tab0,
              embedded_test_server()->GetURL("/glic/browser_tests/test.html"));
  ASSERT_OK(OpenGlicForActiveTab());
  glic::GlicHistogramTester histogram_tester;
  ExecuteJsTest();

  // No error should be logged to the text histogram.
  histogram_tester.ExpectTotalCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Text", 0);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithPixelOutput,
                       testFaviconLoadsWithGetTabFaviconById) {
  auto* tab_0_contents = GetTabListInterface()->GetTab(0)->GetContents();
  ASSERT_TRUE(content::NavigateToURL(tab_0_contents, GetTestUrl("page.html")));

  GetTabListInterface()->OpenTab(GetTestUrl("page2.html"), -1);

  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->GetSharingManagerInternal().PinTabs(
      {GetTabListInterface()->GetTab(0)->GetHandle(),
       GetTabListInterface()->GetTab(1)->GetHandle()});
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithPixelOutput, testFaviconIsUpdated) {
  ASSERT_OK(OpenGlicForActiveTab());

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithPixelOutput, testFaviconIsRemoved) {
  ASSERT_OK(OpenGlicForActiveTab());

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithPixelOutput,
                       testFaviconIsOmittedWithClientCapabilities) {
  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->GetSharingManagerInternal().PinTabs(
      {GetTabListInterface()->GetActiveTab()->GetHandle()});
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithPixelOutput,
                       testTabFaviconObserverLifecycleAndCleanup) {
  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->GetSharingManagerInternal().PinTabs(
      {GetTabListInterface()->GetActiveTab()->GetHandle()});
  tabs::TabHandle active_tab_handle =
      GetTabListInterface()->GetActiveTab()->GetHandle();

  EXPECT_FALSE(service()->tab_favicon_observer().HasTabObserverForTesting(
      active_tab_handle));

  ExecuteJsTest();

  // Wait for the subscription Mojo message to be processed.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return service()->tab_favicon_observer().HasTabObserverForTesting(
        active_tab_handle);
  }));

  ContinueJsTest();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    service()->tab_favicon_observer().FireCleanupTimerForTesting();
    return !service()->tab_favicon_observer().HasTabObserverForTesting(
        active_tab_handle);
  }));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithPixelOutput,
                       testTabFaviconObserverTabWillClose) {
  auto* tab_0_contents = GetTabListInterface()->GetTab(0)->GetContents();
  ASSERT_TRUE(content::NavigateToURL(tab_0_contents, GetTestUrl("page.html")));
  tabs::TabInterface* second_tab =
      GetTabListInterface()->OpenTab(GetTestUrl("page2.html"), -1);
  tabs::TabHandle second_tab_handle = second_tab->GetHandle();

  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->GetSharingManagerInternal().PinTabs(
      {second_tab_handle});

  ExecuteJsTest();

  // Wait for the subscription Mojo message to be processed.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return service()->tab_favicon_observer().HasTabObserverForTesting(
        second_tab_handle);
  }));

  GetTabListInterface()->CloseTab(second_tab_handle);

  EXPECT_FALSE(service()->tab_favicon_observer().HasTabObserverForTesting(
      second_tab_handle));
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_testAndroidFaviconUpdatedViaObserver \
  testAndroidFaviconUpdatedViaObserver
#else
#define MAYBE_testAndroidFaviconUpdatedViaObserver \
  DISABLED_testAndroidFaviconUpdatedViaObserver
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithPixelOutput,
                       MAYBE_testAndroidFaviconUpdatedViaObserver) {
#if BUILDFLAG(IS_ANDROID)
  ASSERT_OK(OpenGlicForActiveTab());
  GetOnlyGlicInstance()->GetSharingManagerInternal().PinTabs(
      {GetTabListInterface()->GetActiveTab()->GetHandle()});

  ExecuteJsTest();

  auto* observer =
      service()->tab_favicon_observer().GetTabFaviconObserverForTesting(
          GetTabListInterface()->GetActiveTab()->GetHandle());
  ASSERT_TRUE(observer);

  SkBitmap red_bitmap;
  red_bitmap.allocN32Pixels(16, 16);
  red_bitmap.eraseColor(SK_ColorRED);
  observer->OnFaviconUpdated(red_bitmap);

  ContinueJsTest();
#else
  GTEST_SKIP() << "Android-only test";
#endif
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testInvokeWaitsForNotifyPanelWillOpen) {
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.surface = DefaultSurface{
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface()};
  coordinator().InvokeWithAutoSubmit(GetPassKey(), std::move(options));

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetExperimentalTriggeringUpdates) {
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
  GetOnlyGlicInstance()
      ->GetExperimentalTriggeringManager()
      ->GetExperimentalTriggeringUpdates(std::move(remote),
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

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetExperimentalTriggeringUpdatesError) {
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
  GetOnlyGlicInstance()
      ->GetExperimentalTriggeringManager()
      ->GetExperimentalTriggeringUpdates(std::move(remote),
                                         future.GetCallback());
  ContinueJsTest();

  run_loop.Run();
  EXPECT_TRUE(future.Get());

  EXPECT_EQ(handler.GetObservation(), mojom::SubscriberObservationType::kError);
}

IN_PROC_BROWSER_TEST_P(GlicApiMultiProfileTest, testPageMetadataCrossProfile) {
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

IN_PROC_BROWSER_TEST_P(GlicApiMultiProfileTest, testTabDataCrossProfile) {
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

IN_PROC_BROWSER_TEST_P(GlicApiMultiProfileTest, testTabFaviconCrossProfile) {
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

IN_PROC_BROWSER_TEST_P(GlicApiMultiProfileTest, testGetContextCrossProfile) {
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

IN_PROC_BROWSER_TEST_P(GlicApiTestWithWebContentsWarming,
                       testWebClientReadyOnFullLoad) {
  ASSERT_TRUE(
      coordinator().GetWebContentsWarmingPoolForTesting().MaybeStartWarming(
          GlicWarmingTrigger::kStartup));
  ASSERT_OK(RunUntilNotNull([&]() {
    return coordinator()
        .GetWebContentsWarmingPoolForTesting()
        .GetWarmedContainerForTesting();
  }));
  // Opening the glic window will trigger the bootstrap, which should transition
  // the WebUI state to kReady.
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetPageMetadata) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetPageMetadataInvalidTabId) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetPageMetadataEmptyNames) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetPageMetadataMultipleSubscriptions) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetPageMetadataUpdates) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

// TODO(crbug.com/449764057): Flakes/fails on all platforms except windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_testGetPageMetadataOnNavigation testGetPageMetadataOnNavigation
#else
#define MAYBE_testGetPageMetadataOnNavigation \
  DISABLED_testGetPageMetadataOnNavigation
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testGetPageMetadataOnNavigation) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

// TODO(crbug.com/454086033): Re-enable this test once I figure out how to
// discard the tab while preserving the test harness.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       DISABLED_testGetPageMetadataWebContentsChanged) {
  // Navigate the first tab.
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));

  ASSERT_OK(OpenGlicForActiveTab());

  // Runs the JS test until the first `advanceToNextStep()`.
  ExecuteJsTest();

  // The JS test is now paused.
  // We first switch focus to the second tab (Tab 1) so that Tab 0 becomes
  // inactive and can be safely discarded.
  GetTabListInterface()->ActivateTab(
      GetTabListInterface()->GetTab(1)->GetHandle());

  content::WebContents* web_contents =
      GetTabListInterface()->GetTab(0)->GetContents();
  ASSERT_TRUE(web_contents);

  // Discard the tab. This will destroy the WebContents.
  resource_coordinator::TabLifecycleUnitExternal::FromWebContents(web_contents)
      ->DiscardTab(::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  // Wait for the tab to be discarded.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetTabListInterface()->GetTab(0)->GetContents()->WasDiscarded();
  }));

  // Select the tab to reload it. This will create a new WebContents.
  GetTabListInterface()->ActivateTab(
      GetTabListInterface()->GetTab(0)->GetHandle());
  content::WebContents* new_web_contents =
      GetTabListInterface()->GetTab(0)->GetContents();
  ASSERT_TRUE(new_web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(new_web_contents));

  // Change the content of the 'author' meta tag from "George" to "Ruth".
  const char* script =
      "document.querySelector('meta[name=\"author\"]').setAttribute('content', "
      "'Ruth')";
  ASSERT_TRUE(content::ExecJs(new_web_contents, script));

  // Continue the JS test to verify the metadata update.
  ContinueJsTest();
}
#endif

// TODO(harringtond): Times out on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_testGetPageMetadataTabDestroyed \
  DISABLED_testGetPageMetadataTabDestroyed
#else
#define MAYBE_testGetPageMetadataTabDestroyed testGetPageMetadataTabDestroyed
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testGetPageMetadataTabDestroyed) {
  // Open a second tab and open glic.
  GetTabListInterface()->OpenTab(
      embedded_test_server()->GetURL("/glic/browser_tests/test.html"), -1);
  GetTabListInterface()->ActivateTab(
      GetTabListInterface()->GetTab(0)->GetHandle());
  ASSERT_OK(OpenGlicForActiveTab());

  // Pin both tabs.
  GetOnlyGlicInstance()->GetSharingManagerInternal().PinTabs(
      {GetTabListInterface()->GetTab(0)->GetHandle(),
       GetTabListInterface()->GetTab(1)->GetHandle()});

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testAdditionalContext) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
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
          "text/plain", mojo_base::BigBuffer(data), std::nullopt)));

  // Add a part with a screenshot.
  std::vector<uint8_t> screenshot_data = {1, 2, 3, 4};
  additional_context->parts.push_back(
      glic::mojom::AdditionalContextPart::NewScreenshot(
          glic::mojom::Screenshot::New(
              10, 20, screenshot_data, "image/png",
              glic::mojom::ImageOriginAnnotations::New(),
              /*encryption_scheme=*/
              glic::mojom::ScreenshotEncryptionScheme::kNone)));

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

  auto tab_context = glic::mojom::TabContextResult::New();
  tab_context->tab_data = std::move(tab_data);
  additional_context->parts.push_back(
      glic::mojom::AdditionalContextPart::NewTabContext(
          std::move(tab_context)));

  additional_context->parts.push_back(
      glic::mojom::AdditionalContextPart::NewRegion(
          glic::mojom::CapturedRegion::NewRect(gfx::Rect(10, 20, 30, 40))));

  instance->SendAdditionalContext(std::move(additional_context));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testAdditionalContextQueued) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));

  ToggleGlicForActiveTab(/*prevent_close=*/true);
  auto* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);

  glic::mojom::AdditionalContextPtr additional_context =
      glic::mojom::AdditionalContext::New();
  additional_context->name = "queued part";
  additional_context->tab_id = 1;
  additional_context->frameUrl = GURL("http://example.com");

  std::vector<uint8_t> data = {'q', 'u', 'e', 'u', 'e', 'd'};
  additional_context->parts.push_back(
      glic::mojom::AdditionalContextPart::NewData(glic::mojom::ContextData::New(
          "text/plain", mojo_base::BigBuffer(data), std::nullopt)));

  instance->SendAdditionalContext(std::move(additional_context));

  ASSERT_OK(WaitForGlicOpen());

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCancelActions) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testRegisterConversationWithEmptyId) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  // Verify that there is no conversation ID initially.
  ASSERT_FALSE(instance->conversation_id());

  ExecuteJsTest();

  // Verify that the conversation_id() still returns std::nullopt.
  ASSERT_FALSE(instance->conversation_id());

  // Verify that GetConversationInfo() returns a non-null info with correct
  // title and empty ID.
  mojom::ConversationInfoPtr retrieved_info = instance->GetConversationInfo();
  EXPECT_EQ("", retrieved_info->conversation_id);
  EXPECT_EQ("Empty Conversation", retrieved_info->conversation_title);
}

// TODO(b/548051765): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_testCallingApiWhileHiddenRecordsMetrics \
  DISABLED_testCallingApiWhileHiddenRecordsMetrics
#else
#define MAYBE_testCallingApiWhileHiddenRecordsMetrics \
  testCallingApiWhileHiddenRecordsMetrics
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       MAYBE_testCallingApiWhileHiddenRecordsMetrics) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));

  glic::GlicHistogramTester histogram_tester;
  ContinueJsTest();
  histogram_tester.ExpectBucketCount("Glic.Api.RequestCounts.CreateTab",
                                     GlicRequestEvent::kRequestReceived, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.Api.RequestCounts.CreateTab",
      GlicRequestEvent::kRequestReceivedWhileInactive, 1);
}

class GlicApiTestWithGeminiActOnWebPolicy : public GlicApiTest {
 public:
  GlicApiTestWithGeminiActOnWebPolicy() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {{features::kGlicActorEnterprisePrefDefault.name,
          features::kGlicActorEnterprisePrefDefault.GetName(
              features::GlicActorEnterprisePrefDefault::kDisabledByDefault)},
         {features::kGlicActorPolicyControlExemption.name, "false"}});
  }
  ~GlicApiTestWithGeminiActOnWebPolicy() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    GlicApiTest::SetUpInProcessBrowserTestFixture();
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

    GlicApiTest::SetUpBrowserContextKeyedServices(context);
  }

  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();

    scoped_glic_bypass_.emplace();

    adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());
    identity_test_env_ = adaptor_->identity_test_env();
    identity_test_env_->SetTestURLLoaderFactory(&test_url_loader_factory_);
    identity_manager_ = IdentityManagerFactory::GetForProfile(GetProfile());

    // Simulate sign-in.
    AccountInfo account_info = identity_test_env_->MakePrimaryAccountAvailable(
        "foo@bar.com", signin::ConsentLevel::kSync);

    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_->UpdateAccountInfoForAccount(account_info);
    identity_test_env_->SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        "bar.com", "Full Name", "Given Name", "Locale", "Picture URL");

    GetProfile()->GetPrefs()->SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier, 1);

    policy_provider_.SetupPolicyServiceForPolicyUpdates(
        GetProfile()->GetProfilePolicyConnector()->policy_service());
  }

  void TearDownOnMainThread() override {
    scoped_glic_bypass_.reset();
    identity_manager_ = nullptr;
    identity_test_env_ = nullptr;
    adaptor_.reset();
    policy_provider_.SetupPolicyServiceForPolicyUpdates(nullptr);
    GlicApiTest::TearDownOnMainThread();
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
  std::optional<GlicEnabling::ScopedBypassEnablementChecksForTesting>
      scoped_glic_bypass_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithGeminiActOnWebPolicy,
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

class GlicApiTestWithSkills : public GlicApiTest {
 public:
  GlicApiTestWithSkills() {
    scoped_feature_list_.InitAndEnableFeature(::features::kSkillsEnabled);
  }

  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();
    service_ = skills::SkillsServiceFactory::GetForProfile(GetProfile());
    ASSERT_TRUE(service_);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return service_->GetServiceStatus() !=
             skills::SkillsService::ServiceStatus::kNotInitialized;
    }));
    service_->SetServiceStatusForTesting(
        skills::SkillsService::ServiceStatus::kReady);
    ASSERT_OK(OpenGlicForActiveTab());
  }

  void TearDownOnMainThread() override {
    service_ = nullptr;
    GlicApiTest::TearDownOnMainThread();
  }

  skills::SkillsService* SkillsService() { return service_; }

  void WaitForSkillsTab(const std::string& path) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
      return tab && base::StartsWith(
                        tab->GetContents()->GetLastCommittedURL().spec(),
                        GURL(chrome::kChromeUISkillsURL).Resolve(path).spec());
    }));
  }

 private:
  raw_ptr<skills::SkillsService> service_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills, testGetSkillSuccess) {
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

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills, testGetSkillPreviewsSuccess) {
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

class GlicApiTestWithSkillsDisabled : public GlicApiTest {
 public:
  GlicApiTestWithSkillsDisabled() {
    scoped_feature_list_.InitAndEnableFeature(::features::kSkillsEnabled);
  }

  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();
    GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                         false);
    ASSERT_OK(OpenGlicForActiveTab());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkillsDisabled, testGetSkillDisabled) {
  ExecuteJsTest();
}

// TODO(b/546606964): enable these tests on android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkillsDisabled,
                       testSkillsEnabledToggledAtRuntime) {
  ExecuteJsTest();
  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       true);
  ContinueJsTest();
  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       false);
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkillsDisabled,
                       testContextualSkillsRetainedWhenStartingPrefDisabled) {
  const GURL url = GetTestUrl("page.html");
  skills::proto::SkillsList skills_list;
  skills::proto::Skill* skill = skills_list.add_skills();
  skill->set_id("contextual_skill_id_1");
  skill->set_name("contextual_skill_1");
  skill->set_icon("contextual_skill_icon_1");
  skill->set_description("contextual_skill_description_1");
  skill->set_prompt("contextual_skill_prompt_1");

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/skills.proto.SkillsList");
  skills_list.SerializeToString(any_metadata.mutable_value());
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_any_metadata(any_metadata);

  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile());
  optimization_guide_decider->AddHintForTesting(
      url, optimization_guide::proto::OptimizationType::SKILLS, metadata);

  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), url));

  ExecuteJsTest();

  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       true);
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills, testSkillsEnabledState) {
  glic::GlicHistogramTester histogram_tester;
  SkillsService()->AddSkill(/*source_skill_id=*/"source_id_1",
                            /*name=*/"test_skill_1",
                            /*icon=*/"test_icon_1",
                            /*prompt=*/"test_prompt_1");
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  histogram_tester.ExpectBucketCount(
      "Glic.Skills.WebClient.Event",
      static_cast<int>(mojom::SkillsWebClientEvent::kOpenedMenu), 1);
  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       false);
  ContinueJsTest();
  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       true);
  ContinueJsTest();
  histogram_tester.ExpectBucketCount(
      "Glic.Skills.WebClient.Event",
      static_cast<int>(mojom::SkillsWebClientEvent::kOpenedMenu), 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills, testCreateSkillAndDisable) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
    auto* controller = static_cast<skills::SkillsUiTabController*>(
        skills::SkillsUiTabControllerInterface::From(tab));
    return controller && controller->IsShowing();
  }));
  GetProfile()->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled,
                                       false);
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  auto* controller = static_cast<skills::SkillsUiTabController*>(
      skills::SkillsUiTabControllerInterface::From(tab));
  ASSERT_TRUE(controller);
  controller->CloseDialog();
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills, testDisplaySkillInDialogSuccess) {
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
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills, testShowManageSkillsUi) {
  ExecuteJsTest();
  WaitForSkillsTab(chrome::kChromeUISkillsYourSkillsPath);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills, testShowBrowseSkillsUi) {
  ExecuteJsTest();
  WaitForSkillsTab(chrome::kChromeUISkillsBrowsePath);
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills,
                       testSendingContextualSkillsToGlic) {
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
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com"),
      /*category=*/std::nullopt, /*creation_time=*/std::nullopt));
  skills_batch_1.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_2", "contextual_skill_2", "contextual_skill_icon_2",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_2",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com"),
      /*category=*/std::nullopt, /*creation_time=*/std::nullopt));

  GlicInstanceImpl* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);
  instance->skills_manager().NotifyContextualSkillsChanged(
      std::move(skills_batch_1));

  ContinueJsTest();

  std::vector<mojom::SkillPreviewPtr> skills_batch_2;
  skills_batch_2.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_3", "contextual_skill_3", "contextual_skill_icon_3",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_3",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com"),
      /*category=*/std::nullopt, /*creation_time=*/std::nullopt));
  instance->skills_manager().NotifyContextualSkillsChanged(
      std::move(skills_batch_2));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills,
                       testSendingPendingContextualSkillsToGlic) {
  ToggleGlicForActiveTab(/*prevent_close=*/true);
  GlicInstanceImpl* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);

  std::vector<mojom::SkillPreviewPtr> skills_batch;
  skills_batch.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_1", "contextual_skill_1", "contextual_skill_icon_1",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_1",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com"),
      /*category=*/std::nullopt, /*creation_time=*/std::nullopt));

  instance->skills_manager().NotifyContextualSkillsChanged(
      std::move(skills_batch));

  ASSERT_OK(WaitForGlicOpen());

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills,
                       testChangingActiveTabClearsPendingContextualSkills) {
  GetProfile()->GetPrefs()->SetBoolean(
      prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, false);

  ToggleGlicForActiveTab(/*prevent_close=*/true);
  GlicInstanceImpl* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);

  std::vector<mojom::SkillPreviewPtr> skills_batch;
  skills_batch.push_back(mojom::SkillPreview::New(
      "contextual_skill_id_1", "contextual_skill_1", "contextual_skill_icon_1",
      mojom::SkillSource::kFirstParty, "contextual_skill_description_1",
      /*curated_by=*/std::nullopt, /*image_url=*/GURL("https://example.com"),
      /*category=*/std::nullopt, /*creation_time=*/std::nullopt));

  instance->skills_manager().NotifyContextualSkillsChanged(
      std::move(skills_batch));

  // Change the active tab before Glic is opened.
  CreateAndActivateTab(
      embedded_test_server()->GetURL("/glic/browser_tests/test.html"));

  ASSERT_OK_AND_ASSIGN(auto* instance2, OpenGlicForActiveTab());

  ExecuteJsTest({.instance = instance2});
}

// TODO(b/546606964): enable these tests on android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills, testShowManageSkillsUiNoWindow) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndDetach());
  BrowserWindowInterface* browser_to_close = GetBrowserWindowInterface();
  PlatformBrowserTest::CreateIncognitoBrowser();
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
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithSkills, testCreateSkillNoWindow) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndDetach());
  BrowserWindowInterface* browser_to_close = GetBrowserWindowInterface();
  PlatformBrowserTest::CreateIncognitoBrowser();
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
}
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

const uint8_t kTestRecipientPublicKey[] = {
    0x04, 0x35, 0x02, 0x67, 0xB9, 0x10, 0x8F, 0x9B, 0xF1, 0x85, 0xF5,
    0x1B, 0xD7, 0xA4, 0xEF, 0xBD, 0x28, 0xB3, 0x11, 0x40, 0xBA, 0xD0,
    0xEE, 0xB2, 0x97, 0xDA, 0x6A, 0x93, 0x2D, 0x26, 0x45, 0xBD, 0xB2,
    0x9A, 0x9F, 0xB8, 0x19, 0xD8, 0x21, 0x6F, 0x66, 0xE3, 0xF6, 0x0B,
    0x74, 0xB2, 0x28, 0x38, 0xDC, 0xA7, 0x8A, 0x58, 0x0D, 0x56, 0x47,
    0x3E, 0xD0, 0x5B, 0x5C, 0x93, 0x4E, 0xB3, 0x89, 0x87, 0x64};

const uint8_t kTestAuthSecret[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                   0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                   0x0D, 0x0E, 0x0F, 0x10};

}  // namespace

IN_PROC_BROWSER_TEST_P(GlicApiTestWithExperimentalTriggeringScreenshot,
                       testCaptureAndUploadEncryptedScreenshot) {
  std::vector<uint8_t> recipient_public_key =
      base::ToVector(kTestRecipientPublicKey);
  std::vector<uint8_t> auth_secret = base::ToVector(kTestAuthSecret);

  ASSERT_OK(OpenGlicForActiveTab());
  RegisterConversation(GetOnlyGlicInstance(), "test-conv-id");
  ASSERT_OK(CreateActorTaskObservingActiveTab(GetOnlyGlicInstance()));

  ASSERT_TRUE(RunUntil(
      [&]() { return GetOnlyGlicInstance()->host().IsWebClientConnected(); },
      "waiting for web client connected"));

  base::test::TestFuture<const std::optional<std::string>&> future;
  ASSERT_NE(GetOnlyGlicInstance()->GetExperimentalTriggeringManager(), nullptr);
  GetOnlyGlicInstance()
      ->GetExperimentalTriggeringManager()
      ->CaptureAndUploadEncryptedScreenshot(recipient_public_key, auth_secret,
                                            future.GetCallback());

  ExecuteJsTest();

  std::optional<std::string> file_token = future.Get();
  ASSERT_TRUE(file_token.has_value());
  EXPECT_EQ(*file_token, "mock-file-token-12345");
}

IN_PROC_BROWSER_TEST_P(
    GlicApiTestWithExperimentalTriggeringScreenshot,
    testCaptureAndUploadEncryptedScreenshotWithUnfocusablePage) {
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      GURL(chrome::kChromeUIVersionURL)));

  std::vector<uint8_t> recipient_public_key =
      base::ToVector(kTestRecipientPublicKey);
  std::vector<uint8_t> auth_secret = base::ToVector(kTestAuthSecret);

  ASSERT_OK(OpenGlicForActiveTab());
  RegisterConversation(GetOnlyGlicInstance(), "test-conv-id");
  ASSERT_OK(CreateActorTaskObservingActiveTab(GetOnlyGlicInstance()));

  base::test::TestFuture<const std::optional<std::string>&> future;
  ASSERT_NE(GetOnlyGlicInstance()->GetExperimentalTriggeringManager(), nullptr);
  GetOnlyGlicInstance()
      ->GetExperimentalTriggeringManager()
      ->CaptureAndUploadEncryptedScreenshot(recipient_public_key, auth_secret,
                                            future.GetCallback());

  std::optional<std::string> file_token = future.Get();
  EXPECT_FALSE(file_token.has_value());
}

class GlicApiUnresponsiveTest : public GlicApiTest {
 public:
  GlicApiUnresponsiveTest() {
    features_.InitAndEnableFeatureWithParameters(
        features::kGlicClientResponsivenessCheck,
        {
            {features::kGlicClientResponsivenessCheckIntervalMs.name, "1000"},
            {features::kGlicClientResponsivenessCheckTimeoutMs.name, "3000"},
            {features::kGlicClientUnresponsiveUiMaxTimeMs.name, "1000"},
            {features::kGlicClientResponsivenessCheckIgnoreWhenDebuggerAttached
                 .name,
             "false"},
        });
  }

 private:
  base::test::ScopedFeatureList features_;
};

#if defined(SLOW_BINARY)
#define MAYBE_testUnresponsive DISABLED_testUnresponsive
#else
#define MAYBE_testUnresponsive testUnresponsive
#endif
IN_PROC_BROWSER_TEST_P(GlicApiUnresponsiveTest, MAYBE_testUnresponsive) {
  GlicHistogramTester histogram_tester;
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  GlicClientConnectionObserver connection_observer(instance);
  // This web client does not respond to responsiveness checks, so the client
  // will be declared unresponsive and disconnect.
  ExecuteJsTest();
  ASSERT_OK(connection_observer.WaitForDisconnected());

  histogram_tester.ExpectBucketCount(
      "Glic.Host.WebClientUnresponsiveState",
      /*WebClientUnresponsiveState.ENTERED_FROM_CUSTOM_HEARTBEAT*/ 1, 1);
  histogram_tester.ExpectBucketCount("Glic.Host.WebClientUnresponsiveState",
                                     /*WebClientUnresponsiveState.EXITED*/ 4,
                                     1);
  histogram_tester.ExpectTotalCount(
      "Glic.Host.WebClientUnresponsiveState.Duration", 1);
  histogram_tester.ExpectBucketCount("Glic.PanelWebUiState.Error",
                                     /*WebUiErrorReason.CLIENT_ERROR*/ 6, 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testActuationOnWebSetting) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testSetContextAccessIndicator) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testSetAudioDucking) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetDisplayMedia) {
  // getDisplayMedia() (tab capture) is not supported on standard mobile
  // Android.
  SKIP_TEST_FOR_NON_DESKTOP_ANDROID();
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testJournal) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  histogram_tester.ExpectTotalCount("Glic.Actor.JournalEvent.async_event", 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testStopMicrophone) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  base::test::TestFuture<void> microphone_stopped;
  GetOnlyGlicInstance()->host().StopMicrophone(
      microphone_stopped.GetCallback());
  EXPECT_TRUE(microphone_stopped.Wait());
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testSetSyntheticExperimentState) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([]() {
    std::vector<variations::ActiveGroupId> trials =
        g_browser_process->metrics_service()
            ->GetSyntheticTrialRegistry()
            ->GetCurrentSyntheticFieldTrialsForTest();
    variations::ActiveGroupId expected =
        variations::MakeActiveGroupId("TestTrial", "Enabled");
    return std::ranges::any_of(trials, [&](const auto& trial) {
      return trial.name == expected.name && trial.group == expected.group;
    });
  }));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testSetSyntheticExperimentStateMultiProfile) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([]() {
    std::vector<variations::ActiveGroupId> trials =
        g_browser_process->metrics_service()
            ->GetSyntheticTrialRegistry()
            ->GetCurrentSyntheticFieldTrialsForTest();
    variations::ActiveGroupId expected =
        variations::MakeActiveGroupId("TestTrial", "MultiProfileDetected");
    return std::ranges::any_of(trials, [&](const auto& trial) {
      return trial.name == expected.name && trial.group == expected.group;
    });
  }));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testNotifyPanelWillOpenIsCalledOnce) {
  ASSERT_OK_AND_ASSIGN(auto* instance1, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient());
  PreventDeletionOnClose();
  ExecuteJsTest();
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));
  ASSERT_OK_AND_ASSIGN(auto* instance2, OpenGlicForActiveTab());
  ASSERT_EQ(instance1, instance2);
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testSwitchConversationToLastActiveConversation) {
  base::HistogramTester histogram_tester;
  ASSERT_OK_AND_ASSIGN(auto* tab0_instance, OpenGlicForActiveTab());

  ExecuteJsTest({.params = base::Value("step1"), .instance = tab0_instance});

  CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  ASSERT_OK_AND_ASSIGN(auto* tab1_instance, OpenGlicForActiveTab());

  ExecuteJsTest({.params = base::Value("step2"), .instance = tab1_instance});
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.Interaction.SwitchConversationTarget",
               GlicSwitchConversationTarget::kSwitchedToLastActive) == 1;
  }));
  ContinueJsTest({.instance = tab0_instance});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testSwitchConversationToOldConversationInOldInstance) {
  base::HistogramTester histogram_tester;
  ASSERT_OK_AND_ASSIGN(auto* tab0_instance, OpenGlicForActiveTab());

  ExecuteJsTest({.params = base::Value("step1"), .instance = tab0_instance});

  CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  ASSERT_OK_AND_ASSIGN(auto* tab1_instance, OpenGlicForActiveTab());

  ExecuteJsTest({.params = base::Value("step2"), .instance = tab1_instance});
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.Interaction.SwitchConversationTarget",
               GlicSwitchConversationTarget::kSwitchedToNewInstance) == 1;
  }));

  CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  ASSERT_OK_AND_ASSIGN(auto* tab2_instance, OpenGlicForActiveTab());

  ExecuteJsTest({.params = base::Value("step3"), .instance = tab2_instance});
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.Interaction.SwitchConversationTarget",
               GlicSwitchConversationTarget::kSwitchedToExistingInstance) == 1;
  }));
  ContinueJsTest({.instance = tab0_instance});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testSwitchConversationToExistingInstance) {
  // Open glic for the first tab. It will register a conversation.
  ASSERT_OK_AND_ASSIGN(auto* tab0_instance, OpenGlicForActiveTab());
  ExecuteJsTest({.params = base::Value("first"), .instance = tab0_instance});

  // Open a second tab and second glic instance. It will switch conversations
  // resulting in deleting the second glic instance.
  CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(auto* tab1_instance, OpenGlicForActiveTab());
  ExecuteJsTest({.params = base::Value("second"), .instance = tab1_instance});

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return coordinator().GetInstances().size() == 1u; }));
  ASSERT_EQ("id_hello", GetOnlyGlicInstance()->conversation_id());

  // This should continue the test in the first instance, because tab 2 is now
  // bound to that instance.
  ContinueJsTest({.instance = tab0_instance});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testPanelWillOpenHasRecentlyActiveConversations) {
  tabs::TabInterface* tab0 = GetTabListInterface()->GetActiveTab();
  NavigateTab(*tab0,
              embedded_test_server()->GetURL("/browser_tests/test.html"));
  ASSERT_OK_AND_ASSIGN(auto* tab0_instance, OpenGlicForActiveTab());
  ExecuteJsTest(
      {.params = base::Value("instance1"), .instance = tab0_instance});

  tabs::TabInterface* tab1 = CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  ASSERT_OK_AND_ASSIGN(auto* tab1_instance, OpenGlicForActiveTab());
  ExecuteJsTest(
      {.params = base::Value("instance2"), .instance = tab1_instance});

  tabs::TabInterface* tab2 = CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  ASSERT_OK_AND_ASSIGN(auto* tab2_instance, OpenGlicForActiveTab());
  ExecuteJsTest(
      {.params = base::Value("instance3"), .instance = tab2_instance});

  // Activate tabs in a specific order to set recency: 0, 2, 1.
  // Instance 1 will be most recent, then 2, then 0.
  for (tabs::TabInterface* tab : {tab0, tab2, tab1}) {
    ActivateTab(tab);
    // Switching tabs does not automatically show the panel on all platforms
    // (e.g. mobile Android due to peek behavior), so explicitly open Glic for
    // the active tab to ensure it becomes active.
    ASSERT_OK(OpenGlicForActiveTab());
    auto* instance = GetInstanceForTab(tab);
    ASSERT_TRUE(instance);
    ASSERT_TRUE(base::test::RunUntil([&]() { return instance->IsActive(); }));
  }

  // Open a 4th tab to verify.
  CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  ASSERT_OK_AND_ASSIGN(auto* tab3_instance, OpenGlicForActiveTab());
  ExecuteJsTest(
      {.params = base::Value("instance4"), .instance = tab3_instance});

  // Open a 5th tab to verify.
  CreateAndActivateTab(
      embedded_test_server()->GetURL("/browser_tests/test.html"));
  ASSERT_OK_AND_ASSIGN(auto* tab4_instance, OpenGlicForActiveTab());
  ExecuteJsTest({.params = base::Value("verify"), .instance = tab4_instance});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testPanelWillOpenHasPromptSuggestion) {
  // Simulate click on contextual cue with prompt suggestion.
  glic::GlicInvokeOptions options(
      glic::Target(*GetTabListInterface()->GetActiveTab()),
      glic::mojom::InvocationSource::kNudge);
  options.prompts.push_back("Prompt Suggestion");
  glic::GlicKeyedServiceFactory::GetGlicKeyedService(GetProfile())
      ->Invoke(std::move(options));

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testTabDataUpdateOnUrlChangeForPinnedTab) {
  tabs::TabInterface* tab0 = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab0);
  std::string tab0_id = GlicTabId(tab0->GetHandle());

  CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_OK_AND_ASSIGN(auto* tab1_instance, OpenGlicForActiveTab());

  ExecuteJsTest({.params = base::Value(base::DictValue().Set("tabId", tab0_id)),
                 .instance = tab1_instance});

  // Navigate to another page in the first tab.
  GURL new_url = embedded_test_server()->GetURL(
      "/glic/browser_tests/test.html?changed=true");
  NavigateTab(*tab0, new_url);

  ContinueJsTest({.instance = tab1_instance});
}

// TabData.favicon is not supported on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_testTabDataUpdateOnFaviconChangeForPinnedTab \
  DISABLED_testTabDataUpdateOnFaviconChangeForPinnedTab
#else
#define MAYBE_testTabDataUpdateOnFaviconChangeForPinnedTab \
  testTabDataUpdateOnFaviconChangeForPinnedTab
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       MAYBE_testTabDataUpdateOnFaviconChangeForPinnedTab) {
  tabs::TabInterface* tab0 = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab0);
  NavigateTab(*tab0,
              embedded_test_server()->GetURL("/glic/browser_tests/test.html"));
  std::string tab0_id = GlicTabId(tab0->GetHandle());

  CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_OK_AND_ASSIGN(auto* tab1_instance, OpenGlicForActiveTab());

  ExecuteJsTest({.params = base::Value(base::DictValue().Set("tabId", tab0_id)),
                 .instance = tab1_instance});

  // Add favicon to the webcontents.
  const char* script =
      "var link = document.createElement('link');"
      "link.rel = 'icon';"
      "link.href= '../../../glic/youtube_favicon_16x16.png';"
      "document.head.appendChild(link);";
  ASSERT_TRUE(content::ExecJs(tab0->GetContents(), script));

  ContinueJsTest({.instance = tab1_instance});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testMetrics) {
  glic::GlicHistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicClosedCaptioningEnabled,
                                       true);
  ASSERT_OK(OpenGlicForActiveTab());

  ExecuteJsTest();

  ASSERT_OK(WaitForUserActionCount(user_action_tester,
                                   "GlicContextUploadStarted", 1));
  ASSERT_OK(WaitForUserActionCount(user_action_tester,
                                   "GlicContextUploadCompleted", 1));
  ASSERT_OK(
      WaitForUserActionCount(user_action_tester, "GlicReactionModelled", 1));
  ASSERT_OK(
      WaitForUserActionCount(user_action_tester, "GlicResponseStopByUser", 1));
  ASSERT_OK(histogram_tester.WaitForBucketCount(
      "Glic.Response.ClosedCaptionsShown", /*sample=*/true,
      /*expected_bucket_count=*/1));
  ASSERT_OK(
      histogram_tester.WaitForTotalCount("Glic.TabContext.UploadTime", 1));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testUserInputSubmittedPromptType) {
  glic::GlicHistogramTester histogram_tester;
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  ASSERT_OK(histogram_tester.WaitForBucketCount(
      "Glic.Turn.PromptType", mojom::PromptType::kTypedText, 1));
}

// TODO(crbug.com/454083080): Fix this, it hangs.
IN_PROC_BROWSER_TEST_P(GlicApiTest, DISABLED_testCaptureScreenshot) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

// TODO(crbug.com/441588906): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_P(GlicApiTest, DISABLED_testFetchInactiveTabScreenshot) {
  tabs::TabInterface* tab0 = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab0);
  CreateAndActivateTab(GetSimpleTestUrl());

  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  ActivateTab(tab0);

  ContinueJsTest();
}

// TODO(crbug.com/460826488): Enable on ChromeOS.
// Win-asan is flaky.
#if (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
     (BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)))
#define MAYBE_testFetchInactiveTabScreenshotWhileMinimized \
  DISABLED_testFetchInactiveTabScreenshotWhileMinimized
#else
#define MAYBE_testFetchInactiveTabScreenshotWhileMinimized \
  testFetchInactiveTabScreenshotWhileMinimized
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       MAYBE_testFetchInactiveTabScreenshotWhileMinimized) {
  tabs::TabInterface* tab0 = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab0);
  CreateAndActivateTab(GetSimpleTestUrl());

  bool can_fetch_screenshot = BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC);

  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest({.params = base::Value(can_fetch_screenshot)});

  ActivateTab(tab0);
#if !BUILDFLAG(IS_ANDROID)
  GetBrowserWindowInterface()->GetWindow()->Minimize();
#endif

  ContinueJsTest();
}

// TODO(b/498955581): Clean up glic hibernation experiments, and test in the
// coordinator test.
IN_PROC_BROWSER_TEST_P(GlicApiTest, testHibernateAllOnMemoryPressure) {
  ASSERT_TRUE(
      coordinator().GetWebContentsWarmingPoolForTesting().MaybeStartWarming(
          GlicWarmingTrigger::kStartup));

  // Open 3 instances, with instance 2 being the active one.
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab1);
  ASSERT_OK_AND_ASSIGN(auto* instance1, OpenGlicForActiveTab());

  CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_OK_AND_ASSIGN(auto* instance2, OpenGlicForActiveTab());

  CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_OK_AND_ASSIGN(auto* instance3, OpenGlicForActiveTab());

  // Close instance 3 to make it non-showing and non-actuating.
  ASSERT_OK(CloseAllEmbeddersAndWait(instance3));
  ASSERT_FALSE(instance3->IsHibernated());

  // Switch back to tab 1, so instance 1 is now active and instance 2 is not
  // showing.
  ActivateTab(tab1);
  ASSERT_OK(WaitForGlicOpen(tab1));

  // There is a warmed contents initially. It should be non-showing and
  // non-actuating.
  ASSERT_TRUE(
      coordinator().GetWebContentsWarmingPoolForTesting().MaybeStartWarming(
          GlicWarmingTrigger::kStartup));
  ASSERT_TRUE(coordinator()
                  .GetWebContentsWarmingPoolForTesting()
                  .HasWarmedContainerForTesting());

  // Simulate memory pressure.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Wait for the non-showing instances to hibernate.
  ASSERT_OK(WaitForGlicHibernated(instance2));
  ASSERT_OK(WaitForGlicHibernated(instance3));

  // Verify the warmed contents is reset.
  ASSERT_FALSE(coordinator()
                   .GetWebContentsWarmingPoolForTesting()
                   .HasWarmedContainerForTesting());

  // Active instance should not be hibernated.
  ASSERT_TRUE(instance1->IsShowing());
  ASSERT_FALSE(instance1->IsHibernated());
}

auto DefaultTestParamSet() {
  return testing::Values(TestParams{});
}

#ifndef DISABLE_ALL_TESTS
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicGetHostCapabilityApiTest,
    testing::Values(TestParams{},
                    TestParams{.enable_scroll_to_pdf = true},
                    TestParams{.trust_first_onboarding_arm2 = true},
                    TestParams{.trust_first_onboarding_arm2 = true,
                               .auto_open_pdf = true}),
    &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestUserStatusCheckTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestNoFloatyOrLiveMode,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithFastTimeout,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestGeminiEnterpriseSettingsOverride,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestGeminiEnterpriseSettingsDisabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

#if !BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestGeminiEnterpriseSettingsPolicy,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestGeminiEnterpriseSettingsPolicyUnset,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
#endif

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithWebContentsWarming,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithPixelOutput,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithContextualCueing,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithGeminiActOnWebPolicy,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiMultiProfileTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithDefaultTabContextDisabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithBlankInstanceDelay,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithDefaultTabContextEnabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicApiTestForNoWebUiLoader,
    testing::Values(TestParams{.enable_no_web_ui_loader = false},
                    TestParams{.enable_no_web_ui_loader = true}),
    &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithWebActuationSettingDisabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithWebActuationSettingEnabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithProcessCounterAbuseVerdictDisabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithMqlsIdGetterDisabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithMqlsIdGetterEnabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithCachedUserProfile,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestRuntimeFeatureOff,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiScrollToTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithExperimentalTriggeringScreenshot,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiUnresponsiveTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithSkills,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithSkillsDisabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicOnboardingApiTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestSystemSettingsTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithNewTabDaisyChain,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

#if !BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithFileUploadPolicyEnabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
#endif
#else
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiTestWithFastTimeout);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    GlicApiTestWithWebContentsWarming);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiTestWithPixelOutput);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiTestWithContextualCueing);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    GlicApiTestWithGeminiActOnWebPolicy);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiMultiProfileTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    GlicApiTestWithDefaultTabContextDisabled);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    GlicApiTestWithBlankInstanceDelay);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    GlicApiTestWithDefaultTabContextEnabled);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    GlicApiTestWithWebActuationSettingDisabled);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    GlicApiTestWithWebActuationSettingEnabled);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    GlicApiTestWithProcessCounterAbuseVerdictDisabled);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiTestForNoWebUiLoader);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiScrollToTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    GlicApiTestWithExperimentalTriggeringScreenshot);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiUnresponsiveTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiTestWithSkills);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiTestWithSkillsDisabled);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicOnboardingApiTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiTestSystemSettingsTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GlicApiTestWithNewTabDaisyChain);
#if !BUILDFLAG(IS_ANDROID)
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    GlicApiTestWithFileUploadPolicyEnabled);
#endif
#endif
}  // namespace glic
