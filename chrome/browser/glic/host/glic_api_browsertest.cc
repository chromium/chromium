// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/glic_skills_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/metrics/glic_instance_coordinator_metrics.h"
#include "chrome/browser/glic/service/metrics/glic_instance_helper_metrics.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/suggestions/mock_contextual_cueing_service.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_histogram_tester.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/media/audio_ducker.h"
#include "chrome/browser/permissions/system/mock_platform_handle.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_tab_controller.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/metrics/metrics_service.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/tabs/public/tab_interface.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/dns/mock_host_resolver.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/profiles/profile_picker.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

// This file runs the respective JS tests from
// chrome/test/data/webui/glic/browser_tests/glic_api_browsertest.ts.

#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
#define SLOW_BINARY
#endif

// This skips a test for the multi-instance variant.
#define SKIP_TEST_FOR_MULTI_INSTANCE()                      \
  do {                                                      \
    GTEST_SKIP() << "Not supported in multi-instance mode"; \
    return;                                                 \
  } while (0)

// This skips a test for the multi-instance variant. It's a marker to remember
// to revisit this test later.
#define TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST() SKIP_TEST_FOR_MULTI_INSTANCE()

namespace glic {
namespace {
using ::base::Bucket;
using ::base::BucketsAre;
using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::_;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTab);
std::vector<std::string> GetTestSuiteNames() {
  return {
      "GlicApiTest",
      "GlicApiTestWithOneTab",

      "GlicApiTestUserStatusCheckTest",
      "GlicApiTestWithOneTabMoreDebounceDelay",
      "GlicGetHostCapabilityApiTest",
      "GlicApiTestWithWebContentsWarming",
      "GlicApiTestHibernateAllOnMemoryPressure",
      "GlicApiTestGeminiEnterpriseSettingsOverride",
      "GlicApiTestGeminiEnterpriseSettingsDisabled",
      "GlicApiTestGeminiEnterpriseSettingsPolicy",
      "GlicApiTestGeminiEnterpriseSettingsPolicyUnset",
  };
}

// All tests in this file use the same test params here.
struct TestParams {
  // This is only used by one fixture.
  bool enable_scroll_to_pdf = false;
  bool onboarding_needed = false;
  bool auto_open_pdf = false;
};

class WithTestParams : public testing::WithParamInterface<TestParams> {
 public:
  WithTestParams() {
    test_param_features_.InitAndEnableFeature(features::kGlicMultiInstance);
  }

  static std::string PrintTestVariant(
      const ::testing::TestParamInfo<TestParams>& info) {
    std::vector<std::string> result;
    if (info.param.enable_scroll_to_pdf) {
      result.push_back("EnableScrollToPdf");
    }
    if (info.param.onboarding_needed) {
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

class GlicApiTest : public NonInteractiveGlicApiTest, public WithTestParams {
 public:
  template <typename... Args>
  explicit GlicApiTest(Args&&... args)
      : NonInteractiveGlicApiTest("./glic_api_browsertest.js",
                                  std::forward<Args>(args)...) {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicScrollTo, {}},
         {mojom::features::kGlicMultiTab, {}},
         {features::kGlicWebContentsWarming,
          {
              // Effectively disable web contents warming, to make test output
              // easier to understand.
              {features::kGlicWebContentsWarmingDelay.name, "7d"},
          }},
         {features::kGlicWebActuationSetting, {}},
         {features::kGlicCaptureRegion, {}},
         {features::kGlicPopupWindowsEnabled, {}},
         {features::kGlicUserStatusCheck,
          {{features::kGlicUserStatusRefreshApi.name, "true"},
           {features::kGlicUserStatusThrottleInterval.name, "2s"}}},
         {features::kGlicOpenPasswordManagerSettingsPageApi, {}},
         {features::kGlicActor,
          {{features::kGlicActorPolicyControlExemption.name, "true"}}}},
        /*disabled_features=*/
        {
            features::kGlicWarming,
            kGlicZeroStateSuggestions,
            features::kGlicDaisyChainNewTabs,
        });
    SetUseElementIdentifiers(false);
  }

  void SetUpOnMainThread() override {
    NonInteractiveGlicApiTest::SetUpOnMainThread();

    histogram_tester = std::make_unique<GlicHistogramTester>();
    user_action_tester = std::make_unique<base::UserActionTester>();
    browser()->GetWindow()->Activate();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(b/447705905): Remove extra logging for debugging.
    vmodule_switches_.InitWithSwitches("glic_focused_browser_manager=1");
    NonInteractiveGlicApiTest::SetUpCommandLine(command_line);
  }

