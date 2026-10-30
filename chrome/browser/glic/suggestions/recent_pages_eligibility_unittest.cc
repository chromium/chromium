// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/recent_pages_eligibility.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class RecentPagesEligibilityTest : public testing::Test {
 public:
  RecentPagesEligibilityTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicDefaultTabContextSetting);
  }

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactories(
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
                {TestingProfile::TestingFactory{
                    SyncServiceFactory::GetInstance(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<syncer::TestSyncService>();
                    })}}));
    profile_ = builder.Build();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    // Set up a profile that passes every check by default.
    // `TestSyncService` syncs all types (including `kHistory`) by default.
    account_info_ = identity_test_env()->MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    SetCanUseModelExecutionFeatures(true);
    SetFreStatus(prefs::FreStatus::kCompleted);
    SetTabContextEnabled(true);
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
  }

  void SetCanUseModelExecutionFeatures(bool can_use) {
    AccountCapabilitiesTestMutator(&account_info_)
        .set_can_use_model_execution_features(can_use);
    identity_test_env()->UpdateAccountInfoForAccount(account_info_);
  }

  void SetFreStatus(prefs::FreStatus status) {
    profile_->GetPrefs()->SetInteger(prefs::kGlicCompletedFre,
                                     static_cast<int>(status));
  }

  void SetTabContextEnabled(bool enabled) {
    profile_->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                     enabled);
  }

  RecentPagesIneligibilityReason GetEligibility() {
    return GetRecentPagesEligibility(profile_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  AccountInfo account_info_;
};

TEST_F(RecentPagesEligibilityTest, EligibleWhenAllChecksPass) {
  EXPECT_EQ(GetEligibility(), RecentPagesIneligibilityReason::kNone);
}

TEST_F(RecentPagesEligibilityTest, IneligibleWhenHistorySyncOff) {
  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  EXPECT_EQ(GetEligibility(), RecentPagesIneligibilityReason::kHistorySyncOff);
}

TEST_F(RecentPagesEligibilityTest, IneligibleWithoutSyncService) {
  TestingProfile profile_without_sync;
  EXPECT_EQ(GetRecentPagesEligibility(&profile_without_sync),
            RecentPagesIneligibilityReason::kHistorySyncOff);
}

TEST_F(RecentPagesEligibilityTest, IneligibleWhenFreNotStarted) {
  SetFreStatus(prefs::FreStatus::kNotStarted);
  EXPECT_EQ(GetEligibility(), RecentPagesIneligibilityReason::kFreNotAccepted);
}

TEST_F(RecentPagesEligibilityTest, IneligibleWhenFreIncomplete) {
  SetFreStatus(prefs::FreStatus::kIncomplete);
  EXPECT_EQ(GetEligibility(), RecentPagesIneligibilityReason::kFreNotAccepted);
}

TEST_F(RecentPagesEligibilityTest, IneligibleWhenTabContextDisabled) {
  SetTabContextEnabled(false);
  EXPECT_EQ(GetEligibility(),
            RecentPagesIneligibilityReason::kTabContextDisabled);
}

TEST_F(RecentPagesEligibilityTest,
       IneligibleWhenDefaultTabContextFeatureDisabled) {
  base::test::ScopedFeatureList feature_override;
  feature_override.InitAndDisableFeature(
      features::kGlicDefaultTabContextSetting);
  EXPECT_EQ(GetEligibility(),
            RecentPagesIneligibilityReason::kTabContextDisabled);
}

TEST_F(RecentPagesEligibilityTest, IneligibleWhenAgeRestricted) {
  SetCanUseModelExecutionFeatures(false);
  EXPECT_EQ(GetEligibility(), RecentPagesIneligibilityReason::kAgeRestricted);
}

TEST_F(RecentPagesEligibilityTest, IneligibleWhenCapabilitiesUnknown) {
  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  SyncServiceFactory::GetInstance(),
                  base::BindRepeating([](content::BrowserContext* context)
                                          -> std::unique_ptr<KeyedService> {
                    return std::make_unique<syncer::TestSyncService>();
                  })}}));
  std::unique_ptr<TestingProfile> profile = builder.Build();
  IdentityTestEnvironmentProfileAdaptor adaptor(profile.get());
  adaptor.identity_test_env()->MakePrimaryAccountAvailable(
      "unknown@example.com", signin::ConsentLevel::kSignin);
  profile->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));
  profile->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled, true);

  EXPECT_EQ(GetRecentPagesEligibility(profile.get()),
            RecentPagesIneligibilityReason::kAgeRestricted);
}

TEST_F(RecentPagesEligibilityTest, ReturnsFirstFailingCheckInOrder) {
  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  SetFreStatus(prefs::FreStatus::kNotStarted);
  SetTabContextEnabled(false);
  SetCanUseModelExecutionFeatures(false);
  EXPECT_EQ(GetEligibility(), RecentPagesIneligibilityReason::kHistorySyncOff);

  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  EXPECT_EQ(GetEligibility(), RecentPagesIneligibilityReason::kFreNotAccepted);

  SetFreStatus(prefs::FreStatus::kCompleted);
  EXPECT_EQ(GetEligibility(),
            RecentPagesIneligibilityReason::kTabContextDisabled);

  SetTabContextEnabled(true);
  EXPECT_EQ(GetEligibility(), RecentPagesIneligibilityReason::kAgeRestricted);
}

}  // namespace
}  // namespace glic
