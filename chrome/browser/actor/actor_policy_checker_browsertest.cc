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
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

class ActorPolicyCheckerBrowserTestBase : public ActorToolsTest {
 public:
  ActorPolicyCheckerBrowserTestBase() = default;
  ~ActorPolicyCheckerBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    content::SetupCrossSiteRedirector(embedded_test_server());
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
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

  void UpdateProviderPolicy(const policy::PolicyMap& policy) {
    policy::PolicyMap policy_with_defaults = policy.Clone();
    policy_provider_.UpdateChromePolicy(policy_with_defaults);
  }

 protected:
  bool ShouldForceActOnWeb() override { return false; }

 private:
  ::testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// Tests that exercise the policy checker for non-managed clients.
class ActorPolicyCheckerBrowserTestNonManaged
    : public ActorPolicyCheckerBrowserTestBase {
 public:
  ActorPolicyCheckerBrowserTestNonManaged() = default;
  ~ActorPolicyCheckerBrowserTestNonManaged() override = default;

  void SetUpOnMainThread() override {
    ActorPolicyCheckerBrowserTestBase::SetUpOnMainThread();
    auto* management_service_factory =
        policy::ManagementServiceFactory::GetInstance();
    auto* browser_management_service =
        management_service_factory->GetForProfile(GetProfile());
    ASSERT_TRUE(!browser_management_service ||
                !browser_management_service->IsManaged());
  }
};

IN_PROC_BROWSER_TEST_F(ActorPolicyCheckerBrowserTestNonManaged,
                       AlwaysHaveActuationCapability) {
  EXPECT_TRUE(ActorKeyedService::Get(browser()->profile())
                  ->GetPolicyChecker()
                  .can_act_on_web());

  // Toggle the pref to kDisabled, but won't change the capability for
  // non-managed clients.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(glic::prefs::kGlicActuationOnWeb,
                    base::to_underlying(
                        glic::prefs::GlicActuationOnWebPolicyState::kDisabled));

  // Non-managed clients always have the capability.
  EXPECT_TRUE(ActorKeyedService::Get(browser()->profile())
                  ->GetPolicyChecker()
                  .can_act_on_web());
}

// Tests that exercise the policy checker for managed clients.
class ActorPolicyCheckerBrowserTestManaged
    : public ActorPolicyCheckerBrowserTestBase {
 public:
  ActorPolicyCheckerBrowserTestManaged() {
    // If the default value is kForcedDisabled, the capability won't be changed
    // by the policy value.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {{features::kGlicActorEnterprisePrefDefault.name,
          features::kGlicActorEnterprisePrefDefault.GetName(
              features::GlicActorEnterprisePrefDefault::kDisabledByDefault)}});
  }
  ~ActorPolicyCheckerBrowserTestManaged() override = default;

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

IN_PROC_BROWSER_TEST_F(ActorPolicyCheckerBrowserTestManaged,
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

  // Note: because we explicitly paused the task, the result will be
  // `ActionResultCode::kError` instead of `ActionResultCode::kTaskWentAway`.
  // See `ActorTask::OnFinishedAct` for more details.
  ExpectErrorResult(result, mojom::ActionResultCode::kError);
}

IN_PROC_BROWSER_TEST_F(ActorPolicyCheckerBrowserTestManaged,
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

class ActorPolicyCheckerBrowserTestManagedWithForcedDisabledDefaultPref
    : public ActorPolicyCheckerBrowserTestManaged {
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
      glic::prefs::GlicActuationOnWebPolicyState::kDisabled);

  // If the default pref is kForcedDisabled, the policy value is discarded.
  EXPECT_FALSE(ActorKeyedService::Get(browser()->profile())
                   ->GetPolicyChecker()
                   .can_act_on_web());
}

class ActorPolicyCheckerBrowserTestManagedPolicyNotSet
    : public ActorPolicyCheckerBrowserTestManaged {
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
    ActorPolicyCheckerBrowserTestManaged::SetUpOnMainThread();
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

class ActorPolicyCheckerBrowserTestManagedPolicyChangesCapability
    : public ActorPolicyCheckerBrowserTestManaged {
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
    FallbackToDefaultPref) {
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

}  // namespace actor
