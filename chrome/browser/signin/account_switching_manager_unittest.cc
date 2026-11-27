// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_switching_manager.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/core/browser/test_account_preview_data_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
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
  AccountSwitchingManagerTest() = default;

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
    fourth_account_ =
        identity_test_env()->MakeAccountAvailable("fourth@gmail.com");

    // Stamp accounts A, B, and C as signed in 28+ days ago so tests start in an
    // eligible baseline state; leave `fourth_account_` unstamped for grace
    // period tests.
    const base::Time past_grace_period =
        base::Time::Now() -
        AccountSwitchingManager::kPrimaryAccountSigninGracePeriod;
    SigninPrefs signin_prefs(*profile_->GetPrefs());
    signin_prefs.SetChromeLastSigninTime(primary_account_.GetGaiaId(),
                                         past_grace_period);
    signin_prefs.SetChromeLastSigninTime(secondary_account_.GetGaiaId(),
                                         past_grace_period);
    signin_prefs.SetChromeLastSigninTime(third_account_.GetGaiaId(),
                                         past_grace_period);

    // Record `primary_account_` on Profile 1's own attributes entry to mirror
    // real multi-profile state.
    ProfileAttributesEntry* profile_entry = GetProfileEntry(profile_);
    ASSERT_NE(profile_entry, nullptr);
    profile_entry->SetAuthInfo(primary_account_.GetGaiaId(),
                               u"primary@gmail.com",
                               /*is_consented_primary_account=*/false);

    SetUpMockPromoBubble();
    SetCandidate(secondary_account_.GetGaiaId());
  }

  void TearDown() override {
    last_promo_result_callback_.Reset();
    show_bubble_callback_override_.reset();
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

  void SetCandidate(const GaiaId& gaia_id) {
    preview_service_->SetPreferredAccountForSwitching(MakePreference(gaia_id));
  }

  void ClearCandidate() {
    preview_service_->SetPreferredAccountForSwitching(std::nullopt);
  }

  void SwitchPrimaryAccountInProfile(const AccountInfo& new_primary,
                                     bool stamp_past_grace_period = true) {
    PrimaryAccountMutator* mutator =
        identity_test_env()->identity_manager()->GetPrimaryAccountMutator();
    ASSERT_NE(mutator, nullptr);
    ASSERT_TRUE(mutator->RemovePrimaryAccountButKeepTokens(
        signin_metrics::ProfileSignout::kSignoutForAccountSwitching));
    ASSERT_EQ(mutator->SetPrimaryAccount(
                  new_primary.GetAccountId(), ConsentLevel::kSignin,
                  signin_metrics::AccessPoint::kStartPage),
              PrimaryAccountMutator::PrimaryAccountError::kNoError);
    ProfileAttributesEntry* profile_entry = GetProfileEntry(profile_);
    ASSERT_NE(profile_entry, nullptr);
    profile_entry->SetAuthInfo(new_primary.GetGaiaId(),
                               base::UTF8ToUTF16(new_primary.GetEmail()),
                               /*is_consented_primary_account=*/false);
    if (stamp_past_grace_period) {
      SigninPrefs(*profile_->GetPrefs())
          .SetChromeLastSigninTime(
              new_primary.GetGaiaId(),
              base::Time::Now() -
                  AccountSwitchingManager::kPrimaryAccountSigninGracePeriod);
    }
  }

  void SetUpMockPromoBubble() {
    ON_CALL(mock_show_bubble_callback_, Run)
        .WillByDefault(
            [this](BrowserWindowInterface*, const AccountInfo& primary_account,
                   const AccountInfo& target_account,
                   base::OnceCallback<void(AccountSwitchingPromoResult)>
                       callback) {
              ++promo_show_count_;
              last_promo_primary_account_ = primary_account;
              last_promo_target_account_ = target_account;
              last_promo_result_callback_ = std::move(callback);
              return base::ScopedClosureRunner(base::BindLambdaForTesting(
                  [this]() { ++promo_close_handle_count_; }));
            });
    show_bubble_callback_ = mock_show_bubble_callback_.Get();
    show_bubble_callback_override_.emplace(
        AccountSwitchingManager::SetShowBubbleCallbackForTesting(
            &show_bubble_callback_));
  }

  void ResolvePendingPromo(AccountSwitchingPromoResult result) {
    ASSERT_TRUE(last_promo_result_callback_);
    std::move(last_promo_result_callback_).Run(result);
  }

  void ShowAndResolvePromo(AccountSwitchingPromoResult result) {
    ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
    ASSERT_TRUE(manager_->IsPromoActiveForTesting());
    ResolvePendingPromo(result);
    ASSERT_FALSE(manager_->IsPromoActiveForTesting());
  }

  void ShowAcceptAndCompleteSwitch(bool success) {
    ShowAndResolvePromo(AccountSwitchingPromoResult::kAccepted);
    ASSERT_TRUE(manager_->IsSwitchInProgressForTesting());
    manager_->CompleteSwitchForTesting(success);
    ASSERT_FALSE(manager_->IsSwitchInProgressForTesting());
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
  testing::NiceMock<MockBrowserWindowInterface> browser_window_;

  AccountInfo primary_account_;
  AccountInfo secondary_account_;
  AccountInfo third_account_;
  AccountInfo fourth_account_;

  testing::NiceMock<
      base::MockCallback<AccountSwitchingManager::ShowBubbleCallback>>
      mock_show_bubble_callback_;
  AccountSwitchingManager::ShowBubbleCallback show_bubble_callback_;
  std::optional<base::AutoReset<AccountSwitchingManager::ShowBubbleCallback*>>
      show_bubble_callback_override_;
  int promo_show_count_ = 0;
  int promo_close_handle_count_ = 0;
  AccountInfo last_promo_primary_account_;
  AccountInfo last_promo_target_account_;
  base::OnceCallback<void(AccountSwitchingPromoResult)>
      last_promo_result_callback_;
};

TEST_F(AccountSwitchingManagerTest, InitiallyEligibleAndShowPromo) {
  EXPECT_TRUE(manager_->CanShowPromo());

  // Null browser window does not activate the promo.
  EXPECT_FALSE(manager_->ShowPromo(nullptr));
  EXPECT_FALSE(manager_->IsPromoActiveForTesting());
  EXPECT_TRUE(manager_->CanShowPromo());

  EXPECT_TRUE(manager_->ShowPromo(&browser_window_));
  EXPECT_TRUE(manager_->IsPromoActiveForTesting());
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_EQ(promo_show_count_, 1);
  EXPECT_EQ(last_promo_primary_account_.GetEmail(),
            primary_account_.GetEmail());
  EXPECT_EQ(last_promo_target_account_.GetEmail(),
            secondary_account_.GetEmail());
}

TEST_F(AccountSwitchingManagerTest, PromoNotDisplayedDoesNotThrottle) {
  ShowAndResolvePromo(AccountSwitchingPromoResult::kNotDisplayed);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());
  EXPECT_TRUE(manager_->CanShowPromo());

  // Synchronous `kNotDisplayed` / `kNotDisplayedNoAvatarButton` results return
  // false from `ShowPromo()` and leave the promo eligible.
  for (AccountSwitchingPromoResult result :
       {AccountSwitchingPromoResult::kNotDisplayed,
        AccountSwitchingPromoResult::kNotDisplayedNoAvatarButton}) {
    EXPECT_CALL(mock_show_bubble_callback_, Run)
        .WillOnce([result](BrowserWindowInterface*, const AccountInfo&,
                           const AccountInfo&,
                           base::OnceCallback<void(AccountSwitchingPromoResult)>
                               callback) {
          std::move(callback).Run(result);
          return base::ScopedClosureRunner();
        });
    EXPECT_FALSE(manager_->ShowPromo(&browser_window_));
    EXPECT_FALSE(manager_->IsPromoActiveForTesting());
    EXPECT_TRUE(manager_->CanShowPromo());
  }
}

