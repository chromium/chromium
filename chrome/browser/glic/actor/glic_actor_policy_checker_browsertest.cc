// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"

#include <utility>

#include "base/base_switches.h"
#include "base/containers/to_value_list.h"
#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/enterprise_policy_url_checker.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/tabs/public/tab_interface.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS©_LINUX)

using actor::ActorKeyedService;
using actor::ActorTask;
using actor::ActResultFuture;
using actor::MakeClickRequest;
using actor::MakeNavigateRequest;
using actor::TaskId;
using actor::ToolRequest;

namespace glic {

namespace {

struct TestAccount {
  std::string_view email;
  std::string_view host_domain;
};

constexpr TestAccount kNonEnterpriseAccount = {"foo@testbar.com", ""};
constexpr TestAccount kEnterpriseAccount = {"foo@testenterprise.com",
                                            "testenterprise.com"};
}  // namespace

class GlicActorPolicyCheckerBrowserTestBase : public NonInteractiveGlicTest {
 public:
  GlicActorPolicyCheckerBrowserTestBase() {
#if !BUILDFLAG(ENABLE_GLIC)
    GTEST_SKIP() << "The policy checker is only tested with GLIC enabled.";
#endif  // BUILDFLAG(ENABLE_GLIC)
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /* enabled_features = */
        {{features::kGlicActor,
          {{features::kGlicActorPolicyControlExemption.name, "false"}}},
         {features::kGlicUserStatusCheck, {}}},
        /* disabled_features = */ {});
  }
  ~GlicActorPolicyCheckerBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    content::SetupCrossSiteRedirector(embedded_test_server());
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());

    adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());
    identity_test_env_ = adaptor_->identity_test_env();
    identity_test_env_->SetTestURLLoaderFactory(&test_url_loader_factory_);
    identity_manager_ = IdentityManagerFactory::GetForProfile(GetProfile());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    disclaimer_service_resetter_ =
        enterprise_util::DisableAutomaticManagementDisclaimerUntilReset(
            GetProfile());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    identity_manager_ = nullptr;
    identity_test_env_ = nullptr;
    adaptor_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     &test_url_loader_factory_));

    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
  }

  void SimulatePrimaryAccountChangedSignIn(const TestAccount* account) {
    identity_test_env_->SetAutomaticIssueOfAccessTokens(true);

    AccountInfo account_info = identity_test_env_->MakePrimaryAccountAvailable(
        std::string(account->email), signin::ConsentLevel::kSignin);

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    mutator.set_is_subject_to_enterprise_features(
        !account->host_domain.empty());
    identity_test_env_->UpdateAccountInfoForAccount(account_info);

    identity_test_env_->SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        std::string(account->host_domain),
        base::StrCat({"full_name-", account->email}),
        base::StrCat({"given_name-", account->email}),
        base::StrCat({"local-", account->email}),
        base::StrCat({"full_name-", account->email}));
  }

  void ClearPrimaryAccount() { identity_test_env_->ClearPrimaryAccount(); }

  std::string GetGaiaIdHashBase64() {
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile());
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (primary_account.IsEmpty()) {
      return "";
    }
    return signin::GaiaIdHash::FromGaiaId(primary_account.gaia).ToBase64();
  }

  void AddUserStatusPref(bool is_enterprise_account_data_protected) {
    base::DictValue data;
    data.Set("account_id", GetGaiaIdHashBase64());
    data.Set("user_status", 0);
    data.Set("updated_at", base::Time::Now().InSecondsFSinceUnixEpoch());
    data.Set("isEnterpriseAccountDataProtected",
             is_enterprise_account_data_protected);
    GetProfile()->GetPrefs()->SetDict(glic::prefs::kGlicUserStatus,
                                      std::move(data));
  }

  void SetIsLikelyDogfoodClient(bool is_likely_dogfood_client) {
    g_browser_process->variations_service()->SetIsLikelyDogfoodClientForTesting(
        is_likely_dogfood_client);
  }

  ActorKeyedService& GetActorService() {
    auto* actor_service = ActorKeyedService::Get(GetProfile());
    CHECK(actor_service);
    return *actor_service;
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  tabs::TabInterface& active_tab() {
    auto* tab = tabs::TabInterface::GetFromContents(web_contents());
    CHECK(tab);
    return *tab;
  }

  GlicKeyedService* GetGlicKeyedService() {
    return GlicKeyedServiceFactory::GetGlicKeyedService(GetProfile());
  }

  GlicActorPolicyChecker& GetPolicyChecker() {
    return GetGlicKeyedService()->actor_policy_checker();
  }

 protected:
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;

 private:
  glic::GlicTestEnvironment glic_test_environment_{{
      .force_signin_and_glic_capability = false,
  }};
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::ScopedClosureRunner disclaimer_service_resetter_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that exercise the policy checker for non managed browser
// (!browser_management_service->IsManaged()).
class GlicActorPolicyCheckerBrowserTestNonManagedBrowser
    : public GlicActorPolicyCheckerBrowserTestBase,
      public ::testing::WithParamInterface<int32_t> {
 public:
  GlicActorPolicyCheckerBrowserTestNonManagedBrowser() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /* enabled_features = */
        {{features::kGlicActor,
          {{features::kGlicActorEligibleTiers.name,
            base::ToString(kAllowedTier)}}}},
        /* disabled_features = */ {});
  }
  ~GlicActorPolicyCheckerBrowserTestNonManagedBrowser() override = default;

  void SetUpOnMainThread() override {
    GlicActorPolicyCheckerBrowserTestBase::SetUpOnMainThread();
    auto* management_service_factory =
        policy::ManagementServiceFactory::GetInstance();
    auto* browser_management_service =
        management_service_factory->GetForProfile(GetProfile());
    ASSERT_TRUE(!browser_management_service ||
                !browser_management_service->IsManaged());

    GetProfile()->GetPrefs()->SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier, GetParam());

    SimulatePrimaryAccountChangedSignIn(&kNonEnterpriseAccount);
    CoreAccountInfo core_account_info =
        identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    AccountInfo account_info =
        identity_manager_->FindExtendedAccountInfoByAccountId(
            core_account_info.account_id);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_->UpdateAccountInfoForAccount(account_info);
  }

  bool TestHasChromeBenefits() {
    int32_t tier = GetParam();
    return tier == kAllowedTier;
  }

  int32_t GetOppositeTier() {
    if (TestHasChromeBenefits()) {
      return kDisallowedTier;
    }
    return kAllowedTier;
  }

 private:
  static constexpr int32_t kAllowedTier = 1;
  static constexpr int32_t kDisallowedTier = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// On non-managed browsers, the user follows consumer terms, which is based on
