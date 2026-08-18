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
      "GlicApiTestWithFastTimeout",
      "GlicApiTestWithOneTabAndCachedUserProfile",

      "GlicApiTestUserStatusCheckTest",
      "GlicApiTestWithOneTabMoreDebounceDelay",
      "GlicGetHostCapabilityApiTest",
      "GlicApiTestWithMqlsIdGetterEnabled",
      "GlicApiTestRuntimeFeatureOff",
      "GlicApiTestWithGeminiActOnWebPolicy",
      "GlicApiTestWithWebContentsWarming",
      "GlicApiTestHibernateAllOnMemoryPressure",
      "GlicApiTestWithDaisyChain",
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

class GlicApiTestWithMqlsIdGetterEnabled : public GlicApiTestWithOneTab {
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


// Test fixture that preloads the web client before starting the test.

class GlicApiTestWithFastTimeout : public GlicApiTest {
 public:
  GlicApiTestWithFastTimeout() {
    features2_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{
            features::kGlicWebClientLoadTimes,
            {
// For slow binaries, use a longer timeout.
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiTest::SetUpCommandLine(command_line);
    // --glic-dev brings behavioral changes, and we'd rather test the default
    // behavior.
    command_line->RemoveSwitch(::switches::kGlicDev);
  }

 private:
  base::test::ScopedFeatureList features2_;
};

class GlicApiTestWithGeminiActOnWebPolicy : public GlicApiTestWithOneTab {
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
    GlicApiTestWithOneTab::SetUpInProcessBrowserTestFixture();
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

    GlicApiTestWithOneTab::SetUpBrowserContextKeyedServices(context);
  }

  void SetUpOnMainThread() override {
    GlicApiTestWithOneTab::SetUpOnMainThread();

    adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());
    identity_test_env_ = adaptor_->identity_test_env();
    identity_test_env_->SetTestURLLoaderFactory(&test_url_loader_factory_);
    identity_manager_ = IdentityManagerFactory::GetForProfile(GetProfile());
    SimulatePrimaryAccountChangedSignIn("foo@bar.com", "");

    GetProfile()->GetPrefs()->SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier, 1);

    policy_provider_.SetupPolicyServiceForPolicyUpdates(
        browser()->GetProfile()->GetProfilePolicyConnector()->policy_service());
  }

  void TearDownOnMainThread() override {
    identity_manager_ = nullptr;
    identity_test_env_ = nullptr;
    adaptor_.reset();
    policy_provider_.SetupPolicyServiceForPolicyUpdates(nullptr);
    GlicApiTestWithOneTab::TearDownOnMainThread();
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

 private:
  // `email` must be non-empty. Empty `host_domain` simulates a consumer
  // account.
  void SimulatePrimaryAccountChangedSignIn(std::string_view email,
                                           std::string_view host_domain) {
    identity_test_env_->SetAutomaticIssueOfAccessTokens(true);

    AccountInfo account_info = identity_test_env_->MakePrimaryAccountAvailable(
        std::string(email), signin::ConsentLevel::kSignin);

    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    mutator.set_is_subject_to_enterprise_features(!host_domain.empty());

    identity_test_env_->UpdateAccountInfoForAccount(account_info);
    identity_test_env_->SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        std::string(host_domain), base::StrCat({"full_name-", email}),
        base::StrCat({"given_name-", email}), base::StrCat({"local-", email}),
        base::StrCat({"full_name-", email}));
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;

  ::testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

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


class GlicApiTestWithDaisyChain : public GlicApiTest {
 public:
  GlicApiTestWithDaisyChain() {
    daisy_chain_features_.InitAndEnableFeature(
        features::kGlicDaisyChainNewTabs);
  }

  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, true);
  }