TEST_F(AccountSwitchingManagerTest, PrimaryAccountSigninGracePeriod) {
  // Missing sign-in timestamp blocks eligibility (fail closed).
  SwitchPrimaryAccountInProfile(fourth_account_,
                                /*stamp_past_grace_period=*/false);
  SigninPrefs(*profile_->GetPrefs()).ClearAllChromeLastSigninTime();
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));

  // Stamping sign-in now starts a 28-day grace period.
  SigninPrefs(*profile_->GetPrefs())
      .SetChromeLastSigninTime(fourth_account_.GetGaiaId(), base::Time::Now());
  EXPECT_FALSE(manager_->CanShowPromo());

  // Just before 28 days: still blocked.
  task_environment_.AdvanceClock(
      AccountSwitchingManager::kPrimaryAccountSigninGracePeriod -
      base::Seconds(1));
  EXPECT_FALSE(manager_->CanShowPromo());

  // Exactly 28 days: eligible.
  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_TRUE(manager_->CanShowPromo());

  // Null timestamp (`"0"`) fails closed rather than treating epoch delta as
  // elapsed.
  SigninPrefs(*profile_->GetPrefs())
      .SetChromeLastSigninTime(fourth_account_.GetGaiaId(), base::Time());
  EXPECT_FALSE(manager_->CanShowPromo());

  // Future timestamp (clock skew) also fails closed within grace period.
  SigninPrefs(*profile_->GetPrefs())
      .SetChromeLastSigninTime(fourth_account_.GetGaiaId(),
                               base::Time::Now() + base::Days(1));
  EXPECT_FALSE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest, GlobalIgnoreCooldownAndCap) {
  // Dismissals and ignores 1 through 4 enforce a 7-day global cooldown and
  // then recover.
  for (int i = 0; i < AccountSwitchingManager::kMaxGlobalIgnores - 1; ++i) {
    ASSERT_TRUE(manager_->CanShowPromo());
    ShowAndResolvePromo(i % 2 == 0 ? AccountSwitchingPromoResult::kDismissed
                                   : AccountSwitchingPromoResult::kIgnored);

    // Immediately blocked globally across all candidates.
    EXPECT_FALSE(manager_->CanShowPromo());
    SetCandidate(third_account_.GetGaiaId());
    EXPECT_FALSE(manager_->CanShowPromo());

    // Still blocked 1 second before the 7-day cooldown expires.
    task_environment_.AdvanceClock(
        AccountSwitchingManager::kGlobalIgnoreCooldown - base::Seconds(1));
    EXPECT_FALSE(manager_->CanShowPromo());

    // Eligible again once the 7-day cooldown expires.
    task_environment_.AdvanceClock(base::Seconds(1));
    EXPECT_TRUE(manager_->CanShowPromo());
    SetCandidate(secondary_account_.GetGaiaId());
    EXPECT_TRUE(manager_->CanShowPromo());
  }

  // 5th ignore hits the permanent global cap.
  ShowAndResolvePromo(AccountSwitchingPromoResult::kDismissed);
  EXPECT_FALSE(manager_->CanShowPromo());
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());

  // Remains permanently blocked even well past the 7-day cooldown.
  task_environment_.AdvanceClock(base::Days(365));
  EXPECT_FALSE(manager_->CanShowPromo());
  SetCandidate(secondary_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest, GlobalExplicitDismissalCooldown) {
  ShowAndResolvePromo(AccountSwitchingPromoResult::kDeclined);

  // Blocked globally for 28 days across all candidates.
  EXPECT_FALSE(manager_->CanShowPromo());
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());

  // Just before 28 days: still blocked.
  task_environment_.AdvanceClock(
      AccountSwitchingManager::kGlobalExplicitDismissalCooldown -
      base::Seconds(1));
  EXPECT_FALSE(manager_->CanShowPromo());
  SetCandidate(secondary_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());

  // At 28 days: eligible again (pair dismissal count is only 1 < 2).
  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_TRUE(manager_->CanShowPromo());
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_TRUE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest, PairExplicitDismissalCapOfTwo) {
  // First dismissal of `(A, B)`.
  ShowAndResolvePromo(AccountSwitchingPromoResult::kDeclined);
  task_environment_.AdvanceClock(
      AccountSwitchingManager::kGlobalExplicitDismissalCooldown);
  EXPECT_TRUE(manager_->CanShowPromo());

  // Second dismissal in the opposite direction `(B, A)` hits the shared
  // per-pair cap of 2 for `{A, B}`.
  SwitchPrimaryAccountInProfile(secondary_account_);
  SetCandidate(primary_account_.GetGaiaId());
  ASSERT_TRUE(manager_->CanShowPromo());
  ShowAndResolvePromo(AccountSwitchingPromoResult::kDeclined);
  task_environment_.AdvanceClock(
      AccountSwitchingManager::kGlobalExplicitDismissalCooldown);

  // `{A, B}` stays permanently blocked in both directions even after 365 days.
  EXPECT_FALSE(manager_->CanShowPromo());
  SwitchPrimaryAccountInProfile(primary_account_);
  SetCandidate(secondary_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());

  task_environment_.AdvanceClock(base::Days(365));
  EXPECT_FALSE(manager_->CanShowPromo());
  SwitchPrimaryAccountInProfile(secondary_account_);
  SetCandidate(primary_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());

  // Unrelated pairs remain eligible.
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_TRUE(manager_->CanShowPromo());
  SwitchPrimaryAccountInProfile(primary_account_);
  EXPECT_TRUE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest, SymmetricPairSuppression) {
  // Suppressing `{A, B}` blocks switching prompts in both directions while
  // both accounts remain in the profile.
  SigninPrefs(*profile_->GetPrefs())
      .SetAccountSwitchingPromoSuppressedBySwitch(
          secondary_account_.GetGaiaId(), primary_account_.GetGaiaId());

  EXPECT_FALSE(manager_->CanShowPromo());
  SwitchPrimaryAccountInProfile(secondary_account_);
  SetCandidate(primary_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());

  // Other pairs remain eligible.
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_TRUE(manager_->CanShowPromo());

  // Suppression persists indefinitely while both accounts remain.
  task_environment_.AdvanceClock(base::Days(365));
  SetCandidate(primary_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());
  SwitchPrimaryAccountInProfile(primary_account_);
  SetCandidate(secondary_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest,
       ActivePromoAndSwitchBlockOverlapAndSuppressOnCompletion) {
  ASSERT_TRUE(manager_->CanShowPromo());
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  EXPECT_TRUE(manager_->IsPromoActiveForTesting());
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));

  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsPromoActiveForTesting());
  EXPECT_TRUE(manager_->IsSwitchInProgressForTesting());

  // While switching is in progress, promos are blocked across all candidates.
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));

  manager_->CompleteSwitchForTesting(/*success=*/true);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());

  // Once finished, `{A, B}` is suppressed in both directions and other pairs
  // wait out the 7-day post-switch cooldown.
  SetCandidate(secondary_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());
  SwitchPrimaryAccountInProfile(secondary_account_);
  SetCandidate(primary_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());

  SwitchPrimaryAccountInProfile(primary_account_);
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());
  task_environment_.AdvanceClock(AccountSwitchingManager::kPostSwitchCooldown);
  EXPECT_TRUE(manager_->CanShowPromo());
  ShowAcceptAndCompleteSwitch(/*success=*/true);
  EXPECT_FALSE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest,
       SuccessfulSwitchSuppressesPairBidirectionally) {
  const SigninPrefs signin_prefs(*profile_->GetPrefs());
  ASSERT_FALSE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      primary_account_.GetGaiaId(), secondary_account_.GetGaiaId()));

  ShowAndResolvePromo(AccountSwitchingPromoResult::kAccepted);
  ASSERT_TRUE(manager_->IsSwitchInProgressForTesting());

  // Suppression is not recorded until the switch actually finishes
  // successfully.
  EXPECT_FALSE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      primary_account_.GetGaiaId(), secondary_account_.GetGaiaId()));

  manager_->CompleteSwitchForTesting(/*success=*/true);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());

  EXPECT_TRUE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      primary_account_.GetGaiaId(), secondary_account_.GetGaiaId()));
  EXPECT_TRUE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      secondary_account_.GetGaiaId(), primary_account_.GetGaiaId()));
  EXPECT_FALSE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      primary_account_.GetGaiaId(), third_account_.GetGaiaId()));

  // Suppression persists indefinitely across the 28-day window and beyond.
  task_environment_.AdvanceClock(base::Days(365));
  EXPECT_FALSE(manager_->CanShowPromo());
  SwitchPrimaryAccountInProfile(secondary_account_);
  SetCandidate(primary_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest, AbortedSwitchDoesNotSuppressPair) {
  const SigninPrefs signin_prefs(*profile_->GetPrefs());

  ShowAcceptAndCompleteSwitch(/*success=*/false);

  EXPECT_FALSE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      primary_account_.GetGaiaId(), secondary_account_.GetGaiaId()));
  EXPECT_TRUE(manager_->CanShowPromo());
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_TRUE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest,
       PrefObserverStartingNextSwitchOnCompletionIsSafe) {
  base::test::ScopedFeatureList skip_cooldown_features;
  skip_cooldown_features.InitAndEnableFeatureWithParameters(
      switches::kEnableAccountPreviewSwitchingAccount,
      {{switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.name,
        "true"}});

  bool started_second_switch = false;
  PrefChangeRegistrar registrar;
  registrar.Init(profile_->GetPrefs());
  SigninPrefs::ObserveSigninPrefsChanges(
      registrar, base::BindLambdaForTesting([&]() {
        if (!started_second_switch) {
          started_second_switch = true;
          SetCandidate(third_account_.GetGaiaId());
          ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
          ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
        }
      }));

  ShowAndResolvePromo(AccountSwitchingPromoResult::kAccepted);
  ASSERT_TRUE(manager_->IsSwitchInProgressForTesting());

  // Completing the first switch notifies the pref observer synchronously when
  // recording pair suppression. Starting a follow-up promo/switch in that
  // observer must not be cleared or corrupted by the first completion.
  manager_->CompleteSwitchForTesting(/*success=*/true);
  EXPECT_TRUE(started_second_switch);
  EXPECT_TRUE(manager_->IsSwitchInProgressForTesting());

  manager_->CompleteSwitchForTesting(/*success=*/true);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());
  EXPECT_TRUE(
      SigninPrefs(*profile_->GetPrefs())
          .IsAccountSwitchingPromoSuppressedBySwitch(
              primary_account_.GetGaiaId(), third_account_.GetGaiaId()));
}