// kAiSubscriptionTier.
IN_PROC_BROWSER_TEST_P(GlicActorPolicyCheckerBrowserTestNonManagedBrowser,
                       CapabilityBasedOnSubscriptionTier) {
  EXPECT_EQ(GetPolicyChecker().CanActOnWeb(), TestHasChromeBenefits());
  EXPECT_EQ(GetPolicyChecker().CannotActOnWebReason(),
            TestHasChromeBenefits()
                ? GlicActorPolicyChecker::CannotActReason::kNone
                : GlicActorPolicyChecker::CannotActReason::
                      kAccountMissingChromeBenefits);

  // Toggle the pref to kDisabled, but won't change the capability for
  // non-managed clients.
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetInteger(glic::prefs::kGlicActuationOnWeb,
                    std::to_underlying(
                        glic::prefs::GlicActuationOnWebPolicyState::kDisabled));
  EXPECT_EQ(GetPolicyChecker().CanActOnWeb(), TestHasChromeBenefits());
  EXPECT_THAT(GetPolicyChecker().CannotActOnWebReason(),
              TestHasChromeBenefits()
                  ? GlicActorPolicyChecker::CannotActReason::kNone
                  : GlicActorPolicyChecker::CannotActReason::
                        kAccountMissingChromeBenefits);

  // Set the user pref from Allowed to Disallowed or from Disallowed to Allowed.
  GetProfile()->GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, GetOppositeTier());
  EXPECT_NE(GetPolicyChecker().CanActOnWeb(), TestHasChromeBenefits());
  EXPECT_THAT(GetPolicyChecker().CannotActOnWebReason(),
              TestHasChromeBenefits()
                  ? GlicActorPolicyChecker::CannotActReason::
                        kAccountMissingChromeBenefits
                  : GlicActorPolicyChecker::CannotActReason::kNone);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         GlicActorPolicyCheckerBrowserTestNonManagedBrowser,
                         ::testing::Values(0, 1));

