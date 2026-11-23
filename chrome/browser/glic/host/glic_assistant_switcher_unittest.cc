// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_assistant_switcher.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/glic/common/glic_relaunch.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "base/callback_list.h"
#include "base/values.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {

mojom::SwitchAssistantResult RunSwitchAssistant(Profile& profile,
                                                mojom::AssistantTarget target) {
  base::test::TestFuture<mojom::SwitchAssistantResponsePtr> future;
  SwitchAssistant(profile, target, future.GetCallback());
  return future.Take()->result;
}

class GlicAssistantSwitcherUnitTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGeic,
          {{"enabled", "true"},
           {"geic-guest-url", "https://business.gemini.google/side-panel"}}}},
        {});
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override { profile_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;

  // Counts restarts requested by `SwitchAssistant()` instead of restarting.
  int relaunch_count_ = 0;
  // Runs, if set, when a restart is requested.
  base::RepeatingClosure on_relaunch_;
  base::AutoReset<base::RepeatingClosure> reset_restart_ =
      SetRestartCallbackForTesting(base::BindLambdaForTesting([this]() {
        ++relaunch_count_;
        if (on_relaunch_) {
          on_relaunch_.Run();
        }
      }));
};

TEST_F(GlicAssistantSwitcherUnitTest, DisallowedByDefaultWithFinchUrl) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(CanSwitchToAnyAssistant(*profile_));
  EXPECT_FALSE(
      CanSwitchAssistant(*profile_, mojom::AssistantTarget::kGeminiConsumer));
  EXPECT_EQ(
      RunSwitchAssistant(*profile_, mojom::AssistantTarget::kGeminiConsumer),
      mojom::SwitchAssistantResult::kErrorNotAllowed);
  EXPECT_EQ(relaunch_count_, 0);
  EXPECT_EQ(profile_->GetPrefs()->GetInteger(prefs::kGlicAssistantSelection),
            std::to_underlying(prefs::AssistantSelection::kAuto));
  histogram_tester.ExpectUniqueSample(
      "Glic.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Enterprise.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
}

TEST_F(GlicAssistantSwitcherUnitTest, RejectsUnknownTarget) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(CanSwitchAssistant(*profile_, mojom::AssistantTarget::kUnknown));
  EXPECT_EQ(RunSwitchAssistant(*profile_, mojom::AssistantTarget::kUnknown),
            mojom::SwitchAssistantResult::kErrorInvalidTarget);
  EXPECT_EQ(relaunch_count_, 0);
  histogram_tester.ExpectUniqueSample(
      "Glic.SwitchAssistant.Result.Unknown",
      mojom::SwitchAssistantResult::kErrorInvalidTarget, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Enterprise.SwitchAssistant.Result.Unknown",
      mojom::SwitchAssistantResult::kErrorInvalidTarget, 1);
}