TEST_F(AccountSwitchingManagerTest,
       ShutdownAndDestructionSafeAcrossActivePromoOrSwitch) {
  // Verify `Shutdown()` closes an active promo bubble and drops any late
  // callback without recording ignores, dismissals, or suppression.
  {
    auto standalone_manager = std::make_unique<AccountSwitchingManager>(
        profile_->GetPrefs(),
        ChromeSigninClientFactory::GetForProfile(profile_),
        IdentityManagerFactory::GetForProfile(profile_),
        AccountPreviewDataServiceFactory::GetForProfile(profile_));
    const int initial_close_count = promo_close_handle_count_;
    ASSERT_TRUE(standalone_manager->ShowPromo(&browser_window_));
    ASSERT_TRUE(standalone_manager->IsPromoActiveForTesting());
    auto late_callback = std::move(last_promo_result_callback_);
    ASSERT_TRUE(late_callback);
    standalone_manager->Shutdown();
    EXPECT_EQ(promo_close_handle_count_, initial_close_count + 1);
    EXPECT_FALSE(standalone_manager->IsPromoActiveForTesting());
    std::move(late_callback).Run(AccountSwitchingPromoResult::kDeclined);
    EXPECT_TRUE(manager_->CanShowPromo());
  }

  // Verify `Shutdown()` cancels an in-progress switch without suppressing the
  // pair.
  {
    auto standalone_manager = std::make_unique<AccountSwitchingManager>(
        profile_->GetPrefs(),
        ChromeSigninClientFactory::GetForProfile(profile_),
        IdentityManagerFactory::GetForProfile(profile_),
        AccountPreviewDataServiceFactory::GetForProfile(profile_));
    ASSERT_TRUE(standalone_manager->ShowPromo(&browser_window_));
    ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
    ASSERT_TRUE(standalone_manager->IsSwitchInProgressForTesting());
    standalone_manager->Shutdown();
    EXPECT_FALSE(standalone_manager->IsSwitchInProgressForTesting());
    EXPECT_FALSE(
        SigninPrefs(*profile_->GetPrefs())
            .IsAccountSwitchingPromoSuppressedBySwitch(
                primary_account_.GetGaiaId(), secondary_account_.GetGaiaId()));
  }

  // Verify destroying the manager directly while a switch is active also leaves
  // the pair unsuppressed.
  {
    auto standalone_manager = std::make_unique<AccountSwitchingManager>(
        profile_->GetPrefs(),
        ChromeSigninClientFactory::GetForProfile(profile_),
        IdentityManagerFactory::GetForProfile(profile_),
        AccountPreviewDataServiceFactory::GetForProfile(profile_));
    ASSERT_TRUE(standalone_manager->ShowPromo(&browser_window_));
    ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
    ASSERT_TRUE(standalone_manager->IsSwitchInProgressForTesting());
    standalone_manager.reset();
    EXPECT_FALSE(
        SigninPrefs(*profile_->GetPrefs())
            .IsAccountSwitchingPromoSuppressedBySwitch(
                primary_account_.GetGaiaId(), secondary_account_.GetGaiaId()));
  }

  ShowAndResolvePromo(AccountSwitchingPromoResult::kAccepted);
  ASSERT_TRUE(manager_->IsSwitchInProgressForTesting());

  manager_ = nullptr;
  preview_service_ = nullptr;
  identity_test_env_adaptor_.reset();
  profile_ = nullptr;
  profile_manager_.DeleteTestingProfile("Profile 1");
}