// Tests that exercise the policy checker for managed browser
// (browser_management_service->IsManaged()).
class GlicActorPolicyCheckerBrowserTestManagedBrowser
    : public GlicActorPolicyCheckerBrowserTestBase {
 public:
  GlicActorPolicyCheckerBrowserTestManagedBrowser() {
    // If the default value is kForcedDisabled, the capability won't be changed
    // by the policy value.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /* enabled_features = */
        {{features::kGlicActor,
          {{features::kGlicActorEnterprisePrefDefault.name,
            features::kGlicActorEnterprisePrefDefault.GetName(
                features::GlicActorEnterprisePrefDefault::
                    kDisabledByDefault)}}}},
        /* disabled_features = */ {});
  }
  ~GlicActorPolicyCheckerBrowserTestManagedBrowser() override = default;

  void SetUpOnMainThread() override {
    policy_provider_.SetupPolicyServiceForPolicyUpdates(
        GetProfile()->GetProfilePolicyConnector()->policy_service());
    scoped_management_service_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(GetProfile()),
            policy::EnterpriseManagementAuthority::CLOUD);

    GlicActorPolicyCheckerBrowserTestBase::SetUpOnMainThread();

    auto* management_service_factory =
        policy::ManagementServiceFactory::GetInstance();
    auto* browser_management_service =
        management_service_factory->GetForProfile(GetProfile());
    ASSERT_TRUE(browser_management_service);
    ASSERT_TRUE(browser_management_service->IsManaged());

    // This is a managed browser, so a non-enterprise account must be signed in
    // in order to get the actuation capability.
    SimulatePrimaryAccountChangedSignIn(&kNonEnterpriseAccount);
  }

  void TearDownOnMainThread() override {
    policy_provider_.SetupPolicyServiceForPolicyUpdates(nullptr);
    // `scoped_management_service_override_` points to the profile-scoped
    // `ManagementService`. Destroying it before the profile.
    scoped_management_service_override_.reset();
    GlicActorPolicyCheckerBrowserTestBase::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kNoErrorDialogs);
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void UpdateGeminiActOnWebPolicy(
      std::optional<glic::prefs::GlicActuationOnWebPolicyState> value) {
    UpdateGeminiActOnWebAndUrlPolicies(value, /*url_allowlist=*/{},
                                       /*url_blocklist=*/{},
                                       /*await_list_update=*/false);
  }

  void UpdateGeminiActOnWebAndUrlPolicies(
      std::optional<glic::prefs::GlicActuationOnWebPolicyState> value,
      const std::vector<std::string>& url_allowlist,
      const std::vector<std::string>& url_blocklist,
      bool await_list_update) {
    GlicActorPolicyChecker& policy_checker = GetPolicyChecker();

    policy::PolicyMap policies;
    std::optional<base::Value> value_to_set;
    if (value.has_value()) {
      value_to_set = base::Value(std::to_underlying(*value));
    }
    std::optional<base::Value> allow_list_value =
        base::Value(base::ToValueList(url_allowlist));
    std::optional<base::Value> block_list_value =
        base::Value(base::ToValueList(url_blocklist));
    policies.Set(policy::key::kGeminiActOnWebSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                 std::move(value_to_set), nullptr);
    policies.Set(policy::key::kGeminiActOnWebAllowedForURLs,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                 std::move(allow_list_value), nullptr);
    policies.Set(policy::key::kGeminiActOnWebBlockedForURLs,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                 std::move(block_list_value), nullptr);
    base::test::TestFuture<void> on_url_lists_updated;
    base::CallbackListSubscription list_update_subscription =
        policy_checker.AddUrlListsUpdateObserverForTesting(
            on_url_lists_updated.GetRepeatingCallback());
    policy_provider_.UpdateChromePolicy(policies);

    if (await_list_update) {
      EXPECT_TRUE(on_url_lists_updated.Wait());
    }
  }

  struct PolicyCheckResult {
    actor::MayActOnUrlBlockReason may_act_on_url_block_reason;
    bool can_act_on_web;
  };
  void TestPolicyCombination(
      glic::prefs::GlicActuationOnWebPolicyState actuation_policy,
      const std::vector<std::string>& url_allowlist,
      const std::vector<std::string>& url_blocklist,
      const GURL& url_to_check,
      PolicyCheckResult expected_result) {
    UpdateGeminiActOnWebAndUrlPolicies(actuation_policy, url_allowlist,
                                       url_blocklist,
                                       /*await_list_update=*/true);

    auto* actor_service = ActorKeyedService::Get(GetProfile());
    GlicActorPolicyChecker& policy_checker = GetPolicyChecker();

    EXPECT_EQ(expected_result.can_act_on_web, policy_checker.CanActOnWeb());

    base::test::TestFuture<actor::MayActOnUrlBlockReason> allowed;
    MayActOnUrl(url_to_check, /*allow_insecure_http=*/true, GetProfile(),
                actor_service->GetJournal(), TaskId(123), policy_checker,
                allowed.GetCallback());
    EXPECT_EQ(expected_result.may_act_on_url_block_reason, allowed.Get());
  }

 private:
  ::testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_management_service_override_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorPolicyCheckerBrowserTestManagedBrowser,
                       TasksDroppedWhenActuationCapabilityIsDisabled) {
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled);
  EXPECT_TRUE(GetPolicyChecker().CanActOnWeb());
  EXPECT_EQ(GetPolicyChecker().CannotActOnWebReason(),
            GlicActorPolicyChecker::CannotActReason::kNone);

  GURL url = embedded_test_server()->GetURL("/empty.html");
  std::unique_ptr<ToolRequest> action =
      MakeNavigateRequest(active_tab(), url.spec());
  ActResultFuture result;
  TaskId task_id = GetActorService().CreateTask(&GetPolicyChecker());
  ActorTask* task = GetActorService().GetTask(task_id);
  ASSERT_TRUE(task);

  task->Act(ToRequestList(action), result.GetCallback());
  task->Pause(/*from_actor=*/true);
  EXPECT_EQ(task->GetState(), actor::ActorTask::State::kPausedByActor);

  // Since the profile is managed, we can disable the capability by changing
  // the policy.
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kDisabled);
  EXPECT_FALSE(GetPolicyChecker().CanActOnWeb());
  EXPECT_EQ(GetPolicyChecker().CannotActOnWebReason(),
            GlicActorPolicyChecker::CannotActReason::kDisabledByPolicy);

  ExpectErrorResult(result, actor::mojom::ActionResultCode::kTaskPaused);
}