 private:
  base::test::ScopedFeatureList daisy_chain_features_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testSwitchConversationToLastActiveConversation) {
  RunTestSequence(OpenGlic(GlicInstrumentMode::kHostAndContents,
                           /*conversation_id=*/std::nullopt));

  ExecuteJsTest({.params = base::Value("step1")});

  ASSERT_TRUE(AddTabAtIndex(1, page_url(), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(1);
  TrackGlicInstanceWithTabIndex(1);
  RunTestSequence(InstrumentTab(kSecondTab),
                  OpenGlic(GlicInstrumentMode::kHostAndContents,
                           /*conversation_id=*/std::nullopt));

  ExecuteJsTest({.params = base::Value("step2")});
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester->GetBucketCount(
               "Glic.Interaction.SwitchConversationTarget",
               GlicSwitchConversationTarget::kSwitchedToLastActive) == 1;
  }));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testSwitchConversationToOldConversationInOldInstance) {
  RunTestSequence(OpenGlic(GlicInstrumentMode::kHostAndContents,
                           /*conversation_id=*/std::nullopt));

  ExecuteJsTest({.params = base::Value("step1")});

  ASSERT_TRUE(AddTabAtIndex(1, page_url(), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(1);
  TrackGlicInstanceWithTabIndex(1);
  RunTestSequence(InstrumentTab(kSecondTab),
                  OpenGlic(GlicInstrumentMode::kHostAndContents,
                           /*conversation_id=*/std::nullopt));

  ExecuteJsTest({.params = base::Value("step2")});
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester->GetBucketCount(
               "Glic.Interaction.SwitchConversationTarget",
               GlicSwitchConversationTarget::kSwitchedToNewInstance) == 1;
  }));
  ASSERT_TRUE(AddTabAtIndex(1, page_url(), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(1);
  RunTestSequence(InstrumentTab(kThirdTab),
                  OpenGlic(GlicInstrumentMode::kHostAndContents,
                           /*conversation_id=*/std::nullopt));

  ExecuteJsTest({.params = base::Value("step3")});

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester->GetBucketCount(
               "Glic.Interaction.SwitchConversationTarget",
               GlicSwitchConversationTarget::kSwitchedToExistingInstance) == 1;
  }));
  ContinueJsTest();
}

class GlicApiTestRuntimeFeatureOff : public GlicApiTestWithOneTab {
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
  ExecuteJsTest();

  auto* web_contents = FindGlicWebUIContents();
  // Reach in to `GlicApiHost`'s handler to call a function that's gated by
  // a disabled feature.
  const char* script = R"js(
(()=>{
  const appController = appRouter.glicController;
  if (!appController.webview.host.handler.getModelQualityClientId) {
    return "Method not found";
  }
  appController.webview.host.handler.getModelQualityClientId();
  return "Method called";
})()
)js";
  auto result = content::EvalJs(web_contents->GetPrimaryMainFrame(), script);
  ASSERT_EQ("Method called", result.ExtractString());

  WaitForWebUiState(mojom::WebUiState::kError);
  histogram_tester->ExpectUniqueSample(
      "Glic.Host.WebClientState.OnDestroy",
      11 /*MOJO_PIPE_CLOSED_UNEXPECTEDLY_AFTER_INITIALIZE*/, 1);

  // Verify the reload button works.
  RunTestSequence(
      ClickWebElement(TargetWebContents::kGlicWebUi, "#reload", false));

  WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testPanelActiveWithMicrophone) {
  TrackFloatingGlicInstance();
  // Add another tab and open Floaty.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));

  RunTestSequence(InstrumentTab(kFirstTab),
                  NavigateWebContents(kFirstTab, page_url()),
                  OpenGlicFloatingWindow(GlicInstrumentMode::kHostAndContents,
                                         /*conversation_id=*/std::nullopt));

  ExecuteJsTest();

  GetHost()->OnMicrophoneStatusChanged(mojom::MicrophoneStatus::kListening);

  // Activating the other tab should take focus away from Floaty. Floaty should
  // still remain active.
  browser()->tab_strip_model()->ActivateTabAt(1);
  browser()->GetWindow()->Activate();

  EXPECT_TRUE(GetGlicInstance()->IsActive());

  ContinueJsTest();

  // Pause the microphone and focus on the window. Floaty should not be
  // considered active.
  GetHost()->OnMicrophoneStatusChanged(mojom::MicrophoneStatus::kNotListening);
  browser()->tab_strip_model()->ActivateTabAt(1);
  browser()->GetWindow()->Activate();

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !GetGlicInstance()->IsActive(); }));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithMqlsIdGetterEnabled,
                       testGetModelQualityClientIdFeatureEnabled) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithoutPermission) {
  // In multi-instance mode, we only fetch context from pinned tabs.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  ExecuteJsTest();

  // Should record the respective error to the text mode histogram.
  EXPECT_THAT(
      histogram_tester->GetAllSamplesForPrefix(
          "Glic.Api.GetContextFromFocusedTab.Error"),
      UnorderedElementsAre(Pair(
          "Glic.Api.GetContextFromFocusedTab.Error.Text",
          BucketsAre(Bucket(GlicGetContextFromTabError::
                                kPermissionDeniedContextPermissionNotEnabled,
                            1)))));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromPinnedTabWithoutPermission) {
  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromTab.Error"),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithNoRequestedData) {
  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromFocusedTab.Error"),
              testing::IsEmpty());
}