TEST_F(AccountSwitchingManagerTest,
       AcceptingOpenPromoRevalidatesEligibilityForShownPair) {
  // Open promo for `(primary_account_, secondary_account_)`.
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));

  // Promo throttling/cooldown updates while the promo is open do not block
  // `kAccepted` for the valid pair.
  {
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                prefs::kAccountSwitchingPromoState);
    update->Set("last_ignored_time", base::TimeToValue(base::Time::Now()));
  }
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_TRUE(manager_->IsSwitchInProgressForTesting());
  manager_->CompleteSwitchForTesting(/*success=*/true);

  const SigninPrefs signin_prefs(*profile_->GetPrefs());
  EXPECT_TRUE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      primary_account_.GetGaiaId(), secondary_account_.GetGaiaId()));

  // Clear ignore cooldown and verify changing `AccountPreviewDataService` to a
  // different candidate while `(primary_account_, third_account_)` is open
  // rejects `kAccepted` without starting a switch.
  task_environment_.AdvanceClock(
      AccountSwitchingManager::kGlobalIgnoreCooldown);
  SetCandidate(third_account_.GetGaiaId());
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  SetCandidate(fourth_account_.GetGaiaId());
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());
  EXPECT_FALSE(signin_prefs.IsAccountSwitchingPromoSuppressedBySwitch(
      primary_account_.GetGaiaId(), third_account_.GetGaiaId()));
  SetCandidate(third_account_.GetGaiaId());

  // Target account signed in on another profile while promo is open.
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  ProfileAttributesEntry* other_entry = GetProfileEntry(other_profile_);
  ASSERT_NE(other_entry, nullptr);
  other_entry->SetAuthInfo(third_account_.GetGaiaId(), u"third@gmail.com",
                           /*is_consented_primary_account=*/false);
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());
  other_entry->SetAuthInfo(GaiaId(), std::u16string(),
                           /*is_consented_primary_account=*/false);

  // Target persistent refresh token error while promo is open.
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      third_account_.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      third_account_.GetAccountId(), GoogleServiceAuthError::AuthErrorNone());

  // Target account becoming child while promo is open.
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(third_account_)
          .SetIsChildAccount(Tribool::kTrue)
          .Build());
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(third_account_)
          .SetIsChildAccount(Tribool::kFalse)
          .Build());

  // Target refresh token removed while promo is open.
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  identity_test_env()->RemoveRefreshTokenForAccount(
      third_account_.GetAccountId());
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());
  third_account_ = identity_test_env()->MakeAccountAvailable("third@gmail.com");

  // Target account becoming managed while promo is open.
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(third_account_)
          .SetHostedDomain("example.com")
          .Build());
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());

  // Switch candidate to `fourth_account_` for primary-account and pref guards.
  SetCandidate(fourth_account_.GetGaiaId());

  // Primary-account persistent error while promo is open.
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_.GetAccountId(), GoogleServiceAuthError::AuthErrorNone());

  // Primary account changed while promo is open.
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  SwitchPrimaryAccountInProfile(fourth_account_);
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());
  SwitchPrimaryAccountInProfile(primary_account_);

  // Primary account cleared (signed out) while promo is open.
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  identity_test_env()->ClearPrimaryAccount();
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());

  // Restore signed-in state and verify `prefs::kSigninAllowed = false` blocks
  // `kAccepted` while open.
  primary_account_ = identity_test_env()->MakePrimaryAccountAvailable(
      "primary@gmail.com", ConsentLevel::kSignin);
  fourth_account_ =
      identity_test_env()->MakeAccountAvailable("fourth@gmail.com");
  task_environment_.AdvanceClock(
      AccountSwitchingManager::kPrimaryAccountSigninGracePeriod);
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  profile_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());
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
    update->Set("last_switch_time",
                base::TimeToValue(base::Time::Now() + base::Days(30)));
  }
  EXPECT_TRUE(manager_->CanShowPromo());

  {
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                prefs::kAccountSwitchingPromoState);
    update->Set("last_ignored_time", base::TimeToValue(base::Time()));
    update->Set("last_explicit_dismissal_time",
                base::TimeToValue(base::Time()));
    update->Set("last_switch_time", base::TimeToValue(base::Time()));
  }
  EXPECT_TRUE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest, DisabledFeatureFlagsBlockPromo) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        switches::kEnableAccountPreviewSwitchingAccount);
    EXPECT_FALSE(manager_->CanShowPromo());
    EXPECT_FALSE(manager_->ShowPromo(&browser_window_));
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
    EXPECT_FALSE(manager_->CanShowPromo());
    EXPECT_FALSE(manager_->ShowPromo(&browser_window_));

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
  EXPECT_TRUE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest,
       SkipCooldownBypassesThrottlingButNotStructuralGuards) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableAccountPreviewSwitchingAccount,
      {{switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.name,
        "true"}});

  // Trigger every throttling / cooldown / pair-suppression rule and switch to
  // an account without `ChromeLastSigninTime`.
  for (int i = 0; i < AccountSwitchingManager::kMaxGlobalIgnores; ++i) {
    ShowAndResolvePromo(AccountSwitchingPromoResult::kDismissed);
  }
  ShowAndResolvePromo(AccountSwitchingPromoResult::kDeclined);
  ShowAndResolvePromo(AccountSwitchingPromoResult::kDeclined);
  SigninPrefs(*profile_->GetPrefs())
      .SetAccountSwitchingPromoSuppressedBySwitch(
          secondary_account_.GetGaiaId(), fourth_account_.GetGaiaId());
  SwitchPrimaryAccountInProfile(fourth_account_,
                                /*stamp_past_grace_period=*/false);

  EXPECT_TRUE(manager_->CanShowPromo());

  // Ongoing switch guard is still enforced.
  ShowAndResolvePromo(AccountSwitchingPromoResult::kAccepted);
  ASSERT_TRUE(manager_->IsSwitchInProgressForTesting());
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));
  manager_->CompleteSwitchForTesting(/*success=*/true);
  EXPECT_TRUE(manager_->CanShowPromo());
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_TRUE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest, RegularAndOffTheRecordProfiles) {
  EXPECT_EQ(AccountSwitchingManagerFactory::GetForProfile(nullptr), nullptr);

  // Regular profile gets a stable non-null instance and registered pref.
  EXPECT_EQ(AccountSwitchingManagerFactory::GetForProfile(profile_), manager_);
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

TEST_F(AccountSwitchingManagerTest,
       TargetAccountSignedInOnOtherProfileBlocksPromoAndSwitching) {
  ASSERT_TRUE(manager_->CanShowPromo());

  // Sign `secondary_account_` into `other_profile_` as primary.
  ProfileAttributesEntry* other_entry = GetProfileEntry(other_profile_);
  ASSERT_NE(other_entry, nullptr);
  other_entry->SetAuthInfo(secondary_account_.GetGaiaId(),
                           u"secondary@gmail.com",
                           /*is_consented_primary_account=*/false);

  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));

  // Unrelated accounts not signed in on another profile remain eligible.
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_TRUE(manager_->CanShowPromo());
  SetCandidate(secondary_account_.GetGaiaId());

  // Even `SkipCooldown` does not bypass the multi-profile structural guard.
  {
    base::test::ScopedFeatureList skip_cooldown_features;
    skip_cooldown_features.InitAndEnableFeatureWithParameters(
        switches::kEnableAccountPreviewSwitchingAccount,
        {{switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.name,
          "true"}});
    EXPECT_FALSE(manager_->CanShowPromo());
    EXPECT_FALSE(manager_->ShowPromo(&browser_window_));
  }

  // Omitted ephemeral profiles are ignored by
  // `IsAccountSignedInOnOtherProfile`.
  other_entry->SetIsEphemeral(true);
  other_entry->SetIsOmitted(true);
  EXPECT_TRUE(manager_->CanShowPromo());
  other_entry->SetIsOmitted(false);
  other_entry->SetIsEphemeral(false);
  EXPECT_FALSE(manager_->CanShowPromo());

  // Signing `secondary_account_` out of `other_profile_` restores eligibility.
  other_entry->SetAuthInfo(GaiaId(), std::u16string(),
                           /*is_consented_primary_account=*/false);
  ASSERT_TRUE(manager_->CanShowPromo());

  // Signing `secondary_account_` into `other_profile_` while a promo is open
  // also blocks switching on acceptance.
  ASSERT_TRUE(manager_->ShowPromo(&browser_window_));
  other_entry->SetAuthInfo(secondary_account_.GetGaiaId(),
                           u"secondary@gmail.com",
                           /*is_consented_primary_account=*/false);
  ResolvePendingPromo(AccountSwitchingPromoResult::kAccepted);
  EXPECT_FALSE(manager_->IsSwitchInProgressForTesting());
}

