// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_policy_checker.h"

#include "base/base_switches.h"
#include "base/test/test_future.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS©_LINUX)

namespace actor {

namespace {

struct TestAccount {
  std::string_view email;
  std::string_view host_domain;
};

constexpr TestAccount kNonEnterpriseAccount = {"foo@testbar.com", ""};
constexpr TestAccount kEnterpriseAccount = {"foo@testenterprise.com",
                                            "testenterprise.com"};
}  // namespace

class ActorPolicyCheckerBrowserTestBase : public ActorToolsTest {
 public:
  ActorPolicyCheckerBrowserTestBase() {
#if !BUILDFLAG(ENABLE_GLIC)
    GTEST_SKIP() << "The policy checker is only tested with GLIC enabled.";
#endif  // BUILDFLAG(ENABLE_GLIC)
    scoped_feature_list_.InitAndEnableFeature(features::kGlicUserStatusCheck);
  }
  ~ActorPolicyCheckerBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    content::SetupCrossSiteRedirector(embedded_test_server());
    ActorToolsTest::SetUpOnMainThread();
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

    ActorToolsTest::TearDownOnMainThread();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     &test_url_loader_factory_));

    ActorToolsTest::SetUpBrowserContextKeyedServices(context);
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
    base::Value::Dict data;
    data.Set("account_id", GetGaiaIdHashBase64());
    data.Set("user_status", 0);
    data.Set("updated_at", base::Time::Now().InSecondsFSinceUnixEpoch());
    data.Set("isEnterpriseAccountDataProtected",
             is_enterprise_account_data_protected);
    GetProfile()->GetPrefs()->SetDict(glic::prefs::kGlicUserStatus,
                                      std::move(data));
  }

 protected:
  bool ShouldForceActOnWeb() override { return false; }
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::ScopedClosureRunner disclaimer_service_resetter_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that exercise the policy checker for non managed browser
// (!browser_management_service->IsManaged()).
class ActorPolicyCheckerBrowserTestNonManagedBrowser
    : public ActorPolicyCheckerBrowserTestBase,
      public ::testing::WithParamInterface<int32_t> {
 public:
  ActorPolicyCheckerBrowserTestNonManagedBrowser() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor, {{features::kGlicActorEligibleTiers.name,
                                base::ToString(kAllowedTier)}});
  }
  ~ActorPolicyCheckerBrowserTestNonManagedBrowser() override = default;

  void SetUpOnMainThread() override {
    ActorPolicyCheckerBrowserTestBase::SetUpOnMainThread();
    auto* management_service_factory =
        policy::ManagementServiceFactory::GetInstance();
    auto* browser_management_service =
        management_service_factory->GetForProfile(GetProfile());
    ASSERT_TRUE(!browser_management_service ||
                !browser_management_service->IsManaged());

    browser()->profile()->GetPrefs()->SetInteger(
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
IN_PROC_BROWSER_TEST_P(ActorPolicyCheckerBrowserTestNonManagedBrowser,
                       CapabilityBasedOnSubscriptionTier) {
  EXPECT_EQ(ActorKeyedService::Get(browser()->profile())
                ->GetPolicyChecker()
                .can_act_on_web(),
            TestHasChromeBenefits());

  // Toggle the pref to kDisabled, but won't change the capability for
  // non-managed clients.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(glic::prefs::kGlicActuationOnWeb,
                    base::to_underlying(
                        glic::prefs::GlicActuationOnWebPolicyState::kDisabled));
  EXPECT_EQ(ActorKeyedService::Get(browser()->profile())
                ->GetPolicyChecker()
                .can_act_on_web(),
            TestHasChromeBenefits());

  // Set the user pref from Allowed to Disallowed or from Disallowed to Allowed.
  browser()->profile()->GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, GetOppositeTier());
  EXPECT_NE(ActorKeyedService::Get(browser()->profile())
                ->GetPolicyChecker()
                .can_act_on_web(),
            TestHasChromeBenefits());
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         ActorPolicyCheckerBrowserTestNonManagedBrowser,
                         ::testing::Values(0, 1));