  // Common setup used in some tests.
  void NavigateTabAndOpenGlic(bool open_floating = false) {
    if (open_floating) {
      TrackFloatingGlicInstance();
    }
    // Load the test page in a tab, so that there is some page context.
    RunTestSequence(
        InstrumentTab(kFirstTab), NavigateWebContents(kFirstTab, page_url()),
        Log("Opening Glic window"),
        !open_floating
            ? OpenGlic(GlicInstrumentMode::kHostAndContents,
                       /*conversation_id=*/std::nullopt)
            : OpenGlicFloatingWindow(GlicInstrumentMode::kHostAndContents,
                                     /*conversation_id=*/std::nullopt),
        Log("Done opening glic window"));
  }

  void NavigateTabAndOpenGlicFloating() { NavigateTabAndOpenGlic(true); }

  GlicInstanceCoordinatorImpl& GetInstanceCoordinatorImpl() {
    return static_cast<GlicInstanceCoordinatorImpl&>(
        GetService()->instance_coordinator());
  }

  GURL page_url() {
    return InProcessBrowserTest::embedded_test_server()->GetURL(
        "/glic/browser_tests/test.html");
  }

  GlicInstanceImpl* OpenGlicInNewTabAndGetInstance(
      int index,
      ui::ElementIdentifier tab_id) {
    EXPECT_TRUE(AddTabAtIndex(index, page_url(), ui::PAGE_TRANSITION_TYPED));
    browser()->tab_strip_model()->ActivateTabAt(index);
    TrackGlicInstanceWithTabIndex(index);
    RunTestSequence(
        InstrumentTab(tab_id), OpenGlic(GlicInstrumentMode::kNone),
        RegisterConversation("instance_" + base::NumberToString(index)));
    GlicInstanceImpl* instance = GetGlicInstanceImpl();
    return instance;
  }

  std::unique_ptr<GlicHistogramTester> histogram_tester;
  std::unique_ptr<base::UserActionTester> user_action_tester;

  base::test::ScopedFeatureList features_;
  logging::ScopedVmoduleSwitches vmodule_switches_;
};

class GlicApiTestWithOneTab : public GlicApiTest {
 public:
  explicit GlicApiTestWithOneTab(const GlicTestEnvironmentConfig& config = {})
      : GlicApiTest(base::FieldTrialParams(), config) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {},
        /*disabled_features=*/
        {features::kGlicDefaultTabContextSetting});
  }

  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();

    NavigateTabAndOpenGlic();
  }

  std::string GetDocumentIdForTab(ui::ElementIdentifier tab_id) {
    ui::TrackedElement* const element =
        ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(tab_id);
    CHECK(element);
    content::RenderFrameHost* rfh = AsInstrumentedWebContents(element)
                                        ->web_contents()
                                        ->GetPrimaryMainFrame();
    return optimization_guide::DocumentIdentifierUserData::
        GetDocumentIdentifier(rfh->GetGlobalFrameToken())
            .value();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};


// Test fixture that preloads the web client before starting the test.

// Test fixture that injects GeminiEnterpriseSettings via command line override.
class GlicApiTestGeminiEnterpriseSettingsOverride : public GlicApiTestWithOneTab {
 public:
  GlicApiTestGeminiEnterpriseSettingsOverride() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicGeminiEnterpriseSettingsEnabled);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiTestWithOneTab::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kGlicGeminiEnterpriseSettingsOverride,
        "{\"project_id\": \"switch-project\", \"app_id\": \"switch-engine\", "
        "\"location\": \"switch-location\"}");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

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

class GlicApiTestGeminiEnterpriseSettingsPolicy : public GlicApiTestWithOneTab {
 public:
  GlicApiTestGeminiEnterpriseSettingsPolicy()
      : GlicApiTestWithOneTab(GlicTestEnvironmentConfig{
            .default_account_hosted_domain = "enterprise.com"}) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicGeminiEnterpriseSettingsEnabled);
  }

  void SetUpInProcessBrowserTestFixture() override {
    GlicApiTestWithOneTab::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    policy_provider_.SetupPolicyServiceForPolicyUpdates(
        browser()->GetProfile()->GetProfilePolicyConnector()->policy_service());

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
                 policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                 base::Value(std::move(enterprise_settings)), nullptr);
    policy_provider_.UpdateChromePolicy(policies);

    base::RunLoop().RunUntilIdle();

    GlicApiTestWithOneTab::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    policy_provider_.SetupPolicyServiceForPolicyUpdates(nullptr);
    GlicApiTestWithOneTab::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ::testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