// Note: Win-ASAN is flaky.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_testGetContextFromFocusedTabWithAllRequestedData \
  DISABLED_testGetContextFromFocusedTabWithAllRequestedData
#else
#define MAYBE_testGetContextFromFocusedTabWithAllRequestedData \
  testGetContextFromFocusedTabWithAllRequestedData
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       MAYBE_testGetContextFromFocusedTabWithAllRequestedData) {
  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromFocusedTab.Error"),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextForActorFromTabWithoutPermission) {
  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextForActorFromTab.Error"),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextForActorFromTabWithRestrictedUrl) {
  // Navigate to an un-focusable internal page.
  RunTestSequence(NavigateWebContents(kFirstTab, chrome::GetSettingsUrl("")));

  ExecuteJsTest();

  // Checks that the correct error was reported.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextForActorFromTab.Error"),
              UnorderedElementsAre(Pair(
                  "Glic.Api.GetContextForActorFromTab.Error.Text",
                  BucketsAre(Bucket(
                      GlicGetContextFromTabError::kPermissionDenied, 1)))));
}

// Note: PDF support is a necessary preconition for this test.
#if BUILDFLAG(ENABLE_PDF)
#define MAYBE_testGetContextFromFocusedTabWithPdfFile \
  testGetContextFromFocusedTabWithPdfFile
#else
#define MAYBE_testGetContextFromFocusedTabWithPdfFile \
  DISABLED_testGetContextFromFocusedTabWithPdfFile
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       MAYBE_testGetContextFromFocusedTabWithPdfFile) {
  RunTestSequence(NavigateWebContents(
      kFirstTab,
      InProcessBrowserTest::embedded_test_server()->GetURL("/pdf/test.pdf")));

  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromFocusedTab.Error"),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithUnFocusablePage) {
  // Navigate to an un-focusable internal page.
  RunTestSequence(NavigateWebContents(kFirstTab, chrome::GetSettingsUrl("")));

  // Web client request focused tab contents.
  ExecuteJsTest();

  // Checks that the correct error was reported.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromFocusedTab.Error"),
              UnorderedElementsAre(Pair(
                  "Glic.Api.GetContextFromFocusedTab.Error.Text",
                  BucketsAre(Bucket(
                      GlicGetContextFromTabError::kPermissionDenied, 1)))));
}

// TODO(crbug.com/454083080): Fix this, it hangs.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, DISABLED_testCaptureScreenshot) {
  ExecuteJsTest();
}


class GlicApiTestWithOneTabAndCachedUserProfile : public GlicApiTestWithOneTab {
 public:
  GlicApiTestWithOneTabAndCachedUserProfile() {
    feature_list_.InitAndEnableFeature(
        features::kGlicEnableCachedGetUserProfileInfo);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTabAndCachedUserProfile,
                       testGetUserProfileInfoCached) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetUserProfileInfoDoesNotDeferWhenInactive) {
  ExecuteJsTest();
}