IN_PROC_BROWSER_TEST_F(GlicActorPolicyCheckerBrowserTestManagedBrowser,
                       BlocklistUrl) {
  TestPolicyCombination(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled,
      /*url_allowlist=*/{},
      /*url_blocklist=*/{"example.com"}, GURL("https://example.com"),
      {
          .may_act_on_url_block_reason =
              actor::MayActOnUrlBlockReason::kEnterprisePolicy,
          .can_act_on_web = true,
      });
}

IN_PROC_BROWSER_TEST_F(GlicActorPolicyCheckerBrowserTestManagedBrowser,
                       AllowlistUrl) {
  // Despite the general policy being disabled, `can_act_on_web` needs to be
  // true, since we need to allow actions at least to the point of checking
  // whether the URL is in the allowlist.
  TestPolicyCombination(glic::prefs::GlicActuationOnWebPolicyState::kDisabled,
                        /*url_allowlist=*/{"example.com"},
                        /*url_blocklist=*/{}, GURL("https://example.com"),
                        {
                            .may_act_on_url_block_reason =
                                actor::MayActOnUrlBlockReason::kAllowed,
                            .can_act_on_web = true,
                        });
}

IN_PROC_BROWSER_TEST_F(GlicActorPolicyCheckerBrowserTestManagedBrowser,
                       NonMatchingAllowlistUrl) {
  // Since we need to check against the allow list, `can_act_on_web` is true,
  // but since the URL wasn't in the allow list, we apply the general policy to
  // disable acting.
  TestPolicyCombination(
      glic::prefs::GlicActuationOnWebPolicyState::kDisabled,
      /*url_allowlist=*/{"a.com"},
      /*url_blocklist=*/{}, GURL("https://example.com"),
      {
          .may_act_on_url_block_reason =
              actor::MayActOnUrlBlockReason::kEnterprisePolicy,
          .can_act_on_web = true,
      });
}

IN_PROC_BROWSER_TEST_F(GlicActorPolicyCheckerBrowserTestManagedBrowser,
                       AllowWhenMatchingBothLists) {
  TestPolicyCombination(glic::prefs::GlicActuationOnWebPolicyState::kEnabled,
                        /*url_allowlist=*/{"example.com"},
                        /*url_blocklist=*/{"example.com"},
                        GURL("https://example.com"),
                        {
                            .may_act_on_url_block_reason =
                                actor::MayActOnUrlBlockReason::kAllowed,
                            .can_act_on_web = true,
                        });
}

