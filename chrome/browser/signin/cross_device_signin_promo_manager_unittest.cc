// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/cross_device_signin_promo_manager.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class CrossDeviceSigninPromoManagerTest : public testing::Test {
 public:
  CrossDeviceSigninPromoManagerTest() = default;
  ~CrossDeviceSigninPromoManagerTest() override = default;

  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::FakeDeviceInfoSyncService>();
        }));

    SyncServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
  }

 protected:
  TestingProfile* profile() { return profile_.get(); }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  syncer::FakeDeviceInfoTracker* device_info_tracker() {
    auto* service = static_cast<syncer::FakeDeviceInfoSyncService*>(
        DeviceInfoSyncServiceFactory::GetForProfile(profile_.get()));
    return static_cast<syncer::FakeDeviceInfoTracker*>(
        service->GetDeviceInfoTracker());
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
  }

  void SetHistoryAndTabsSyncingPreference(bool enable_sync) {
    auto* user_settings = sync_service()->GetUserSettings();
    user_settings->SetSelectedType(syncer::UserSelectableType::kHistory,
                                   /*is_type_on=*/enable_sync);
    user_settings->SetSelectedType(syncer::UserSelectableType::kTabs,
                                   /*is_type_on=*/enable_sync);
    user_settings->SetSelectedType(syncer::UserSelectableType::kSavedTabGroups,
                                   /*is_type_on=*/enable_sync);
  }

  void AddDevice(
      const std::string& guid,
      bool is_local,
      syncer::DeviceInfo::OsType os_type = syncer::DeviceInfo::OsType::kLinux) {
    syncer::TestDeviceInfoBuilder builder(os_type);
    builder.WithGuid(guid).WithLastUpdatedTimestamp(base::Time::Now());
    auto device = builder.Build();
    device_info_tracker()->Add(std::move(device));
    if (is_local) {
      device_info_tracker()->SetLocalCacheGuid(guid);
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kCrossDeviceSigninFromDesktop};
};

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_FeatureFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(switches::kCrossDeviceSigninFromDesktop);

  // Setup: Signed in, only local device, history sync enabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(true);

  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_SignedOut) {
  base::HistogramTester histogram_tester;
  // Setup: User is signed out.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::kNotSignedIn, 1);
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_AuthError) {
  base::HistogramTester histogram_tester;
  // Setup: User is signed in but has a persistent error.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::kNotSignedIn, 1);
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_HasMobileDevice) {
  base::HistogramTester histogram_tester;
  // Setup: Signed in, local device added, remote device added.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  AddDevice("remote_device_guid", /*is_local=*/false,
            syncer::DeviceInfo::OsType::kAndroid);
  SetHistoryAndTabsSyncingPreference(true);

  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::kHasMobileDevice, 1);
}

TEST_F(CrossDeviceSigninPromoManagerTest,
       ShouldShowPromo_HasOtherDesktopDevices) {
  base::HistogramTester histogram_tester;
  // Setup: Signed in, local device added, remote desktop device added.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  AddDevice("remote_desktop_guid", /*is_local=*/false,
            syncer::DeviceInfo::OsType::kWindows);
  SetHistoryAndTabsSyncingPreference(true);

  EXPECT_TRUE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::kCanShow, 1);
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_HistorySyncDisabled) {
  base::HistogramTester histogram_tester;
  // Setup: Signed in, only local device, history sync disabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(false);

  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::kDataTypeNotEnabled, 1);
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_HistorySyncEnabled) {
  base::HistogramTester histogram_tester;
  // Setup: Signed in, only local device, history sync enabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(true);

  EXPECT_TRUE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::kCanShow, 1);
}