// TODO(crbug.com/438812885): This is flaky.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, DISABLED_testMetrics) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kGlicClosedCaptioningEnabled, true);

  ExecuteJsTest();
  // Sleeping here is needed so that the calls made from the web client are
  // handled by the browser before the check below.
  sleepWithRunLoop(base::Milliseconds(100));

  histogram_tester->ExpectUniqueSample("Glic.Response.ClosedCaptionsShown",
                                       true, 1);
  EXPECT_EQ(1, user_action_tester->GetActionCount("GlicContextUploadStarted"));
  EXPECT_EQ(1,
            user_action_tester->GetActionCount("GlicContextUploadCompleted"));
  EXPECT_EQ(1, user_action_tester->GetActionCount("GlicReactionModelled"));
  EXPECT_EQ(1, user_action_tester->GetActionCount("GlicResponseStopByUser"));
  histogram_tester->ExpectTotalCount("Glic.FirstReaction.Text.Modelled.Time",
                                     1);
  histogram_tester->ExpectTotalCount("Glic.TabContext.UploadTime", 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testPanelWillOpenHasRecentlyActiveConversations) {
  // Open 3 tabs and register a conversation in each.
  RunTestSequence(InstrumentTab(kFirstTab),
                  NavigateWebContents(kFirstTab, page_url()),
                  OpenGlic(GlicInstrumentMode::kHostAndContents,
                           /*conversation_id=*/std::nullopt));
  ExecuteJsTest({.params = base::Value("instance1")});

  ASSERT_TRUE(AddTabAtIndex(1, page_url(), ui::PAGE_TRANSITION_TYPED));
  TrackGlicInstanceWithTabIndex(1);
  RunTestSequence(InstrumentTab(kSecondTab),
                  OpenGlic(GlicInstrumentMode::kHostAndContents,
                           /*conversation_id=*/std::nullopt));
  ExecuteJsTest({.params = base::Value("instance2")});

  ASSERT_TRUE(AddTabAtIndex(2, page_url(), ui::PAGE_TRANSITION_TYPED));
  TrackGlicInstanceWithTabIndex(2);
  RunTestSequence(OpenGlic(GlicInstrumentMode::kHostAndContents,
                           /*conversation_id=*/std::nullopt));
  ExecuteJsTest({.params = base::Value("instance3")});

  // Activate tabs in a specific order to set recency: 1, 3, 2.
  // Instance 2 will be most recent, then 3, then 1.
  for (int tab_index : {0, 2, 1}) {
    browser()->tab_strip_model()->ActivateTabAt(tab_index);
    tabs::TabInterface* tab =
        browser()->tab_strip_model()->GetTabAtIndex(tab_index);
    GlicInstance* instance = GetService()->GetInstanceForTab(tab);
    ASSERT_TRUE(instance);
    ASSERT_TRUE(base::test::RunUntil([&]() { return instance->IsActive(); }));
  }

  // Open a 4th tab to verify.
  ASSERT_TRUE(AddTabAtIndex(3, page_url(), ui::PAGE_TRANSITION_TYPED));
  TrackGlicInstanceWithTabIndex(3);
  RunTestSequence(OpenGlic(GlicInstrumentMode::kHostAndContents,
                           /*conversation_id=*/std::nullopt));
  ExecuteJsTest({.params = base::Value("instance4")});

  // Open a 5th tab to verify.
  ASSERT_TRUE(AddTabAtIndex(4, page_url(), ui::PAGE_TRANSITION_TYPED));
  TrackGlicInstanceWithTabIndex(4);
  RunTestSequence(OpenGlic(GlicInstrumentMode::kHostAndContents,
                           /*conversation_id=*/std::nullopt));
  ExecuteJsTest({.params = base::Value("verify")});
}

