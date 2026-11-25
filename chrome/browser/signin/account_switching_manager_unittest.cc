// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_switching_manager.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/account_switching_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/core/browser/test_account_preview_data_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

class AccountSwitchingManagerTest : public testing::Test {
 public:
  AccountSwitchingManagerTest() {
    profile_ = std::make_unique<TestingProfile>();
    manager_ = AccountSwitchingManagerFactory::GetForProfile(profile_.get());

    // By default, stamp accounts A, B, and C as signed in 28+ days ago so
    // individual tests can focus on the specific rule under test.
    const base::Time past_grace_period =
        base::Time::Now() -
        AccountSwitchingManager::kPrimaryAccountSigninGracePeriod;
    SigninPrefs(*profile_->GetPrefs())
        .SetChromeLastSigninTime(account_a_, past_grace_period);
    SigninPrefs(*profile_->GetPrefs())
        .SetChromeLastSigninTime(account_b_, past_grace_period);
    SigninPrefs(*profile_->GetPrefs())
        .SetChromeLastSigninTime(account_c_, past_grace_period);
  }

 protected:
  struct FeatureInitializer {
    FeatureInitializer() {
      feature_list.InitWithFeatures(
          {switches::kEnableAccountPreviewData,
           switches::kEnableAccountPreviewSwitchingAccount},
          {});
    }
    base::test::ScopedFeatureList feature_list;
  } feature_initializer_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<AccountSwitchingManager> manager_;

  const GaiaId account_a_ = GaiaId("gaia_a");
  const GaiaId account_b_ = GaiaId("gaia_b");
  const GaiaId account_c_ = GaiaId("gaia_c");
  const GaiaId account_d_ = GaiaId("gaia_d");
};

TEST_F(AccountSwitchingManagerTest, InitiallyEligible) {
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
}

TEST_F(AccountSwitchingManagerTest, InvalidAccountsBlocked) {
  EXPECT_FALSE(manager_->CanShowPromo(GaiaId(), account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, GaiaId()));
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_a_));
}

TEST_F(AccountSwitchingManagerTest, PrimaryAccountSigninGracePeriod) {
  // Missing sign-in timestamp blocks eligibility (fail closed).
  EXPECT_FALSE(manager_->CanShowPromo(account_d_, account_b_));

  // Stamping sign-in now starts a 28-day grace period.
  SigninPrefs(*profile_->GetPrefs())
      .SetChromeLastSigninTime(account_d_, base::Time::Now());
  EXPECT_FALSE(manager_->CanShowPromo(account_d_, account_b_));

  // Just before 28 days: still blocked.
  task_environment_.FastForwardBy(
      AccountSwitchingManager::kPrimaryAccountSigninGracePeriod -
      base::Seconds(1));
  EXPECT_FALSE(manager_->CanShowPromo(account_d_, account_b_));

  // Exactly 28 days: eligible.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(manager_->CanShowPromo(account_d_, account_b_));

  // Null timestamp (`"0"`) fails closed rather than treating epoch delta as
  // elapsed.
  SigninPrefs(*profile_->GetPrefs())
      .SetChromeLastSigninTime(account_d_, base::Time());
  EXPECT_FALSE(manager_->CanShowPromo(account_d_, account_b_));

  // Future timestamp (clock skew) also fails closed within grace period.
  SigninPrefs(*profile_->GetPrefs())
      .SetChromeLastSigninTime(account_d_, base::Time::Now() + base::Days(1));
  EXPECT_FALSE(manager_->CanShowPromo(account_d_, account_b_));
}

TEST_F(AccountSwitchingManagerTest, GlobalIgnoreCooldownAndCap) {
  // Ignores 1 through 4 enforce a 7-day global cooldown and then recover.
  for (int i = 0; i < AccountSwitchingManager::kMaxGlobalIgnores - 1; ++i) {
    ASSERT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
    manager_->RecordPromoIgnored();

    // Immediately blocked globally across all pairs.
    EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
    EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_c_));

    // Still blocked 1 second before the 7-day cooldown expires.
    task_environment_.FastForwardBy(
        AccountSwitchingManager::kGlobalIgnoreCooldown - base::Seconds(1));
    EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));

    // Eligible again once the 7-day cooldown expires.
    task_environment_.FastForwardBy(base::Seconds(1));
    EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
    EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_c_));
  }

  // 5th ignore hits the permanent global cap.
  manager_->RecordPromoIgnored();
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_c_));

  // Remains permanently blocked even well past the 7-day cooldown.
  task_environment_.FastForwardBy(base::Days(365));
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_b_, account_c_));
}