TEST_F(GlicAssistantSwitcherUnitTest, RejectsOffTheRecordProfile) {
  TestingProfile::Builder otr_builder;
  Profile* otr_profile = otr_builder.BuildIncognito(profile_.get());
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(CanSwitchToAnyAssistant(*otr_profile));
  EXPECT_FALSE(CanSwitchAssistant(*otr_profile,
                                  mojom::AssistantTarget::kGeminiConsumer));
  EXPECT_EQ(
      RunSwitchAssistant(*otr_profile, mojom::AssistantTarget::kGeminiConsumer),
      mojom::SwitchAssistantResult::kErrorNotAllowed);
  EXPECT_EQ(relaunch_count_, 0);
  histogram_tester.ExpectUniqueSample(
      "Glic.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Enterprise.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Sets the GEiC guest URL through a mock `GeminiEnterpriseUrl` policy and
// builds a personal profile that latches with it.
class GlicAssistantSwitcherPolicyTest : public GlicAssistantSwitcherUnitTest {
 public:
  void SetUp() override {
    GlicAssistantSwitcherUnitTest::SetUp();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
  }

  void TearDown() override {
    policy_profile_.reset();
    policy_provider_.Shutdown();
    GlicAssistantSwitcherUnitTest::TearDown();
  }

 protected:
  static constexpr char kPolicyGuestUrl[] =
      "https://business.gemini.google/home/cid/abc123";

  // Sets the `GeminiEnterpriseUrl` policy at `scope`.
  void SetGeicPolicy(policy::PolicyScope scope) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kGeminiEnterpriseUrl,
                 policy::POLICY_LEVEL_MANDATORY, scope,
                 policy::POLICY_SOURCE_CLOUD, base::Value(kPolicyGuestUrl),
                 nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

  // Sets the policy at `scope` and creates a personal profile with `selection`
  // that latches with it.
  TestingProfile* CreatePolicyProfile(
      policy::PolicyScope scope,
      prefs::AssistantSelection selection = prefs::AssistantSelection::kAuto) {
    SetGeicPolicy(scope);
    // The policy pref is set before the provider latches, when keyed services
    // are created.
    base::CallbackListSubscription subscription =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindLambdaForTesting(
                    [selection](content::BrowserContext* context) {
                      PrefService* prefs =
                          Profile::FromBrowserContext(context)->GetPrefs();
                      prefs->SetString(prefs::kGlicGeminiEnterpriseUrl,
                                       kPolicyGuestUrl);
                      prefs->SetInteger(prefs::kGlicAssistantSelection,
                                        std::to_underlying(selection));
                    }));
    policy::PolicyServiceImpl::Providers providers = {&policy_provider_};
    TestingProfile::Builder builder;
    builder.SetPolicyService(
        std::make_unique<policy::PolicyServiceImpl>(std::move(providers)));
    policy_profile_ = builder.Build();
    return policy_profile_.get();
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<TestingProfile> policy_profile_;
};

TEST_F(GlicAssistantSwitcherPolicyTest, SwitchToConsumerAllowed) {
  TestingProfile* profile = CreatePolicyProfile(policy::POLICY_SCOPE_MACHINE);
  profile->GetPrefs()->SetString(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey, kPolicyGuestUrl);
  base::HistogramTester histogram_tester;
  base::test::TestFuture<mojom::SwitchAssistantResponsePtr> future;
  // The callback runs, the result is recorded, and the known provider is
  // cleared before the restart is requested, since the browser may shut down
  // before code after the restart request runs.
  on_relaunch_ = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(future.IsReady());
    histogram_tester.ExpectUniqueSample(
        "Glic.SwitchAssistant.Result.GeminiConsumer",
        mojom::SwitchAssistantResult::kSuccess, 1);
    histogram_tester.ExpectUniqueSample(
        "Glic.Enterprise.SwitchAssistant.Result.GeminiConsumer",
        mojom::SwitchAssistantResult::kSuccess, 1);
    EXPECT_FALSE(profile->GetPrefs()->HasPrefPath(
        prefs::kGlicProviderSwitchNoticeKnownProviderKey));
  });

  EXPECT_TRUE(CanSwitchToAnyAssistant(*profile));
  EXPECT_FALSE(
      CanSwitchAssistant(*profile, mojom::AssistantTarget::kGeminiEnterprise));
  EXPECT_TRUE(
      CanSwitchAssistant(*profile, mojom::AssistantTarget::kGeminiConsumer));
  SwitchAssistant(*profile, mojom::AssistantTarget::kGeminiConsumer,
                  future.GetCallback());
  EXPECT_EQ(future.Take()->result, mojom::SwitchAssistantResult::kSuccess);
  EXPECT_EQ(relaunch_count_, 1);
  EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kGlicAssistantSelection),
            std::to_underlying(prefs::AssistantSelection::kConsumer));
  EXPECT_FALSE(profile->GetPrefs()->HasPrefPath(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey));
}