TEST_F(CrossDeviceSigninPromoManagerTest,
       ShouldShowPromo_ProfileMenuEntryIgnoreHistorySync) {
  base::HistogramTester histogram_tester;
  // Setup: Signed in, only local device, history sync disabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(false);

  // Profile menu promo should still return true even if history sync is
  // disabled.
  EXPECT_TRUE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kProfileMenu, profile()));
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ProfileMenu",
      CrossDeviceSigninPromoShouldShowResult::kCanShow, 1);
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_ShownLimitReached) {
  base::HistogramTester histogram_tester;
  // Setup: Signed in, local device, history sync enabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(true);

  // Show the promo 4 times.
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(ShouldShowCrossDeviceSigninPromo(
        CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
    OnCrossDeviceSigninPromoShown(
        CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile());
    histogram_tester.ExpectBucketCount(
        "Signin.CrossDeviceSigninPromo.ShownCount.HistoryPage", i + 1, 1);
  }

  // 5th time: still allowed.
  EXPECT_TRUE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  OnCrossDeviceSigninPromoShown(CrossDeviceSigninPromoEntryPoint::kHistoryPage,
                                profile());
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.ShownCount.HistoryPage", 5, 1);

  // 6th time: limit reached.
  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::kShownLimitReached, 1);
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_DismissedCooldown) {
  base::HistogramTester histogram_tester;
  // Setup: Signed in, local device, history sync enabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(true);

  // Show once.
  EXPECT_TRUE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  OnCrossDeviceSigninPromoShown(CrossDeviceSigninPromoEntryPoint::kHistoryPage,
                                profile());
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShownCount.HistoryPage", 1, 1);

  // Dismiss it.
  OnCrossDeviceSigninPromoDismissed(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile());
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.DismissedAtShownCount.HistoryPage", 1, 1);

  // Cooldown active: should not show.
  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::kCooldownActive, 1);

  // Fast forward by 6 days: still active.
  FastForwardBy(base::Days(6));
  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::kCooldownActive, 2);

  // Fast forward to 7 days: cooldown expired, allowed to show.
  FastForwardBy(base::Days(1));
  EXPECT_TRUE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::kCanShow, 2);
}

TEST_F(CrossDeviceSigninPromoManagerTest,
       ShouldShowPromo_ShownAfterDismissalLimit) {
  base::HistogramTester histogram_tester;
  // Setup: Signed in, local device, history sync enabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(true);

  // Show and dismiss.
  OnCrossDeviceSigninPromoShown(CrossDeviceSigninPromoEntryPoint::kHistoryPage,
                                profile());
  OnCrossDeviceSigninPromoDismissed(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile());

  // Wait 7 days.
  FastForwardBy(base::Days(7));
  EXPECT_TRUE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::kCanShow, 1);

  // Show it again after dismissal.
  OnCrossDeviceSigninPromoShown(CrossDeviceSigninPromoEntryPoint::kHistoryPage,
                                profile());

  // Now it was shown once after dismissal, so it should be blocked permanently.
  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::
          kAlreadyShownAfterDismissalLimitReached,
      1);

  // Even after another 7 days.
  FastForwardBy(base::Days(7));
  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.HistoryPage",
      CrossDeviceSigninPromoShouldShowResult::
          kAlreadyShownAfterDismissalLimitReached,
      2);
}

TEST_F(CrossDeviceSigninPromoManagerTest,
       ProfileMenuPromoIgnoresDismissalLimits) {
  base::HistogramTester histogram_tester;
  // Setup: Signed in, local device, history sync enabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(true);

  // Show 10 times.
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(ShouldShowCrossDeviceSigninPromo(
        CrossDeviceSigninPromoEntryPoint::kProfileMenu, profile()));
  }
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDeviceSigninPromo.ShouldShowResult.ProfileMenu",
      CrossDeviceSigninPromoShouldShowResult::kCanShow, 10);
}

TEST_F(CrossDeviceSigninPromoManagerTest,
       OpenSigninToPhoneQrCodeBubbleRecordsOpenedMetric) {
  base::HistogramTester histogram_tester;

  OpenSigninToPhoneQrCodeBubble(nullptr,
                                CrossDeviceSigninPromoEntryPoint::kHistoryPage,
                                base::DoNothing());
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.OpenedQrCodeBubble",
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, 1);

  OpenSigninToPhoneQrCodeBubble(nullptr,
                                CrossDeviceSigninPromoEntryPoint::kProfileMenu,
                                base::DoNothing());
  histogram_tester.ExpectBucketCount(
      "Signin.CrossDeviceSigninPromo.OpenedQrCodeBubble",
      CrossDeviceSigninPromoEntryPoint::kProfileMenu, 1);
}