TEST_F(AccountSwitchingManagerTest, GlobalExplicitDismissalCooldown) {
  manager_->RecordPromoExplicitlyDismissed(account_a_, account_b_);

  // Blocked globally for 28 days across all pairs (including unrelated pairs).
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_c_));
  EXPECT_FALSE(manager_->CanShowPromo(account_b_, account_a_));

  // Just before 28 days: still blocked.
  task_environment_.FastForwardBy(
      AccountSwitchingManager::kGlobalExplicitDismissalCooldown -
      base::Seconds(1));
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_c_));

  // At 28 days: eligible again (pair dismissal count is only 1 < 2).
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_c_));
}

TEST_F(AccountSwitchingManagerTest, PairExplicitDismissalCapOfTwo) {
  // First dismissal of `(A, B)`.
  manager_->RecordPromoExplicitlyDismissed(account_a_, account_b_);
  task_environment_.FastForwardBy(
      AccountSwitchingManager::kGlobalExplicitDismissalCooldown);
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_TRUE(manager_->CanShowPromo(account_b_, account_a_));

  // Second dismissal in the opposite direction `(B, A)` hits the shared
  // per-pair cap of 2 for `{A, B}`.
  manager_->RecordPromoExplicitlyDismissed(account_b_, account_a_);
  task_environment_.FastForwardBy(
      AccountSwitchingManager::kGlobalExplicitDismissalCooldown);

  // `{A, B}` stays permanently blocked in both directions even after 365 days.
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_b_, account_a_));
  task_environment_.FastForwardBy(base::Days(365));
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_b_, account_a_));

  // Unrelated pairs remain eligible.
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_c_));
  EXPECT_TRUE(manager_->CanShowPromo(account_c_, account_a_));
}

TEST_F(AccountSwitchingManagerTest, SymmetricPairSuppression) {
  // Suppressing `{A, B}` blocks switching prompts in both directions while
  // both accounts remain in the profile.
  SigninPrefs(*profile_->GetPrefs())
      .SetAccountSwitchingPromoSuppressedBySwitch(account_b_, account_a_);

  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_b_, account_a_));
  // Other pairs remain eligible.
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_c_));

  // Suppression persists indefinitely while both accounts remain.
  task_environment_.FastForwardBy(base::Days(365));
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_b_, account_a_));
}

TEST_F(AccountSwitchingManagerTest,
       StartSwitchingBlocksOverlapAndRunsCallbackOnCompletion) {
  ASSERT_TRUE(manager_->CanShowPromo(account_a_, account_b_));

  base::MockOnceClosure first_callback;
  EXPECT_CALL(first_callback, Run()).Times(0);
  EXPECT_TRUE(
      manager_->StartSwitching(account_a_, account_b_, first_callback.Get()));

  // While switching is in progress, promos and overlapping switches are
  // blocked, and the rejected call's callback is not invoked.
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_c_));

  base::MockOnceClosure rejected_callback;
  EXPECT_CALL(rejected_callback, Run()).Times(0);
  EXPECT_FALSE(manager_->StartSwitching(account_a_, account_c_,
                                        rejected_callback.Get()));

  testing::Mock::VerifyAndClearExpectations(&first_callback);
  EXPECT_CALL(first_callback, Run()).Times(1);
  manager_->CompleteSwitchForTesting(/*success=*/true);

  // Once finished, `{A, B}` is suppressed by the completed switch, while
  // eligibility for other pairs and switching are restored (also works without
  // a caller-provided callback).
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_b_, account_a_));
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_c_));
  EXPECT_TRUE(manager_->StartSwitching(account_a_, account_c_));
  manager_->CompleteSwitchForTesting(/*success=*/true);
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_c_));
  EXPECT_TRUE(manager_->CanShowPromo(account_b_, account_c_));
}