TEST_F(GlicAssistantSwitcherPolicyTest,
       SwitchToEnterpriseNotAllowedWhenAlreadyLatchedToEnterprise) {
  TestingProfile* profile = CreatePolicyProfile(policy::POLICY_SCOPE_MACHINE);
  profile->GetPrefs()->SetString(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey, kPolicyGuestUrl);
  base::HistogramTester histogram_tester;

  EXPECT_EQ(
      RunSwitchAssistant(*profile, mojom::AssistantTarget::kGeminiEnterprise),
      mojom::SwitchAssistantResult::kErrorNotAllowed);
  EXPECT_EQ(relaunch_count_, 0);
  EXPECT_TRUE(profile->GetPrefs()->HasPrefPath(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey));
  histogram_tester.ExpectUniqueSample(
      "Glic.SwitchAssistant.Result.GeminiEnterprise",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Enterprise.SwitchAssistant.Result.GeminiEnterprise",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
}

TEST_F(GlicAssistantSwitcherPolicyTest, SwitchToEnterpriseAllowed) {
  TestingProfile* profile = CreatePolicyProfile(
      policy::POLICY_SCOPE_MACHINE, prefs::AssistantSelection::kConsumer);
  ASSERT_EQ(GlicProviderLatch::GetProviderForTesting(*profile),
            GlicProvider::kGemini);
  profile->GetPrefs()->SetString(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey, "");
  base::HistogramTester histogram_tester;
  base::test::TestFuture<mojom::SwitchAssistantResponsePtr> future;
  on_relaunch_ = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(future.IsReady());
    histogram_tester.ExpectUniqueSample(
        "Glic.SwitchAssistant.Result.GeminiEnterprise",
        mojom::SwitchAssistantResult::kSuccess, 1);
    histogram_tester.ExpectUniqueSample(
        "Glic.Consumer.SwitchAssistant.Result.GeminiEnterprise",
        mojom::SwitchAssistantResult::kSuccess, 1);
    EXPECT_FALSE(profile->GetPrefs()->HasPrefPath(
        prefs::kGlicProviderSwitchNoticeKnownProviderKey));
  });

  EXPECT_TRUE(CanSwitchToAnyAssistant(*profile));
  EXPECT_FALSE(
      CanSwitchAssistant(*profile, mojom::AssistantTarget::kGeminiConsumer));
  EXPECT_TRUE(
      CanSwitchAssistant(*profile, mojom::AssistantTarget::kGeminiEnterprise));
  SwitchAssistant(*profile, mojom::AssistantTarget::kGeminiEnterprise,
                  future.GetCallback());
  EXPECT_EQ(future.Take()->result, mojom::SwitchAssistantResult::kSuccess);
  EXPECT_EQ(relaunch_count_, 1);
  EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kGlicAssistantSelection),
            std::to_underlying(prefs::AssistantSelection::kEnterprise));
  EXPECT_FALSE(profile->GetPrefs()->HasPrefPath(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey));
}

TEST_F(GlicAssistantSwitcherPolicyTest,
       SwitchToConsumerNotAllowedWhenAlreadyLatchedToConsumer) {
  TestingProfile* profile = CreatePolicyProfile(
      policy::POLICY_SCOPE_MACHINE, prefs::AssistantSelection::kConsumer);
  ASSERT_EQ(GlicProviderLatch::GetProviderForTesting(*profile),
            GlicProvider::kGemini);
  profile->GetPrefs()->SetString(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey, "");
  base::HistogramTester histogram_tester;

  EXPECT_EQ(
      RunSwitchAssistant(*profile, mojom::AssistantTarget::kGeminiConsumer),
      mojom::SwitchAssistantResult::kErrorNotAllowed);
  EXPECT_EQ(relaunch_count_, 0);
  EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kGlicAssistantSelection),
            std::to_underlying(prefs::AssistantSelection::kConsumer));
  EXPECT_TRUE(profile->GetPrefs()->HasPrefPath(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey));
  histogram_tester.ExpectUniqueSample(
      "Glic.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
}

TEST_F(GlicAssistantSwitcherPolicyTest, DisallowedWithUserScopePolicy) {
  TestingProfile* profile = CreatePolicyProfile(policy::POLICY_SCOPE_USER);

  EXPECT_FALSE(CanSwitchToAnyAssistant(*profile));
  EXPECT_FALSE(
      CanSwitchAssistant(*profile, mojom::AssistantTarget::kGeminiConsumer));
  EXPECT_EQ(
      RunSwitchAssistant(*profile, mojom::AssistantTarget::kGeminiConsumer),
      mojom::SwitchAssistantResult::kErrorNotAllowed);
  EXPECT_EQ(relaunch_count_, 0);
}

TEST_F(GlicAssistantSwitcherPolicyTest,
       DisallowedWhenPolicyScopeMovesToUserMidSession) {
  TestingProfile* profile = CreatePolicyProfile(policy::POLICY_SCOPE_MACHINE);
  // The provider latched while the policy was machine-scoped.
  ASSERT_TRUE(GetGlicProviderSpec(profile).CanSwitchBetweenGeminiProviders());

  SetGeicPolicy(policy::POLICY_SCOPE_USER);

  EXPECT_FALSE(CanSwitchToAnyAssistant(*profile));
  EXPECT_FALSE(
      CanSwitchAssistant(*profile, mojom::AssistantTarget::kGeminiConsumer));
  EXPECT_EQ(
      RunSwitchAssistant(*profile, mojom::AssistantTarget::kGeminiConsumer),
      mojom::SwitchAssistantResult::kErrorNotAllowed);
  EXPECT_EQ(relaunch_count_, 0);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace glic