// Tests that exercise the policy checker for managed browser
// (browser_management_service->IsManaged()).
class ActorPolicyCheckerBrowserTestManagedBrowser
    : public ActorPolicyCheckerBrowserTestBase {
 public:
  ActorPolicyCheckerBrowserTestManagedBrowser() {
    // If the default value is kForcedDisabled, the capability won't be changed
    // by the policy value.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {{features::kGlicActorEnterprisePrefDefault.name,
          features::kGlicActorEnterprisePrefDefault.GetName(
              features::GlicActorEnterprisePrefDefault::kDisabledByDefault)}});
  }
  ~ActorPolicyCheckerBrowserTestManagedBrowser() override = default;

  void SetUpOnMainThread() override {
    policy_provider_.SetupPolicyServiceForPolicyUpdates(
        browser()->profile()->GetProfilePolicyConnector()->policy_service());
    scoped_management_service_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(GetProfile()),
            policy::EnterpriseManagementAuthority::CLOUD);

    ActorPolicyCheckerBrowserTestBase::SetUpOnMainThread();

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
    ActorPolicyCheckerBrowserTestBase::TearDownOnMainThread();
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
    policy::PolicyMap policies;
    std::optional<base::Value> value_to_set;
    if (value.has_value()) {
      value_to_set = base::Value(base::to_underlying(*value));
    }
    policies.Set(policy::key::kGeminiActOnWebSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                 std::move(value_to_set), nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

 private:
  ::testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_management_service_override_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorPolicyCheckerBrowserTestManagedBrowser,
                       TasksDroppedWhenActuationCapabilityIsDisabled) {
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled);
  EXPECT_TRUE(ActorKeyedService::Get(browser()->profile())
                  ->GetPolicyChecker()
                  .can_act_on_web());

  GURL url = embedded_test_server()->GetURL("/empty.html");
  std::unique_ptr<ToolRequest> action =
      MakeNavigateRequest(*active_tab(), url.spec());
  ActResultFuture result;
  task_id_ = CreateNewTask();
  actor_task().Act(ToRequestList(action), result.GetCallback());
  actor_task().Pause(/*from_actor=*/true);
  EXPECT_EQ(actor_task().GetState(), ActorTask::State::kPausedByActor);

  // Since the profile is managed, we can disable the capability by changing
  // the policy.
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kDisabled);
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());

  ExpectErrorResult(result, mojom::ActionResultCode::kTaskPaused);
}

IN_PROC_BROWSER_TEST_F(ActorPolicyCheckerBrowserTestManagedBrowser,
                       CannotCreateTaskWhenActOnWebCapabilityIsDisabled) {
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kDisabled);
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());

  auto null_task_id =
      ActorKeyedService::Get(browser()->profile())->CreateTask();
  EXPECT_EQ(null_task_id, TaskId());
}

// Makes sure that on policy-managed clients, when the default pref is
// kForcedDisabled, the policy value is discarded.
class ActorPolicyCheckerBrowserTestManagedWithForcedDisabledDefaultPref
    : public ActorPolicyCheckerBrowserTestManagedBrowser {
 public:
  ActorPolicyCheckerBrowserTestManagedWithForcedDisabledDefaultPref() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {{features::kGlicActorEnterprisePrefDefault.name,
          features::kGlicActorEnterprisePrefDefault.GetName(
              features::GlicActorEnterprisePrefDefault::kForcedDisabled)}});
  }
  ~ActorPolicyCheckerBrowserTestManagedWithForcedDisabledDefaultPref()
      override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ActorPolicyCheckerBrowserTestManagedWithForcedDisabledDefaultPref,
    CapabilityIsDisabled) {
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled);

  // If the default pref is kForcedDisabled, the policy value is discarded.
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());
}

// Makes sure that on policy-managed clients, when the policy is unset, the
// behavior falls back to the default pref value.
class ActorPolicyCheckerBrowserTestManagedPolicyNotSet
    : public ActorPolicyCheckerBrowserTestManagedBrowser {
 public:
  ActorPolicyCheckerBrowserTestManagedPolicyNotSet() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {{features::kGlicActorEnterprisePrefDefault.name,
          features::kGlicActorEnterprisePrefDefault.GetName(
              features::GlicActorEnterprisePrefDefault::kDisabledByDefault)}});
  }
  ~ActorPolicyCheckerBrowserTestManagedPolicyNotSet() override = default;

  void SetUpOnMainThread() override {
    ActorPolicyCheckerBrowserTestManagedBrowser::SetUpOnMainThread();
    // Since the default policy value is unset, unset->unset won't trigger the
    // pref observer. Explicitly set the policy value here.
    UpdateGeminiActOnWebPolicy(
        glic::prefs::GlicActuationOnWebPolicyState::kEnabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorPolicyCheckerBrowserTestManagedPolicyNotSet,
                       FallbackToDefaultPref) {
  UpdateGeminiActOnWebPolicy(std::nullopt);

  // Policy is unset. Fallback to the default pref value.
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());
}