TEST_F(AccountSwitchingManagerTest,
       SuccessfulSwitchSuppressesPairBidirectionally) {
  const SigninPrefs signin_prefs(*profile_->GetPrefs());
  ASSERT_FALSE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      account_a_, account_b_));
  ASSERT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
  ASSERT_TRUE(manager_->CanShowPromo(account_b_, account_a_));
  ASSERT_TRUE(manager_->CanShowPromo(account_a_, account_c_));

  base::MockOnceClosure switch_callback;
  EXPECT_CALL(switch_callback, Run()).Times(0);
  ASSERT_TRUE(
      manager_->StartSwitching(account_a_, account_b_, switch_callback.Get()));

  // Suppression is not recorded until the switch actually finishes
  // successfully.
  EXPECT_FALSE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      account_a_, account_b_));

  testing::Mock::VerifyAndClearExpectations(&switch_callback);
  EXPECT_CALL(switch_callback, Run()).Times(1);
  manager_->CompleteSwitchForTesting(/*success=*/true);

  EXPECT_TRUE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      account_a_, account_b_));
  EXPECT_TRUE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      account_b_, account_a_));
  EXPECT_FALSE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      account_a_, account_c_));

  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_b_, account_a_));
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_c_));

  // Suppression persists indefinitely across the 28-day window and beyond.
  task_environment_.FastForwardBy(base::Days(365));
  EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_b_, account_a_));
}

TEST_F(AccountSwitchingManagerTest, AbortedSwitchDoesNotSuppressPair) {
  const SigninPrefs signin_prefs(*profile_->GetPrefs());
  ASSERT_TRUE(manager_->CanShowPromo(account_a_, account_b_));

  base::MockOnceClosure switch_callback;
  EXPECT_CALL(switch_callback, Run()).Times(1);
  ASSERT_TRUE(
      manager_->StartSwitching(account_a_, account_b_, switch_callback.Get()));

  manager_->CompleteSwitchForTesting(/*success=*/false);

  EXPECT_FALSE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      account_a_, account_b_));
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_TRUE(manager_->CanShowPromo(account_b_, account_a_));
}

TEST_F(AccountSwitchingManagerTest,
       PrefObserverStartingNextSwitchPreservesCallbacks) {
  base::MockOnceClosure first_callback;
  base::MockOnceClosure second_callback;
  EXPECT_CALL(first_callback, Run()).Times(0);
  EXPECT_CALL(second_callback, Run()).Times(0);

  bool started_second_switch = false;
  PrefChangeRegistrar registrar;
  registrar.Init(profile_->GetPrefs());
  SigninPrefs::ObserveSigninPrefsChanges(
      registrar, base::BindLambdaForTesting([&]() {
        if (!started_second_switch) {
          started_second_switch = true;
          EXPECT_TRUE(manager_->StartSwitching(account_a_, account_c_,
                                               second_callback.Get()));
        }
      }));

  ASSERT_TRUE(
      manager_->StartSwitching(account_a_, account_b_, first_callback.Get()));

  // Completing the first switch notifies the pref observer synchronously when
  // recording pair suppression. Starting a follow-up switch in that observer
  // must not overwrite or prematurely run either callback.
  testing::Mock::VerifyAndClearExpectations(&first_callback);
  EXPECT_CALL(first_callback, Run()).Times(1);
  manager_->CompleteSwitchForTesting(/*success=*/true);
  EXPECT_TRUE(started_second_switch);

  testing::Mock::VerifyAndClearExpectations(&first_callback);
  testing::Mock::VerifyAndClearExpectations(&second_callback);
  EXPECT_CALL(second_callback, Run()).Times(1);
  manager_->CompleteSwitchForTesting(/*success=*/true);
}

TEST_F(AccountSwitchingManagerTest,
       StartSwitchingSafeAcrossProfileDestruction) {
  // Verify `Shutdown()` cancels an in-progress switch without running the
  // callback or suppressing the pair.
  {
    auto standalone_manager = std::make_unique<AccountSwitchingManager>(
        profile_->GetPrefs(),
        ChromeSigninClientFactory::GetForProfile(profile_.get()),
        IdentityManagerFactory::GetForProfile(profile_.get()),
        AccountPreviewDataServiceFactory::GetForProfile(profile_.get()));
    base::MockOnceClosure shutdown_callback;
    EXPECT_CALL(shutdown_callback, Run()).Times(0);
    ASSERT_TRUE(standalone_manager->StartSwitching(account_a_, account_b_,
                                                   shutdown_callback.Get()));
    standalone_manager->Shutdown();
    EXPECT_FALSE(
        SigninPrefs(*profile_->GetPrefs())
            .IsAccountSwitchingPromoSuppressedBySwitch(account_a_, account_b_));
  }

  // Verify destroying the manager directly while a switch is active also drops
  // the callback without suppressing the pair.
  {
    auto standalone_manager = std::make_unique<AccountSwitchingManager>(
        profile_->GetPrefs(),
        ChromeSigninClientFactory::GetForProfile(profile_.get()),
        IdentityManagerFactory::GetForProfile(profile_.get()),
        AccountPreviewDataServiceFactory::GetForProfile(profile_.get()));
    base::MockOnceClosure destroy_callback;
    EXPECT_CALL(destroy_callback, Run()).Times(0);
    ASSERT_TRUE(standalone_manager->StartSwitching(account_a_, account_b_,
                                                   destroy_callback.Get()));
    standalone_manager.reset();
    EXPECT_FALSE(
        SigninPrefs(*profile_->GetPrefs())
            .IsAccountSwitchingPromoSuppressedBySwitch(account_a_, account_b_));
  }

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run()).Times(0);
  ASSERT_TRUE(manager_->StartSwitching(account_a_, account_b_, callback.Get()));

  manager_ = nullptr;
  profile_.reset();
}