IN_PROC_BROWSER_TEST_F(GlicActorPolicyCheckerBrowserTestManagedBrowser,
                       UrlListUpdates) {
  {
    SCOPED_TRACE("Not in allowlist");
    TestPolicyCombination(
        glic::prefs::GlicActuationOnWebPolicyState::kDisabled,
        /*url_allowlist=*/{"a.com"},
        /*url_blocklist=*/{}, GURL("https://example.com"),
        {
            .may_act_on_url_block_reason =
                actor::MayActOnUrlBlockReason::kEnterprisePolicy,
            .can_act_on_web = true,
        });
  }
  {
    SCOPED_TRACE("In allowlist");
    TestPolicyCombination(glic::prefs::GlicActuationOnWebPolicyState::kDisabled,
                            /*url_allowlist=*/{"example.com"},
                            /*url_blocklist=*/{}, GURL("https://example.com"),
                            {
                                .may_act_on_url_block_reason =
                                    actor::MayActOnUrlBlockReason::kAllowed,
                                .can_act_on_web = true,
                            });
    }
}

IN_PROC_BROWSER_TEST_F(GlicActorPolicyCheckerBrowserTestManagedBrowser,
                       BlocklistAppliesToAction) {
  UpdateGeminiActOnWebAndUrlPolicies(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled,
      /*url_allowlist=*/{},
      /*url_blocklist=*/{"bar.com"},
      /*await_list_update=*/true);
  EXPECT_TRUE(GetPolicyChecker().CanActOnWeb());

  const GURL url =
      embedded_https_test_server().GetURL("bar.com", "/actor/two_clicks.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TaskId task_id = GetActorService().CreateTask(&GetPolicyChecker());
  ActorTask* task = GetActorService().GetTask(task_id);
  ASSERT_TRUE(task);

  std::optional<int> button_id =
      GetDOMNodeId(*web_contents()->GetPrimaryMainFrame(), "#button1");
  ASSERT_TRUE(button_id.has_value());
  std::unique_ptr<ToolRequest> click =
      MakeClickRequest(*web_contents()->GetPrimaryMainFrame(), *button_id);

  ActResultFuture result;
  task->Act(ToRequestList(click), result.GetCallback());
  const auto expected_result =
      base::FeatureList::IsEnabled(
          actor::kGlicGranularBlockingActionResultCodes)
          ? actor::mojom::ActionResultCode::kActionsBlockedByEnterprisePolicy
          : actor::mojom::ActionResultCode::kUrlBlocked;
  ExpectErrorResult(result, expected_result);
}

IN_PROC_BROWSER_TEST_F(GlicActorPolicyCheckerBrowserTestManagedBrowser,
                       AllowlistAppliesToAction) {
  UpdateGeminiActOnWebAndUrlPolicies(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled,
      /*url_allowlist=*/{"bar.com"},
      /*url_blocklist=*/{},
      /*await_list_update=*/true);
  EXPECT_TRUE(GetPolicyChecker().CanActOnWeb());

  const GURL url =
      embedded_https_test_server().GetURL("bar.com", "/actor/two_clicks.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TaskId task_id = GetActorService().CreateTask(&GetPolicyChecker());
  ActorTask* task = GetActorService().GetTask(task_id);
  ASSERT_TRUE(task);

  std::optional<int> button_id =
      GetDOMNodeId(*web_contents()->GetPrimaryMainFrame(), "#button1");
  ASSERT_TRUE(button_id.has_value());
  std::unique_ptr<ToolRequest> click =
      MakeClickRequest(*web_contents()->GetPrimaryMainFrame(), *button_id);

  ActResultFuture result;
  task->Act(ToRequestList(click), result.GetCallback());
  ExpectOkResult(result);
}

IN_PROC_BROWSER_TEST_F(GlicActorPolicyCheckerBrowserTestManagedBrowser,
                       BlocklistAppliesToNavigation) {
  UpdateGeminiActOnWebAndUrlPolicies(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled,
      /*url_allowlist=*/{},
      /*url_blocklist=*/{"bar.com"},
      /*await_list_update=*/true);
  EXPECT_TRUE(GetPolicyChecker().CanActOnWeb());

  const GURL cross_origin_url =
      embedded_https_test_server().GetURL("bar.com", "/actor/blank.html");
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "foo.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(cross_origin_url.spec())}));

  ASSERT_TRUE(content::NavigateToURL(web_contents(), link_page_url));

  TaskId task_id = GetActorService().CreateTask(&GetPolicyChecker());
  ActorTask* task = GetActorService().GetTask(task_id);
  ASSERT_TRUE(task);

  std::optional<int> link_id =
      GetDOMNodeId(*web_contents()->GetPrimaryMainFrame(), "#link");
  ASSERT_TRUE(link_id.has_value());
  std::unique_ptr<ToolRequest> click =
      MakeClickRequest(*web_contents()->GetPrimaryMainFrame(), *link_id);

  ActResultFuture result;
  task->Act(ToRequestList(click), result.GetCallback());
  const auto expected_result =
      base::FeatureList::IsEnabled(
          actor::kGlicGranularBlockingActionResultCodes)
          ? actor::mojom::ActionResultCode::kActionsBlockedByEnterprisePolicy
          : actor::mojom::ActionResultCode::kTriggeredNavigationBlocked;
  ExpectErrorResult(result, expected_result);
}

IN_PROC_BROWSER_TEST_F(GlicActorPolicyCheckerBrowserTestManagedBrowser,
                       AllowlistAppliesToNavigation) {
  // The test opt guide blocklist is set up to block the following host. We
  // allow it by enterprise policy. The enterprise policy should take
  // precedence.
  UpdateGeminiActOnWebAndUrlPolicies(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled,
      /*url_allowlist=*/{"blocked.example.com"},
      /*url_blocklist=*/{},
      /*await_list_update=*/true);
  EXPECT_TRUE(GetPolicyChecker().CanActOnWeb());

  const GURL cross_origin_url = embedded_https_test_server().GetURL(
      "blocked.example.com", "/actor/blank.html");
  const GURL link_page_url = embedded_https_test_server().GetURL(
      "foo.com",
      base::StrCat({"/actor/link_full_page.html?href=",
                    url::EncodeUriComponent(cross_origin_url.spec())}));

  ASSERT_TRUE(content::NavigateToURL(web_contents(), link_page_url));

  TaskId task_id = GetActorService().CreateTask(&GetPolicyChecker());
  ActorTask* task = GetActorService().GetTask(task_id);
  ASSERT_TRUE(task);

  std::optional<int> link_id =
      GetDOMNodeId(*web_contents()->GetPrimaryMainFrame(), "#link");
  ASSERT_TRUE(link_id.has_value());
  std::unique_ptr<ToolRequest> click =
      MakeClickRequest(*web_contents()->GetPrimaryMainFrame(), *link_id);

  ActResultFuture result;
  task->Act(ToRequestList(click), result.GetCallback());
  ExpectOkResult(result);
}

// Makes sure that on policy-managed clients, when the default pref is
// kForcedDisabled, the policy value is discarded.
class GlicActorPolicyCheckerBrowserTestManagedWithForcedDisabledDefaultPref
    : public GlicActorPolicyCheckerBrowserTestManagedBrowser {
 public:
  GlicActorPolicyCheckerBrowserTestManagedWithForcedDisabledDefaultPref() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /* enabled_features = */
        {{features::kGlicActor,
          {{features::kGlicActorEnterprisePrefDefault.name,
            features::kGlicActorEnterprisePrefDefault.GetName(
                features::GlicActorEnterprisePrefDefault::kForcedDisabled)}}}},
        /* disabled_features = */ {});
  }
  ~GlicActorPolicyCheckerBrowserTestManagedWithForcedDisabledDefaultPref()
      override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    GlicActorPolicyCheckerBrowserTestManagedWithForcedDisabledDefaultPref,
    CapabilityIsDisabled) {
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled);

  // If the default pref is kForcedDisabled, the policy value is discarded.
  EXPECT_FALSE(GetPolicyChecker().CanActOnWeb());
  EXPECT_EQ(GetPolicyChecker().CannotActOnWebReason(),
            GlicActorPolicyChecker::CannotActReason::kDisabledByPolicy);
}