TEST_F(AccountSwitchingManagerTest, FiltersMissingEmptyAndSelfCandidates) {
  ClearCandidate();
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));

  // Empty Gaia ID candidate is rejected.
  SetCandidate(GaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());

  // Candidate matching the primary account is rejected.
  SetCandidate(primary_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());

  // Incomplete candidate without an email is rejected rather than crashing
  // `ShowPromo()`.
  const GaiaId incomplete_gaia_id("incomplete-gaia-id");
  const CoreAccountId incomplete_account_id =
      identity_test_env()
          ->identity_manager()
          ->GetAccountsMutator()
          ->AddOrUpdateAccount(
              incomplete_gaia_id, /*email=*/"", "refresh_token",
              /*is_under_advanced_protection=*/false, std::nullopt,
              signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  ASSERT_TRUE(
      identity_test_env()->identity_manager()->HasAccountWithRefreshToken(
          incomplete_account_id));
  const AccountInfo incomplete_account =
      identity_test_env()->identity_manager()->FindExtendedAccountInfoByGaiaId(
          incomplete_gaia_id);
  ASSERT_FALSE(incomplete_account.IsEmpty());
  ASSERT_TRUE(incomplete_account.GetEmail().empty());
  SetCandidate(incomplete_gaia_id);
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));

  // Valid secondary candidate restores eligibility.
  SetCandidate(secondary_account_.GetGaiaId());
  EXPECT_TRUE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest, FiltersManagedAndChildTargetAccounts) {
  ASSERT_TRUE(manager_->CanShowPromo());

  // Child target account is filtered out.
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(secondary_account_)
          .SetIsChildAccount(Tribool::kTrue)
          .Build());
  EXPECT_FALSE(manager_->CanShowPromo());
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(secondary_account_)
          .SetIsChildAccount(Tribool::kFalse)
          .Build());
  ASSERT_TRUE(manager_->CanShowPromo());

  // Managed target account is filtered out, while an unmanaged candidate
  // remains eligible.
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(secondary_account_)
          .SetHostedDomain("example.com")
          .Build());
  EXPECT_FALSE(manager_->CanShowPromo());
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_TRUE(manager_->CanShowPromo());

  // Unknown management status on a potentially enterprise domain fails closed.
  AccountInfo custom_account =
      identity_test_env()->MakeAccountAvailable("custom@company.com");
  ASSERT_EQ(custom_account.IsManaged(), Tribool::kUnknown);
  SetCandidate(custom_account.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());

  // Flex Org account (`IsManaged() == Tribool::kFalse` but
  // `IsMemberOfFlexOrg() == true`) also filters out the candidate.
  custom_account = AccountInfo::Builder(custom_account)
                       .SetHostedDomain(signin::constants::kNoHostedDomainFound)
                       .Build();
  AccountCapabilitiesTestMutator(&custom_account)
      .set_is_subject_to_enterprise_features(true);
  identity_test_env()->UpdateAccountInfoForAccount(custom_account);
  EXPECT_FALSE(manager_->CanShowPromo());

  // Confirmed consumer non-well-known account is eligible.
  AccountCapabilitiesTestMutator(&custom_account)
      .set_is_subject_to_enterprise_features(false);
  identity_test_env()->UpdateAccountInfoForAccount(custom_account);
  EXPECT_TRUE(manager_->CanShowPromo());

  // Managed, Flex Org, child, and unknown-enterprise-domain primary accounts
  // are also filtered out.
  SetCandidate(third_account_.GetGaiaId());
  ASSERT_TRUE(manager_->CanShowPromo());

  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(primary_account_)
          .SetIsChildAccount(Tribool::kTrue)
          .Build());
  EXPECT_FALSE(manager_->CanShowPromo());
  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(primary_account_)
          .SetIsChildAccount(Tribool::kFalse)
          .Build());
  ASSERT_TRUE(manager_->CanShowPromo());

  primary_account_ =
      AccountInfo::Builder(primary_account_)
          .SetHostedDomain(signin::constants::kNoHostedDomainFound)
          .Build();
  AccountCapabilitiesTestMutator(&primary_account_)
      .set_is_subject_to_enterprise_features(true);
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_);
  EXPECT_FALSE(manager_->CanShowPromo());

  AccountCapabilitiesTestMutator(&primary_account_)
      .set_is_subject_to_enterprise_features(false);
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_);
  ASSERT_TRUE(manager_->CanShowPromo());

  identity_test_env()->UpdateAccountInfoForAccount(
      AccountInfo::Builder(primary_account_)
          .SetHostedDomain("example.com")
          .Build());
  EXPECT_FALSE(manager_->CanShowPromo());

  AccountInfo custom_primary =
      identity_test_env()->MakeAccountAvailable("primary_custom@company.com");
  ASSERT_EQ(custom_primary.IsManaged(), Tribool::kUnknown);
  SwitchPrimaryAccountInProfile(custom_primary);
  EXPECT_FALSE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest,
       FiltersRemovedUnloadedAndErrorRefreshTokens) {
  ASSERT_TRUE(manager_->CanShowPromo());

  // Persistent refresh token error on the candidate filters it out.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account_.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  EXPECT_FALSE(manager_->CanShowPromo());

  // Clearing the error restores eligibility.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account_.GetAccountId(),
      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_TRUE(manager_->CanShowPromo());

  // Persistent refresh token error on the primary account also filters it out.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));

  // Clearing the primary account error restores eligibility.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_.GetAccountId(), GoogleServiceAuthError::AuthErrorNone());
  ASSERT_TRUE(manager_->CanShowPromo());

  // Removing the candidate's refresh token filters out the stale candidate.
  identity_test_env()->RemoveRefreshTokenForAccount(
      secondary_account_.GetAccountId());
  EXPECT_FALSE(manager_->CanShowPromo());

  // Re-adding the candidate restores eligibility.
  secondary_account_ =
      identity_test_env()->MakeAccountAvailable("secondary@gmail.com");
  ASSERT_TRUE(manager_->CanShowPromo());

  // Removing the primary account's refresh token filters out the candidate,
  // and restoring it recovers eligibility.
  identity_test_env()->RemoveRefreshTokenForAccount(
      primary_account_.GetAccountId());
  EXPECT_FALSE(manager_->CanShowPromo());
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  ASSERT_TRUE(manager_->CanShowPromo());

  // Clearing the primary account filters out the candidate.
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(manager_->CanShowPromo());

  // Re-signing in starts a fresh 28-day grace period; once elapsed, resetting
  // refresh tokens to unloaded filters out the candidate.
  primary_account_ = identity_test_env()->MakePrimaryAccountAvailable(
      "primary@gmail.com", ConsentLevel::kSignin);
  secondary_account_ =
      identity_test_env()->MakeAccountAvailable("secondary@gmail.com");
  task_environment_.AdvanceClock(
      AccountSwitchingManager::kPrimaryAccountSigninGracePeriod);
  ASSERT_TRUE(manager_->CanShowPromo());

  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  EXPECT_FALSE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest, SigninDisallowedBlocksPromo) {
  ASSERT_TRUE(manager_->CanShowPromo());

  profile_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));

  // `SkipCooldown` must not bypass `prefs::kSigninAllowed`.
  base::test::ScopedFeatureList skip_cooldown_features;
  skip_cooldown_features.InitAndEnableFeatureWithParameters(
      switches::kEnableAccountPreviewSwitchingAccount,
      {{switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.name,
        "true"}});
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));
}