TEST_F(AccountSwitchingManagerTest,
       StartSwitchingIgnoresPromoThrottlingAndEnforcesGuards) {
  // Promo-display throttling (e.g. another window ignored the promo while an
  // eligible promo is already shown) blocks `CanShowPromo()` without blocking
  // an explicit `StartSwitching()`.
  manager_->RecordPromoIgnored();
  ASSERT_FALSE(manager_->CanShowPromo(account_a_, account_b_));

  base::MockOnceClosure switch_callback;
  EXPECT_CALL(switch_callback, Run()).Times(1);
  EXPECT_TRUE(
      manager_->StartSwitching(account_a_, account_b_, switch_callback.Get()));
  manager_->CompleteSwitchForTesting(/*success=*/true);

  // Pair suppression blocks `CanShowPromo()` without blocking explicit manual
  // switching via `StartSwitching()`, even in the reverse direction.
  ASSERT_FALSE(manager_->CanShowPromo(account_b_, account_a_));
  base::MockOnceClosure reverse_switch_callback;
  EXPECT_CALL(reverse_switch_callback, Run()).Times(1);
  EXPECT_TRUE(manager_->StartSwitching(account_b_, account_a_,
                                       reverse_switch_callback.Get()));
  manager_->CompleteSwitchForTesting(/*success=*/true);

  // Structural guards (invalid accounts) still reject `StartSwitching()`.
  base::MockOnceClosure rejected_callback;
  EXPECT_CALL(rejected_callback, Run()).Times(0);
  EXPECT_FALSE(
      manager_->StartSwitching(GaiaId(), account_b_, rejected_callback.Get()));
  EXPECT_FALSE(manager_->StartSwitching(account_a_, account_a_,
                                        rejected_callback.Get()));
}

TEST_F(AccountSwitchingManagerTest,
       ClockRollbackAndNullTimestampsDoNotWedgeCooldowns) {
  {
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                prefs::kAccountSwitchingPromoState);
    update->Set("last_ignored_time",
                base::TimeToValue(base::Time::Now() + base::Days(30)));
    update->Set("last_explicit_dismissal_time",
                base::TimeToValue(base::Time::Now() + base::Days(30)));
  }
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_b_));

  {
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                prefs::kAccountSwitchingPromoState);
    update->Set("last_ignored_time", base::TimeToValue(base::Time()));
    update->Set("last_explicit_dismissal_time",
                base::TimeToValue(base::Time()));
  }
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
}

TEST_F(AccountSwitchingManagerTest,
       DisabledFeatureFlagsBlockPromoAndSwitching) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        switches::kEnableAccountPreviewSwitchingAccount);
    EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
    EXPECT_FALSE(manager_->StartSwitching(account_a_, account_b_));
  }

  // `kEnableAccountPreviewSwitchingAccount` has no effect if
  // `kEnableAccountPreviewData` is disabled, even with `SkipCooldown`.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{switches::kEnableAccountPreviewSwitchingAccount,
          {{switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.name,
            "true"}}}},
        {switches::kEnableAccountPreviewData});
    EXPECT_FALSE(manager_->CanShowPromo(account_a_, account_b_));
    EXPECT_FALSE(manager_->StartSwitching(account_a_, account_b_));

    auto disabled_profile = std::make_unique<TestingProfile>();
    EXPECT_EQ(
        AccountSwitchingManagerFactory::GetForProfile(disabled_profile.get()),
        nullptr);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        switches::kEnableAccountPreviewSwitchingAccount);
    auto disabled_profile = std::make_unique<TestingProfile>();
    EXPECT_EQ(
        AccountSwitchingManagerFactory::GetForProfile(disabled_profile.get()),
        nullptr);
  }
}