// Makes sure that on policy-managed clients, when the policy is unset, the
// behavior falls back to the default pref value.
class GlicActorPolicyCheckerBrowserTestManagedPolicyNotSet
    : public GlicActorPolicyCheckerBrowserTestManagedBrowser {
 public:
  GlicActorPolicyCheckerBrowserTestManagedPolicyNotSet() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /* enabled_features = */
        {{features::kGlicActor,
          {{features::kGlicActorEnterprisePrefDefault.name,
            features::kGlicActorEnterprisePrefDefault.GetName(
                features::GlicActorEnterprisePrefDefault::
                    kDisabledByDefault)}}}},
        /* disabled_features = */ {});
  }
  ~GlicActorPolicyCheckerBrowserTestManagedPolicyNotSet() override = default;

  void SetUpOnMainThread() override {
    GlicActorPolicyCheckerBrowserTestManagedBrowser::SetUpOnMainThread();
    // Since the default policy value is unset, unset->unset won't trigger the
    // pref observer. Explicitly set the policy value here.
    UpdateGeminiActOnWebPolicy(
        glic::prefs::GlicActuationOnWebPolicyState::kEnabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorPolicyCheckerBrowserTestManagedPolicyNotSet,
                       FallbackToDefaultPref) {
  UpdateGeminiActOnWebPolicy(std::nullopt);

  // Policy is unset. Fallback to the default pref value.
  EXPECT_FALSE(GetPolicyChecker().CanActOnWeb());
  EXPECT_EQ(GetPolicyChecker().CannotActOnWebReason(),
            GlicActorPolicyChecker::CannotActReason::kDisabledByPolicy);
}

// Makes sure that on policy-managed clients, when the default pref is not
// kForcedDisabled, the policy value is reflected in the capability.
class GlicActorPolicyCheckerBrowserTestManagedPolicyChangesCapability
    : public GlicActorPolicyCheckerBrowserTestManagedBrowser {
 public:
  GlicActorPolicyCheckerBrowserTestManagedPolicyChangesCapability() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /* enabled_features = */
        {{features::kGlicActor,
          {{features::kGlicActorEnterprisePrefDefault.name,
            features::kGlicActorEnterprisePrefDefault.GetName(
                features::GlicActorEnterprisePrefDefault::
                    kDisabledByDefault)}}}},
        /* disabled_features = */ {});
  }
  ~GlicActorPolicyCheckerBrowserTestManagedPolicyChangesCapability() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    GlicActorPolicyCheckerBrowserTestManagedPolicyChangesCapability,
    PolicyChangesCapability) {
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled);

  EXPECT_TRUE(GetPolicyChecker().CanActOnWeb());

  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kDisabled);

  EXPECT_FALSE(GetPolicyChecker().CanActOnWeb());
  EXPECT_EQ(GetPolicyChecker().CannotActOnWebReason(),
            GlicActorPolicyChecker::CannotActReason::kDisabledByPolicy);
}