TEST_F(AccountSwitchingManagerTest,
       PostSwitchCooldownBlocksOtherPairsForSevenDays) {
  // Complete switch `{A, B}` while `A` stays primary in the profile.
  ShowAcceptAndCompleteSwitch(/*success=*/true);

  // Even if `C` immediately becomes the preferred candidate, offering another
  // promo is blocked for `kPostSwitchCooldown` (7 days).
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));

  task_environment_.AdvanceClock(AccountSwitchingManager::kPostSwitchCooldown -
                                 base::Seconds(1));
  EXPECT_FALSE(manager_->CanShowPromo());

  // Cooldown persists across manager re-creation.
  {
    AccountSwitchingManager recreated_manager(
        profile_->GetPrefs(),
        ChromeSigninClientFactory::GetForProfile(profile_),
        IdentityManagerFactory::GetForProfile(profile_),
        AccountPreviewDataServiceFactory::GetForProfile(profile_));
    EXPECT_FALSE(recreated_manager.CanShowPromo());
  }

  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_TRUE(manager_->CanShowPromo());

  // An in-profile switch (changing the primary account) also enforces the
  // 7-day cooldown independently of primary-account grace periods.
  ShowAcceptAndCompleteSwitch(/*success=*/true);
  SwitchPrimaryAccountInProfile(secondary_account_);
  SetCandidate(fourth_account_.GetGaiaId());
  EXPECT_FALSE(manager_->CanShowPromo());
  task_environment_.AdvanceClock(AccountSwitchingManager::kPostSwitchCooldown);
  EXPECT_TRUE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest,
       SuppressedPairDoesNotFallBackUntilAnotherBecomesPreferred) {
  // Suppress `{A, B}` via two explicit dismissals while `C` (`third_account_`)
  // is already an eligible secondary account in the profile.
  ShowAndResolvePromo(AccountSwitchingPromoResult::kDeclined);
  task_environment_.AdvanceClock(
      AccountSwitchingManager::kGlobalExplicitDismissalCooldown);
  ShowAndResolvePromo(AccountSwitchingPromoResult::kDeclined);
  task_environment_.AdvanceClock(
      AccountSwitchingManager::kGlobalExplicitDismissalCooldown);

  // As long as `B` remains the preferred candidate, do not fall back to `C`.
  EXPECT_FALSE(manager_->CanShowPromo());

  // Once `C` becomes the preferred candidate, `{A, C}` is eligible.
  SetCandidate(third_account_.GetGaiaId());
  EXPECT_TRUE(manager_->CanShowPromo());

  // Suppress `{A, C}` via completed switch and verify the same rule after
  // `kPostSwitchCooldown`.
  ShowAcceptAndCompleteSwitch(/*success=*/true);
  task_environment_.AdvanceClock(AccountSwitchingManager::kPostSwitchCooldown);
  EXPECT_FALSE(manager_->CanShowPromo());

  // Make an eligible non-suppressed account (`fourth_account_`) preferred.
  SetCandidate(fourth_account_.GetGaiaId());
  EXPECT_TRUE(manager_->CanShowPromo());
}