TEST_F(AccountSwitchingManagerTest, SwitchBackWindowPlaceholder) {
  // Currently inactive pending b/585132769; must not prematurely block
  // eligibility.
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
}

TEST_F(AccountSwitchingManagerTest,
       SkipCooldownBypassesThrottlingButNotStructuralGuards) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableAccountPreviewSwitchingAccount,
      {{switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.name,
        "true"}});

  // Trigger every throttling / cooldown / pair-suppression rule and leave
  // Account D without `ChromeLastSigninTime`.
  for (int i = 0; i < AccountSwitchingManager::kMaxGlobalIgnores; ++i) {
    manager_->RecordPromoIgnored();
  }
  manager_->RecordPromoExplicitlyDismissed(account_d_, account_b_);
  manager_->RecordPromoExplicitlyDismissed(account_d_, account_b_);
  SigninPrefs(*profile_->GetPrefs())
      .SetAccountSwitchingPromoSuppressedBySwitch(account_b_, account_d_);

  EXPECT_TRUE(manager_->CanShowPromo(account_d_, account_b_));

  // Invalid accounts are still blocked.
  EXPECT_FALSE(manager_->CanShowPromo(account_d_, account_d_));
  EXPECT_FALSE(manager_->CanShowPromo(GaiaId(), account_b_));

  // Ongoing switch guard is still enforced.
  EXPECT_TRUE(manager_->StartSwitching(account_d_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_d_, account_b_));
  EXPECT_FALSE(manager_->StartSwitching(account_d_, account_b_));
  manager_->CompleteSwitchForTesting(/*success=*/true);
  EXPECT_TRUE(manager_->CanShowPromo(account_d_, account_b_));
}

TEST_F(AccountSwitchingManagerTest, RegularAndOffTheRecordProfiles) {
  EXPECT_EQ(AccountSwitchingManagerFactory::GetForProfile(nullptr), nullptr);

  // Regular profile gets a stable non-null instance and registered pref.
  EXPECT_EQ(AccountSwitchingManagerFactory::GetForProfile(profile_.get()),
            manager_);
  EXPECT_NE(
      profile_->GetPrefs()->FindPreference(prefs::kAccountSwitchingPromoState),
      nullptr);

  // Off-the-record (Incognito) and Guest profiles do not get an instance.
  Profile* otr_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(AccountSwitchingManagerFactory::GetForProfile(otr_profile),
            nullptr);

  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest_profile = guest_builder.Build();
  EXPECT_EQ(AccountSwitchingManagerFactory::GetForProfile(guest_profile.get()),
            nullptr);
}