// Makes sure that on policy-managed clients, when the default pref is not
// kForcedDisabled, the policy value is reflected in the capability.
class ActorPolicyCheckerBrowserTestManagedPolicyChangesCapability
    : public ActorPolicyCheckerBrowserTestManagedBrowser {
 public:
  ActorPolicyCheckerBrowserTestManagedPolicyChangesCapability() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {{features::kGlicActorEnterprisePrefDefault.name,
          features::kGlicActorEnterprisePrefDefault.GetName(
              features::GlicActorEnterprisePrefDefault::kDisabledByDefault)}});
  }
  ~ActorPolicyCheckerBrowserTestManagedPolicyChangesCapability() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ActorPolicyCheckerBrowserTestManagedPolicyChangesCapability,
    PolicyChangesCapability) {
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled);

  EXPECT_TRUE(ActorKeyedService::Get(browser()->profile())
                  ->GetPolicyChecker()
                  .can_act_on_web());

  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kDisabled);

  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());
}

// Exercise the policy checker for managed accounts (AccountInfo::IsManaged())
// on non managed browsers (!browser_management_service->IsManaged()).
class ActorPolicyCheckerBrowserTestWithManagedAccount
    : public ActorPolicyCheckerBrowserTestBase {
 public:
  ActorPolicyCheckerBrowserTestWithManagedAccount() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {{features::kGlicActorEnterprisePrefDefault.name,
          features::kGlicActorEnterprisePrefDefault.GetName(
              features::GlicActorEnterprisePrefDefault::kEnabledByDefault)},
         {features::kGlicActorEligibleTiers.name,
          base::ToString(kAllowedTier)}});
  }
  ~ActorPolicyCheckerBrowserTestWithManagedAccount() override = default;

  void SetUpOnMainThread() override {
    ActorPolicyCheckerBrowserTestBase::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier, kAllowedTier);
  }

 private:
  static constexpr int32_t kAllowedTier = 1;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorPolicyCheckerBrowserTestWithManagedAccount,
                       CapabilityUpdatedForAccount) {
  // No account is signed in, thus no capability.
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());

  // Still no capability, because the account is an enterprise account whose
  // domain is managed.
  SimulatePrimaryAccountChangedSignIn(&kEnterpriseAccount);
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());

// Note: sign-out from enterprise account is not allowed in ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
  ClearPrimaryAccount();
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());

  // Now the account is not an enterprise account, thus has the capability.
  SimulatePrimaryAccountChangedSignIn(&kNonEnterpriseAccount);
  EXPECT_TRUE(ActorKeyedService::Get(browser()->profile())
                  ->GetPolicyChecker()
                  .can_act_on_web());
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_F(ActorPolicyCheckerBrowserTestWithManagedAccount,
                       GlicUserStatusChanged) {
  // No account is signed in, thus no capability.
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());

  // Now the account is not an enterprise account, thus has the capability.
  SimulatePrimaryAccountChangedSignIn(&kNonEnterpriseAccount);
  EXPECT_TRUE(ActorKeyedService::Get(browser()->profile())
                  ->GetPolicyChecker()
                  .can_act_on_web());

  // `isEnterpriseAccountDataProtected = true` disables the capability.
  AddUserStatusPref(/*is_enterprise_account_data_protected=*/true);
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());
}

// Exercise the policy checker for managed accounts (AccountInfo::IsManaged())
// on policymanaged browsers (browser_management_service->IsManaged()).
using ActorPolicyCheckerBrowserTestWithManagedAccountWithPolicy =
    ActorPolicyCheckerBrowserTestManagedBrowser;

IN_PROC_BROWSER_TEST_F(
    ActorPolicyCheckerBrowserTestWithManagedAccountWithPolicy,
    CapabilityUpdatedForAccount) {
  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kDisabled);
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());

// Note: sign-out from enterprise account is not allowed in ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
  ClearPrimaryAccount();
  // No capability because the policy is disabled.
  SimulatePrimaryAccountChangedSignIn(&kNonEnterpriseAccount);
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());

  UpdateGeminiActOnWebPolicy(
      glic::prefs::GlicActuationOnWebPolicyState::kEnabled);
  EXPECT_TRUE(ActorKeyedService::Get(browser()->profile())
                  ->GetPolicyChecker()
                  .can_act_on_web());
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

}  // namespace actor