TEST_F(AccountSwitchingManagerTest,
       ShowPromoReturnsFalseWithoutSigninViewController) {
  base::AutoReset<AccountSwitchingManager::ShowBubbleCallback*> reset =
      AccountSwitchingManager::SetShowBubbleCallbackForTesting(nullptr);

  // `MockBrowserWindowInterface` does not attach a `SigninViewController`.
  EXPECT_TRUE(manager_->CanShowPromo());
  EXPECT_FALSE(manager_->ShowPromo(&browser_window_));
  EXPECT_FALSE(manager_->IsPromoActiveForTesting());
  EXPECT_TRUE(manager_->CanShowPromo());
}

TEST(AccountSwitchingManagerStandaloneTest, ReturnsFalseWithoutPreviewService) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {switches::kEnableAccountPreviewData,
       switches::kEnableAccountPreviewSwitchingAccount},
      {});
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  testing::NiceMock<MockBrowserWindowInterface> browser_window;

  // Default `TestingProfile` does not instantiate `AccountPreviewDataService`.
  AccountSwitchingManager* manager =
      AccountSwitchingManagerFactory::GetForProfile(&profile);
  ASSERT_NE(manager, nullptr);
  EXPECT_FALSE(manager->CanShowPromo());
  EXPECT_FALSE(manager->ShowPromo(&browser_window));
}

}  // namespace

}  // namespace signin