// TODO(crbug.com/517682376): Flaky on ASan, MSan, and Linux/ChromeOS debug.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    (!defined(NDEBUG) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)))
#define MAYBE_testPanelWillOpenHasPromptSuggestion \
  DISABLED_testPanelWillOpenHasPromptSuggestion
#else
#define MAYBE_testPanelWillOpenHasPromptSuggestion \
  testPanelWillOpenHasPromptSuggestion
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       MAYBE_testPanelWillOpenHasPromptSuggestion) {
  // Simulate click on contextual cue with prompt suggestion.
  glic::GlicInvokeOptions options(glic::mojom::InvocationSource::kNudge);
  options.prompts.push_back("Prompt Suggestion");
  glic::GlicKeyedServiceFactory::GetGlicKeyedService(browser()->GetProfile())
      ->Invoke(std::move(options));

  ExecuteJsTest();
}


IN_PROC_BROWSER_TEST_P(GlicApiTestWithDaisyChain,
                       testDaisyChainRecursiveAndInput) {
  RunTestSequence(InstrumentTab(kFirstTab),
                  NavigateWebContents(kFirstTab, page_url()),
                  OpenGlic(GlicInstrumentMode::kHostAndContents));

  // 1. Trigger "createTab" from the first tab's Glic panel.
  ExecuteJsTest({.params = base::Value("createTab")});

  // 2. Verify new tab opened and switch to it.
  auto* tab_strip = browser()->tab_strip_model();
  ASSERT_TRUE(base::test::RunUntil([&]() { return tab_strip->count() == 2; }));
  tab_strip->ActivateTabAt(1);

  // 3. Wait for Glic to open in the new (second) tab.
  TrackGlicInstanceWithTabIndex(1);
  WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents);

  // 4. Verify no action yet.
  histogram_tester->ExpectTotalCount(
      "Glic.Instance.AutoOpenedPanel.FirstAction.GlicContents", 0);

  // 5. Trigger "createTab" (recursive) from the second tab's panel.
  ExecuteJsTest({.params = base::Value("createTab")});

  // 6. Verify third tab opened.
  ASSERT_TRUE(base::test::RunUntil([&]() { return tab_strip->count() == 3; }));
  tab_strip->ActivateTabAt(2);

  // 7. Verify recursive metric for the second tab (which was daisy chained).
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester->GetBucketCount(
               "Glic.Instance.AutoOpenedPanel.FirstAction.GlicContents",
               DaisyChainFirstAction::kRecursiveDaisyChain) == 1;
  }));

  // 8. Open Glic in the new (third) tab.
  TrackGlicInstanceWithTabIndex(2);
  WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents);

  // 9. Trigger "inputSubmitted" in the third tab's panel.
  ExecuteJsTest({.params = base::Value("inputSubmitted")});

  // 10. Verify inputSubmitted metric for the third tab.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester->GetBucketCount(
               "Glic.Instance.AutoOpenedPanel.FirstAction.GlicContents",
               DaisyChainFirstAction::kInputSubmitted) == 1;
  }));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithDaisyChain, testNewTabMetrics) {
  // 1. Open Glic in first tab.
  RunTestSequence(InstrumentTab(kFirstTab),
                  NavigateWebContents(kFirstTab, page_url()),
                  OpenGlic(GlicInstrumentMode::kHostAndContents));

  // 2. Open a new tab (Ctrl+T equivalent).
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL("chrome://newtab/"), ui::PAGE_TRANSITION_TYPED));
  auto* tab_strip = browser()->tab_strip_model();
  ASSERT_TRUE(base::test::RunUntil([&]() { return tab_strip->count() == 2; }));
  tab_strip->ActivateTabAt(1);

  // 3. Verify Glic is open in the new tab.
  TrackGlicInstanceWithTabIndex(1);
  WaitForAndInstrumentGlic(GlicInstrumentMode::kHostAndContents);

  // 4. Trigger "inputSubmitted".
  ExecuteJsTest({.params = base::Value("inputSubmitted")});

  // 5. Verify Metric.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester->GetBucketCount(
               "Glic.Instance.AutoOpenedPanel.FirstAction.NewTab",
               DaisyChainFirstAction::kInputSubmitted) == 1;
  }));
}