class GlicApiTestGeminiEnterpriseSettingsPolicyUnset
    : public GlicApiTestGeminiEnterpriseSettingsPolicy {
 public:
  GlicApiTestGeminiEnterpriseSettingsPolicyUnset() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kGlicGeminiEnterpriseSettingsEnabled);
  }
 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Note: Test names must match test function names in api_test.ts.

// TODO(harringtond): Many of these tests are minimal, and could be improved
// with additional cases and additional assertions.


// Checks that all tests in api_test.ts have a corresponding test case in this
// file.
// TODO(crbug.com/460826483): Enable on CrOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_testAllTestsAreRegistered DISABLED_testAllTestsAreRegistered
#else
#define MAYBE_testAllTestsAreRegistered testAllTestsAreRegistered
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testAllTestsAreRegistered) {
  AssertAllTestsRegistered(GetTestSuiteNames());
}

// TODO(crbug.com/441588906): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       DISABLED_testFetchInactiveTabScreenshot) {
  // Untested on multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();

  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest();

  browser()->tab_strip_model()->SelectPreviousTab();

  ContinueJsTest();
}

// TODO(crbug.com/460826488): Enable on ChromeOS.
// Win-asan is flaky.
#if (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER))
#define MAYBE_testFetchInactiveTabScreenshotWhileMinimized \
  DISABLED_testFetchInactiveTabScreenshotWhileMinimized
#else
#define MAYBE_testFetchInactiveTabScreenshotWhileMinimized \
  testFetchInactiveTabScreenshotWhileMinimized
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       MAYBE_testFetchInactiveTabScreenshotWhileMinimized) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  RunTestSequence(AddInstrumentedTabAndOpenSidePanel(kSecondTab, page_url()));
  bool can_fetch_screenshot = BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC);

  ExecuteJsTest({.params = base::Value(can_fetch_screenshot)});

  browser()->tab_strip_model()->SelectPreviousTab();
  browser()->GetWindow()->Minimize();

  ContinueJsTest();
}

// TODO(b/498955581): Clean up glic hibernation experiments, and test in the
// coordinator test.
IN_PROC_BROWSER_TEST_P(GlicApiTest, testHibernateAllOnMemoryPressure) {
  ASSERT_TRUE(GetInstanceCoordinator().MaybeStartInitialWarming());

  // Open 3 instances, with instance 2 being the active one.
  GlicInstanceImpl* instance1 = OpenGlicInNewTabAndGetInstance(0, kFirstTab);

  GlicInstanceImpl* instance2 = OpenGlicInNewTabAndGetInstance(1, kSecondTab);
  GlicInstanceImpl* instance3 = OpenGlicInNewTabAndGetInstance(2, kThirdTab);

  // Close instance 3 to make it non-showing and non-actuating.
  RunTestSequence(CloseGlic());
  ASSERT_TRUE(base::test::RunUntil([&]() { return !instance3->IsShowing(); }));
  ASSERT_FALSE(instance3->IsHibernated());

  // Switch back to tab 1, so instance 1 is now active and instance 2 is not
  // showing.
  browser()->tab_strip_model()->ActivateTabAt(0);
  TrackGlicInstanceWithTabIndex(0);
  ASSERT_TRUE(base::test::RunUntil([&]() { return instance1->IsShowing(); }));

  // There is a warmed contents initially. It should be non-showing and
  // non-actuating.
  ASSERT_TRUE(GetInstanceCoordinator().MaybeStartInitialWarming());
  ASSERT_TRUE(GetInstanceCoordinator()
                  .GetWebContentsWarmingPoolForTesting()
                  .HasWarmedContainerForTesting());

  // Simulate memory pressure.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Wait for the non-showing instances to hibernate.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return instance2->IsHibernated() && instance3->IsHibernated();
  }));

  // Verify the warmed contents is reset.
  ASSERT_FALSE(GetInstanceCoordinator()
                   .GetWebContentsWarmingPoolForTesting()
                   .HasWarmedContainerForTesting());

  // Active instance should not be hibernated.
  ASSERT_TRUE(instance1->IsShowing());
  ASSERT_FALSE(instance1->IsHibernated());
}


auto DefaultTestParamSet() {
  return testing::Values(TestParams{});
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicApiTestWithOneTab,
#if defined(SLOW_BINARY)
    // TODO(crbug.com/460826483): Evaluate the feasibility of multi_instance.
    // Even the test setup sometimes doesn't finish on ASAN for multi-instance.
    testing::Values(TestParams{}),
#else
    DefaultTestParamSet(),
#endif
    &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

}  // namespace
}  // namespace glic