class AccountSwitchingManagerEligibilityTest : public testing::Test {
 public:
  AccountSwitchingManagerEligibilityTest() = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(
        AccountPreviewDataServiceFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<TestAccountPreviewDataService>();
            }));

    profile_ = profile_manager_.CreateTestingProfile("Profile 1",
                                                     std::move(factories));
    other_profile_ = profile_manager_.CreateTestingProfile("Profile 2");

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    preview_service_ = static_cast<TestAccountPreviewDataService*>(
        AccountPreviewDataServiceFactory::GetForProfile(profile_));
    manager_ = AccountSwitchingManagerFactory::GetForProfile(profile_);
    ASSERT_NE(preview_service_, nullptr);
    ASSERT_NE(manager_, nullptr);

    primary_account_ = identity_test_env()->MakePrimaryAccountAvailable(
        "primary@gmail.com", ConsentLevel::kSignin);
    secondary_account_ =
        identity_test_env()->MakeAccountAvailable("secondary@gmail.com");
    third_account_ =
        identity_test_env()->MakeAccountAvailable("third@gmail.com");

    // Stamp the primary account as signed in 28+ days ago so tests start in an
    // eligible baseline state.
    SigninPrefs(*profile_->GetPrefs())
        .SetChromeLastSigninTime(
            primary_account_.GetGaiaId(),
            base::Time::Now() -
                AccountSwitchingManager::kPrimaryAccountSigninGracePeriod);

    // Record `primary_account_` on Profile 1's own attributes entry to mirror
    // real multi-profile state.
    ProfileAttributesEntry* profile_entry = GetProfileEntry(profile_);
    ASSERT_NE(profile_entry, nullptr);
    profile_entry->SetAuthInfo(primary_account_.GetGaiaId(),
                               u"primary@gmail.com",
                               /*is_consented_primary_account=*/false);
  }

  void TearDown() override {
    manager_ = nullptr;
    preview_service_ = nullptr;
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    other_profile_ = nullptr;
  }

 protected:
  IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  ProfileAttributesEntry* GetProfileEntry(Profile* profile) {
    return profile_manager_.profile_manager()
        ->GetProfileAttributesStorage()
        .GetProfileAttributesWithPath(profile->GetPath());
  }

  AccountPreviewDataService::AccountPreviewPreference MakePreference(
      const GaiaId& gaia_id) const {
    AccountPreviewDataService::AccountPreviewPreference pref;
    pref.gaia_id = gaia_id;
    pref.preferred_data_types.push_back(
        {syncer::PASSWORDS, SyncDataQuartile::kAboveQ3});
    pref.other_device_info.form_factor =
        sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;
    pref.other_device_info.enabled_data_types = {syncer::PASSWORDS};
    return pref;
  }

  struct FeatureInitializer {
    FeatureInitializer() {
      feature_list.InitWithFeatures(
          {switches::kEnableAccountPreviewData,
           switches::kEnableAccountPreviewSwitchingAccount},
          {});
    }
    base::test::ScopedFeatureList feature_list;
  } feature_initializer_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<TestingProfile> other_profile_ = nullptr;
  raw_ptr<TestAccountPreviewDataService> preview_service_ = nullptr;
  raw_ptr<AccountSwitchingManager> manager_ = nullptr;

  AccountInfo primary_account_;
  AccountInfo secondary_account_;
  AccountInfo third_account_;
};

TEST_F(AccountSwitchingManagerEligibilityTest,
       TargetAccountSignedInOnOtherProfileBlocksPromoAndSwitching) {
  const GaiaId primary_gaia = primary_account_.GetGaiaId();
  const GaiaId secondary_gaia = secondary_account_.GetGaiaId();
  const GaiaId third_gaia = third_account_.GetGaiaId();

  EXPECT_TRUE(manager_->CanShowPromo(primary_gaia, secondary_gaia));

  // Sign `secondary_gaia` into `other_profile_` as primary.
  ProfileAttributesEntry* other_entry = GetProfileEntry(other_profile_);
  ASSERT_NE(other_entry, nullptr);
  other_entry->SetAuthInfo(secondary_gaia, u"secondary@gmail.com",
                           /*is_consented_primary_account=*/false);

  // Both promo eligibility and switching are blocked for `secondary_gaia`.
  EXPECT_FALSE(manager_->CanShowPromo(primary_gaia, secondary_gaia));
  base::MockOnceClosure rejected_callback;
  EXPECT_CALL(rejected_callback, Run()).Times(0);
  EXPECT_FALSE(manager_->StartSwitching(primary_gaia, secondary_gaia,
                                        rejected_callback.Get()));

  // Unrelated accounts not signed in on another profile remain eligible.
  EXPECT_TRUE(manager_->CanShowPromo(primary_gaia, third_gaia));

  // Even `SkipCooldown` does not bypass the multi-profile structural guard.
  {
    base::test::ScopedFeatureList skip_cooldown_features;
    skip_cooldown_features.InitAndEnableFeatureWithParameters(
        switches::kEnableAccountPreviewSwitchingAccount,
        {{switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.name,
          "true"}});
    EXPECT_FALSE(manager_->CanShowPromo(primary_gaia, secondary_gaia));
    EXPECT_FALSE(manager_->StartSwitching(primary_gaia, secondary_gaia));
  }

  // Omitted ephemeral profiles are ignored by
  // `IsAccountSignedInOnOtherProfile`.
  other_entry->SetIsEphemeral(true);
  other_entry->SetIsOmitted(true);
  EXPECT_TRUE(manager_->CanShowPromo(primary_gaia, secondary_gaia));
  other_entry->SetIsOmitted(false);
  other_entry->SetIsEphemeral(false);
  EXPECT_FALSE(manager_->CanShowPromo(primary_gaia, secondary_gaia));

  // Signing `secondary_gaia` out of `other_profile_` restores eligibility.
  other_entry->SetAuthInfo(GaiaId(), std::u16string(),
                           /*is_consented_primary_account=*/false);
  EXPECT_TRUE(manager_->CanShowPromo(primary_gaia, secondary_gaia));
}