// TODO(crbug.com/508719420): Flaky time out.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout,
                       DISABLED_testNavigateToAboutBlank) {
  // Client loads, and navigates to a new URL. We try to load the client again,
  // but it fails.
  RunTestSequence(OpenGlic(GlicInstrumentMode::kHostAndContents));
  WebUIStateListener listener(GetHost());
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest();
  listener.WaitForWebUiState(mojom::WebUiState::kError);
}

// TODO(crbug.com/410881522): Re-enable this test
IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout,
                       DISABLED_testNavigateToBadPage) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  // Client loads, and navigates to a new URL. We try to load the client again,
  // but it fails.
  RunTestSequence(OpenGlic(GlicInstrumentMode::kHostAndContents),
                  RegisterConversation("test-id"));
  WebUIStateListener listener(GetHost());
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(0)});
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  listener.WaitForWebUiState(mojom::WebUiState::kError);

  // Open the glic window to trigger reloading the client.
  // This time the client should load, falling back to the original URL.
  RunTestSequence(OpenGlic(GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest({.params = base::Value(1)});
#endif
}


// TODO(crbug.com/454001121): Re-enable after fixing.
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       DISABLED_testTabDataUpdateOnUrlChangeForPinnedTab) {
  NavigateTabAndOpenGlicFloating();
  const int tab_id =
      GetTabId(browser()->tab_strip_model()->GetActiveWebContents());
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest({.params = base::Value(base::DictValue().Set(
                     "tabId", base::NumberToString(tab_id)))});

  // Navigate to another page in the first tab.
  GURL new_url = embedded_test_server()->GetURL(
      "/glic/browser_tests/test.html?changed=true");
  RunTestSequence(NavigateWebContents(kFirstTab, new_url));

  ContinueJsTest();
}

// TODO(crbug.com/454001121): Re-enable after fixing.
IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       DISABLED_testTabDataUpdateOnFaviconChangeForPinnedTab) {
  NavigateTabAndOpenGlicFloating();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  const int tab_id = GetTabId(web_contents);
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest({.params = base::Value(base::DictValue().Set(
                     "tabId", base::NumberToString(tab_id)))});

  // Add favicon to the webcontents.
  const char* script =
      "var link = document.createElement('link');"
      "link.rel = 'icon';"
      "link.href= '../../../glic/youtube_favicon_16x16.png';"
      "document.head.appendChild(link);";
  ASSERT_TRUE(content::ExecJs(web_contents, script));

  ContinueJsTest();
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

class GlicApiTestUserStatusCheckTest : public GlicApiTestWithOneTab {
 protected:
  void SetUpOnMainThread() override {
    GlicApiTestWithOneTab::SetUpOnMainThread();
    GetService()->enabling().SetUserStatusFetchOverrideForTest(
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

IN_PROC_BROWSER_TEST_P(GlicApiTestUserStatusCheckTest,
                       testMaybeRefreshUserStatus) {
  Profile* profile = browser()->GetProfile();
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile),
      policy::EnterpriseManagementAuthority::CLOUD);
  UpdatePrimaryAccountToBeManaged(profile);

  ASSERT_FALSE(GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin());
  user_status_.user_status_code = UserStatusCode::DISABLED_BY_ADMIN;
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

  Profile* profile = browser()->GetProfile();
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile),
      policy::EnterpriseManagementAuthority::CLOUD);
  UpdatePrimaryAccountToBeManaged(profile);

  ASSERT_FALSE(GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin());
  user_status_.user_status_code = UserStatusCode::ENABLED;
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
  EXPECT_LT(user_status_fetch_count_, 5u)
      << "We should not send most of the fetches";
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testSwitchConversationToExistingInstance) {
  // Open glic. It will register a conversation.
  ExecuteJsTest({.params = base::Value("first")});

  // Open a second tab and second glic instance. It will switch conversations
  // resulting in deleting the second glic instance.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(1);
  TrackGlicInstanceWithTabIndex(1);
  RunTestSequence(InstrumentTab(kSecondTab),
                  OpenGlic(GlicInstrumentMode::kHostAndContents,
                           /*conversation_id=*/std::nullopt));
  ExecuteJsTest({.params = base::Value("second")});

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetInstanceCoordinatorImpl().GetInstances().size() == 1u;
  }));
  ASSERT_EQ("id_hello", GetGlicInstanceImpl()->conversation_id());

  // This should continue the test in the first instance, because tab 2 is now
  // bound to that instance.
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

