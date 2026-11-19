// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_switching_manager.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/signin/account_switching_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
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

  // At 28 days: eligible again.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_c_));
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
  manager_->CompleteSwitchForTesting();

  // Once finished, eligibility and switching are restored (also works without a
  // caller-provided callback).
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
  EXPECT_TRUE(manager_->StartSwitching(account_a_, account_b_));
  manager_->CompleteSwitchForTesting();
  EXPECT_TRUE(manager_->CanShowPromo(account_a_, account_b_));
}

TEST_F(AccountSwitchingManagerTest,
       StartSwitchingSafeAcrossProfileDestruction) {
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
  manager_->CompleteSwitchForTesting();

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

  // Trigger global throttling rules and leave Account D without
  // `ChromeLastSigninTime`.
  for (int i = 0; i < AccountSwitchingManager::kMaxGlobalIgnores; ++i) {
    manager_->RecordPromoIgnored();
  }
  manager_->RecordPromoExplicitlyDismissed(account_d_, account_b_);

  EXPECT_TRUE(manager_->CanShowPromo(account_d_, account_b_));

  // Invalid accounts are still blocked.
  EXPECT_FALSE(manager_->CanShowPromo(account_d_, account_d_));
  EXPECT_FALSE(manager_->CanShowPromo(GaiaId(), account_b_));

  // Ongoing switch guard is still enforced.
  EXPECT_TRUE(manager_->StartSwitching(account_d_, account_b_));
  EXPECT_FALSE(manager_->CanShowPromo(account_d_, account_b_));
  EXPECT_FALSE(manager_->StartSwitching(account_d_, account_b_));
  manager_->CompleteSwitchForTesting();
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

}  // namespace

}  // namespace signin