TEST_F(AccountSwitchingManagerEligibilityTest,
       GetEligibleAccountForSwitchingReturnsValidCandidate) {
  const auto pref = MakePreference(secondary_account_.GetGaiaId());
  preview_service_->SetPreferredAccountForSwitching(pref);

  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), pref);
}

TEST_F(
    AccountSwitchingManagerEligibilityTest,
    GetEligibleAccountForSwitchingFiltersStaleCandidateSignedInOnOtherProfile) {
  const auto stale_pref = MakePreference(secondary_account_.GetGaiaId());
  preview_service_->SetPreferredAccountForSwitching(stale_pref);
  ASSERT_EQ(manager_->GetEligibleAccountForSwitching(), stale_pref);

  // Simulate the user signing into `secondary_account_` as primary in another
  // Chrome profile after `prefs::kAccountPreviewSwitchingPreference` was
  // computed, leaving `AccountPreviewDataService` returning a stale candidate.
  ProfileAttributesEntry* other_entry = GetProfileEntry(other_profile_);
  ASSERT_NE(other_entry, nullptr);
  other_entry->SetAuthInfo(secondary_account_.GetGaiaId(),
                           u"secondary@gmail.com",
                           /*is_consented_primary_account=*/false);

  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);

  // `SkipCooldown` must still filter out the stale multi-profile candidate.
  {
    base::test::ScopedFeatureList skip_cooldown_features;
    skip_cooldown_features.InitAndEnableFeatureWithParameters(
        switches::kEnableAccountPreviewSwitchingAccount,
        {{switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.name,
          "true"}});
    EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);
  }

  // Omitting `other_profile_` or signing out restores eligibility.
  other_entry->SetIsEphemeral(true);
  other_entry->SetIsOmitted(true);
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), stale_pref);
  other_entry->SetIsOmitted(false);
  other_entry->SetIsEphemeral(false);
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);

  other_entry->SetAuthInfo(GaiaId(), std::u16string(),
                           /*is_consented_primary_account=*/false);
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), stale_pref);
}

TEST_F(AccountSwitchingManagerEligibilityTest,
       GetEligibleAccountForSwitchingFiltersStaleOrIneligibleState) {
  // No candidate from `AccountPreviewDataService`.
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);

  // Empty Gaia ID candidate is rejected.
  preview_service_->SetPreferredAccountForSwitching(MakePreference(GaiaId()));
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);

  // Valid secondary candidate is eligible.
  const auto valid_pref = MakePreference(secondary_account_.GetGaiaId());
  preview_service_->SetPreferredAccountForSwitching(valid_pref);
  ASSERT_EQ(manager_->GetEligibleAccountForSwitching(), valid_pref);

  // Disabled feature flag blocks eligibility.
  {
    base::test::ScopedFeatureList disabled_features;
    disabled_features.InitAndDisableFeature(
        switches::kEnableAccountPreviewSwitchingAccount);
    EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);
  }

  // Persistent refresh token error on the candidate filters it out.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account_.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);

  // Clearing the error restores eligibility.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account_.GetAccountId(),
      GoogleServiceAuthError::AuthErrorNone());
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), valid_pref);

  // Promo throttling / pair suppression and primary-account state are enforced
  // separately by `CanShowPromo()`, not by `GetEligibleAccountForSwitching()`.
  SigninPrefs(*profile_->GetPrefs())
      .SetAccountSwitchingPromoSuppressedBySwitch(
          secondary_account_.GetGaiaId(), primary_account_.GetGaiaId());
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), valid_pref);
  EXPECT_FALSE(manager_->CanShowPromo(primary_account_.GetGaiaId(),
                                      secondary_account_.GetGaiaId()));
}