IN_PROC_BROWSER_TEST_P(GlicApiTest, testPanelWillOpenBeforeClientReady) {
  RunTestSequence(InstrumentTab(kFirstTab),
                  OpenGlic(GlicInstrumentMode::kNone));
  Host::PanelWillOpenOptions options;
  options.conversation_info = mojom::ConversationInfo::New();
  options.conversation_info->conversation_id = "test_conversation_id";
  options.conversation_info->conversation_title = "Test Conversation Title";
  options.conversation_info->client_data = "test_client_data_from_cc";
  ASSERT_FALSE(GetHost()->IsWebClientConnected());
  GetHost()->PanelWillOpen(mojom::InvocationSource::kTopChromeButton,
                           std::move(options));
  ExecuteJsTest();
}

class GlicGetHostCapabilityApiTest : public GlicApiTestWithOneTab {
 public:
  GlicGetHostCapabilityApiTest()
      : GlicApiTestWithOneTab(
            {.fre_status = GetParam().onboarding_needed
                               ? prefs::FreStatus::kNotStarted
                               : prefs::FreStatus::kCompleted}) {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (GetParam().enable_scroll_to_pdf) {
      enabled_features.push_back(
          {features::kGlicScrollTo, {{"glic-scroll-to-pdf", "true"}}});
    } else {
      disabled_features.push_back(features::kGlicScrollTo);
    }

    enabled_features.push_back({features::kGlicMultiInstance, {}});
    enabled_features.push_back({mojom::features::kGlicMultiTab, {}});
    enabled_features.push_back({features::kGlicMultitabUnderlines, {}});

    if (GetParam().auto_open_pdf) {
      enabled_features.push_back(
          {features::kAutoOpenGlicForPdf,
           {{"AutoOpenGlicForPdfWithOnboarding", "true"}}});
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  ~GlicGetHostCapabilityApiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicGetHostCapabilityApiTest, testGetHostCapabilities) {
  base::ListValue expected_capabilities;
  if (GetParam().enable_scroll_to_pdf) {
#if BUILDFLAG(ENABLE_PDF)
    expected_capabilities.Append(
        std::to_underlying(mojom::HostCapability::kScrollToPdf));
#endif
  }
  if (GetParam().onboarding_needed) {
    expected_capabilities.Append(
        std::to_underlying(mojom::HostCapability::kTrustFirstOnboardingArm2));
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
  if (base::FeatureList::IsEnabled(features::kGlicWebDragAndDropFileUpload)) {
    expected_capabilities.Append(
        std::to_underlying(mojom::HostCapability::kImgWebDragDrop));
  }
  ExecuteJsTest({.params = base::Value(std::move(expected_capabilities))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testAdditionalContext) {
  // Runs the JS test until the first `advanceToNextStep()`.
  ExecuteJsTest();

  // The JS test is now paused. We can now send the additional context.
  auto context = mojom::AdditionalContext::New();
  std::vector<mojom::AdditionalContextPartPtr> parts;
  {
    auto context_data = mojom::ContextData::New();
    context_data->mime_type = "text/plain";
    context_data->data =
        mojo_base::BigBuffer(std::vector<uint8_t>{'t', 'e', 's', 't'});
    parts.push_back(
        mojom::AdditionalContextPart::NewData(std::move(context_data)));
  }
  {
    auto screenshot = mojom::Screenshot::New();
    screenshot->width_pixels = 10;
    screenshot->height_pixels = 20;
    screenshot->mime_type = "image/png";
    screenshot->data = std::vector<uint8_t>{1, 2, 3, 4};
    screenshot->origin_annotations = mojom::ImageOriginAnnotations::New();
    parts.push_back(
        mojom::AdditionalContextPart::NewScreenshot(std::move(screenshot)));
  }

  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kFirstTab);
  auto* web_contents = AsInstrumentedWebContents(element)->web_contents();
  context->name = "part with everything";
  context->tab_id = GetTabId(web_contents);
  context->origin = url::Origin::Create(web_contents->GetLastCommittedURL());
  context->frameUrl = web_contents->GetLastCommittedURL();

  {
    auto web_page_data = mojom::WebPageData::New();
    web_page_data->main_document = mojom::DocumentData::New();
    web_page_data->main_document->origin =
        url::Origin::Create(context->frameUrl.value());
    web_page_data->main_document->inner_text = "some inner text";
    web_page_data->main_document->inner_text_truncated = false;
    parts.push_back(
        mojom::AdditionalContextPart::NewWebPageData(std::move(web_page_data)));
  }

  {
    parts.push_back(mojom::AdditionalContextPart::NewAnnotatedPageData(
        mojom::AnnotatedPageData::New()));
  }

  {
    auto pdf_data = mojom::PdfDocumentData::New();
    pdf_data->origin = url::Origin::Create(context->frameUrl.value());
    pdf_data->pdf_size_limit_exceeded = false;
    pdf_data->pdf_data = std::vector<uint8_t>{'p', 'd', 'f'};
    parts.push_back(
        mojom::AdditionalContextPart::NewPdfDocumentData(std::move(pdf_data)));
  }

  {
    auto tab_data = mojom::TabData::New();
    tab_data->tab_id = 1;
    tab_data->window_id = 2;
    tab_data->url = GURL("https://google.com");
    auto tab_context = mojom::TabContextResult::New();
    tab_context->tab_data = std::move(tab_data);
    parts.push_back(
        mojom::AdditionalContextPart::NewTabContext(std::move(tab_context)));
  }

  {
    auto region = mojom::CapturedRegion::NewRect(gfx::Rect(10, 20, 30, 40));
    parts.push_back(mojom::AdditionalContextPart::NewRegion(std::move(region)));
  }

  context->parts = std::move(parts);

  auto* tab = tabs::TabInterface::GetFromContents(web_contents);
  ASSERT_TRUE(tab);
  auto* instance = GetService()->GetInstanceForTab(tab);
  ASSERT_TRUE(instance);
  instance->SendAdditionalContext(std::move(context));

  // Continue the JS test to verify the additional context is received.
  ContinueJsTest();
}


IN_PROC_BROWSER_TEST_P(GlicApiTestWithGeminiActOnWebPolicy,
                       testNotifyActOnWebCapabilityChanged) {
  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override(
          policy::ManagementServiceFactory::GetForProfile(GetProfile()),
          policy::EnterpriseManagementAuthority::CLOUD);

  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled);

  // Runs the JS test until the first `advanceToNextStep()`.
  ExecuteJsTest();
  // Disable the capability.
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kDisabled);
  ContinueJsTest();
}

INSTANTIATE_TEST_SUITE_P(,
                         GlicGetHostCapabilityApiTest,
                         testing::Values(TestParams{},
                                         TestParams{
                                             .enable_scroll_to_pdf = true},
                                         TestParams{.onboarding_needed = true},
                                         TestParams{.onboarding_needed = true,
                                                    .auto_open_pdf = true}),
                         &WithTestParams::PrintTestVariant);

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
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithMqlsIdGetterEnabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithOneTabAndCachedUserProfile,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithFastTimeout,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestRuntimeFeatureOff,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestUserStatusCheckTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithGeminiActOnWebPolicy,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithDaisyChain,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

}  // namespace
}  // namespace glic