// Exercise the policy checker for managed accounts (AccountInfo::IsManaged())
// on non managed browsers (!browser_management_service->IsManaged()).
class GlicActorPolicyCheckerBrowserTestWithManagedAccount
    : public GlicActorPolicyCheckerBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  GlicActorPolicyCheckerBrowserTestWithManagedAccount() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /* enabled_features = */
        {{features::kGlicActor,
          {{features::kGlicActorEnterprisePrefDefault.name,
            features::kGlicActorEnterprisePrefDefault.GetName(
                IsPolicyDefaultPrefEnabled()
                    ? features::GlicActorEnterprisePrefDefault::
                          kEnabledByDefault
                    : features::GlicActorEnterprisePrefDefault::
                          kDisabledByDefault)},
           {features::kGlicActorEligibleTiers.name,
            base::ToString(kAllowedTier)}}}},
        /* disabled_features = */ {});
  }
  ~GlicActorPolicyCheckerBrowserTestWithManagedAccount() override = default;

  bool IsPolicyDefaultPrefEnabled() const { return GetParam(); }

  void SetUpOnMainThread() override {
    GlicActorPolicyCheckerBrowserTestBase::SetUpOnMainThread();
    GetProfile()->GetPrefs()->SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier, kAllowedTier);
  }

 private:
  static constexpr int32_t kAllowedTier = 1;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicActorPolicyCheckerBrowserTestWithManagedAccount,
                       CapabilityUpdatedForAccount) {
  // No account is signed in, thus no capability.
  EXPECT_FALSE(GetPolicyChecker().CanActOnWeb());
  EXPECT_EQ(
      GetPolicyChecker().CannotActOnWebReason(),
      GlicActorPolicyChecker::CannotActReason::kAccountCapabilityIneligible);

  // Capability is calculated from the default pref value. Despite the account
  // being an enterprise account, the browser has no management. Fallsback to
  // the default policy pref instead of relying on the policy value.
  SimulatePrimaryAccountChangedSignIn(&kEnterpriseAccount);
  EXPECT_EQ(GetPolicyChecker().CanActOnWeb(), IsPolicyDefaultPrefEnabled());
  if (IsPolicyDefaultPrefEnabled()) {
    EXPECT_EQ(GetPolicyChecker().CannotActOnWebReason(),
              GlicActorPolicyChecker::CannotActReason::kNone);
  } else {
    EXPECT_EQ(
        GetPolicyChecker().CannotActOnWebReason(),
        GlicActorPolicyChecker::CannotActReason::kEnterpriseWithoutManagement);
  }

// Note: sign-out from enterprise account is not allowed in ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
  ClearPrimaryAccount();
  EXPECT_FALSE(GetPolicyChecker().CanActOnWeb());

  // Now the account is not an enterprise account, thus has the capability.
  SimulatePrimaryAccountChangedSignIn(&kNonEnterpriseAccount);
  EXPECT_TRUE(GetPolicyChecker().CanActOnWeb());
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_P(GlicActorPolicyCheckerBrowserTestWithManagedAccount,
                       DataProtectedDogfoodUserCanActOnWeb) {
  SetIsLikelyDogfoodClient(true);
  SimulatePrimaryAccountChangedSignIn(&kNonEnterpriseAccount);
  AddUserStatusPref(/*is_enterprise_account_data_protected=*/true);

  // Dogfood devices are exempted from the data protected check.
  EXPECT_TRUE(GetPolicyChecker().CanActOnWeb());
}

IN_PROC_BROWSER_TEST_P(GlicActorPolicyCheckerBrowserTestWithManagedAccount,
                       CanUseModelExecutionFeaturesCapabilityFalse) {
  SimulatePrimaryAccountChangedSignIn(&kNonEnterpriseAccount);

  CoreAccountInfo core_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(
          core_account_info.account_id);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_use_model_execution_features(false);
  identity_test_env_->UpdateAccountInfoForAccount(account_info);
  EXPECT_FALSE(GetPolicyChecker().CanActOnWeb());
  EXPECT_EQ(
      GetPolicyChecker().CannotActOnWebReason(),
      GlicActorPolicyChecker::CannotActReason::kAccountCapabilityIneligible);
}

IN_PROC_BROWSER_TEST_P(GlicActorPolicyCheckerBrowserTestWithManagedAccount,
                       CanUseModelExecutionFeaturesCapabilityTrue) {
  SimulatePrimaryAccountChangedSignIn(&kNonEnterpriseAccount);

  CoreAccountInfo core_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(
          core_account_info.account_id);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_use_model_execution_features(true);
  identity_test_env_->UpdateAccountInfoForAccount(account_info);
  EXPECT_TRUE(GetPolicyChecker().CanActOnWeb());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GlicActorPolicyCheckerBrowserTestWithManagedAccount,
    ::testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "PolicyDefaultPrefEnabledByDefault"
                        : "PolicyDefaultPrefDisabledByDefault";
    });

// Exercise the policy checker for managed accounts (AccountInfo::IsManaged())
// on policymanaged browsers (browser_management_service->IsManaged()).
using ActorPolicyCheckerBrowserTestWithManagedAccountWithPolicy =
    GlicActorPolicyCheckerBrowserTestManagedBrowser;

IN_PROC_BROWSER_TEST_F(
    ActorPolicyCheckerBrowserTestWithManagedAccountWithPolicy,
    CapabilityUpdatedForAccount) {
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kDisabled);
  EXPECT_FALSE(GetPolicyChecker().CanActOnWeb());
  EXPECT_EQ(GetPolicyChecker().CannotActOnWebReason(),
            GlicActorPolicyChecker::CannotActReason::kDisabledByPolicy);

// Note: sign-out from enterprise account is not allowed in ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
  ClearPrimaryAccount();
  // No capability because the policy is disabled.
  SimulatePrimaryAccountChangedSignIn(&kNonEnterpriseAccount);
  EXPECT_FALSE(GetPolicyChecker().CanActOnWeb());

  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled);
  EXPECT_TRUE(GetPolicyChecker().CanActOnWeb());
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

}  // namespace glic