TEST_F(AccountSwitchingManagerEligibilityTest,
       GetEligibleAccountForSwitchingFiltersManagedTargetAccount) {
  const auto pref = MakePreference(secondary_account_.GetGaiaId());
  preview_service_->SetPreferredAccountForSwitching(pref);
  ASSERT_EQ(manager_->GetEligibleAccountForSwitching(), pref);

  // Explicit managed hosted domain filters out the candidate.
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(secondary_account_)
          .SetHostedDomain("example.com")
          .Build());
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);

  // Unknown management status on a potentially enterprise domain fails closed.
  AccountInfo custom_account =
      identity_test_env()->MakeAccountAvailable("custom@company.com");
  ASSERT_EQ(custom_account.IsManaged(), Tribool::kUnknown);
  const auto custom_pref = MakePreference(custom_account.GetGaiaId());
  preview_service_->SetPreferredAccountForSwitching(custom_pref);
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);

  // Flex Org account (`IsManaged() == Tribool::kFalse` but
  // `IsMemberOfFlexOrg() == true`) also filters out the candidate.
  custom_account = AccountInfo::Builder(custom_account)
                       .SetHostedDomain(signin::constants::kNoHostedDomainFound)
                       .Build();
  AccountCapabilitiesTestMutator(&custom_account)
      .set_is_subject_to_enterprise_features(true);
  identity_test_env()->UpdateAccountInfoForAccount(custom_account);
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);

  // Confirmed consumer non-well-known account is eligible.
  AccountCapabilitiesTestMutator(&custom_account)
      .set_is_subject_to_enterprise_features(false);
  identity_test_env()->UpdateAccountInfoForAccount(custom_account);
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), custom_pref);
}

TEST_F(AccountSwitchingManagerEligibilityTest,
       GetEligibleAccountForSwitchingFiltersChildTargetAccount) {
  const auto pref = MakePreference(secondary_account_.GetGaiaId());
  preview_service_->SetPreferredAccountForSwitching(pref);
  ASSERT_EQ(manager_->GetEligibleAccountForSwitching(), pref);

  // Child target account is filtered out.
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(secondary_account_)
          .SetIsChildAccount(Tribool::kTrue)
          .Build());
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(secondary_account_)
          .SetIsChildAccount(Tribool::kFalse)
          .Build());
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), pref);
}

TEST_F(AccountSwitchingManagerEligibilityTest,
       GetEligibleAccountForSwitchingFiltersRemovedOrUnloadedAccounts) {
  const auto pref = MakePreference(secondary_account_.GetGaiaId());
  preview_service_->SetPreferredAccountForSwitching(pref);
  ASSERT_EQ(manager_->GetEligibleAccountForSwitching(), pref);

  // Removing the candidate's refresh token filters out the stale candidate.
  identity_test_env()->RemoveRefreshTokenForAccount(
      secondary_account_.GetAccountId());
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);

  // Re-adding the candidate restores eligibility.
  secondary_account_ =
      identity_test_env()->MakeAccountAvailable("secondary@gmail.com");
  ASSERT_EQ(manager_->GetEligibleAccountForSwitching(), pref);

  // Resetting refresh tokens to unloaded filters out the candidate.
  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);
}

TEST_F(AccountSwitchingManagerEligibilityTest,
       SigninDisallowedBlocksPromoAndSwitching) {
  ASSERT_TRUE(manager_->CanShowPromo(primary_account_.GetGaiaId(),
                                     secondary_account_.GetGaiaId()));

  profile_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  EXPECT_FALSE(manager_->CanShowPromo(primary_account_.GetGaiaId(),
                                      secondary_account_.GetGaiaId()));
  EXPECT_FALSE(manager_->StartSwitching(primary_account_.GetGaiaId(),
                                        secondary_account_.GetGaiaId()));

  // `SkipCooldown` must not bypass `prefs::kSigninAllowed`.
  base::test::ScopedFeatureList skip_cooldown_features;
  skip_cooldown_features.InitAndEnableFeatureWithParameters(
      switches::kEnableAccountPreviewSwitchingAccount,
      {{switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.name,
        "true"}});
  EXPECT_FALSE(manager_->CanShowPromo(primary_account_.GetGaiaId(),
                                      secondary_account_.GetGaiaId()));
  EXPECT_FALSE(manager_->StartSwitching(primary_account_.GetGaiaId(),
                                        secondary_account_.GetGaiaId()));
}

TEST_F(AccountSwitchingManagerTest,
       GetEligibleAccountForSwitchingReturnsNulloptWithoutPreviewService) {
  // Default `TestingProfile` does not instantiate `AccountPreviewDataService`.
  EXPECT_EQ(manager_->GetEligibleAccountForSwitching(), std::nullopt);
}

}  // namespace

}  // namespace signin
