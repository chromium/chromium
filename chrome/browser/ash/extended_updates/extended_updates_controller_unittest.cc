// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/update_model.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/extended_updates/extended_updates_notification.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;


constexpr char kTimeNow[] = "2024-03-20";
constexpr char kTimePast[] = "2024-02-10";
constexpr char kTimeFarPast[] = "2023-12-25";
constexpr char kTimeFuture[] = "2024-04-30";
constexpr char kTimeFarFuture[] = "2025-05-15";

constexpr char kGaiaId[] = "1234";

constexpr char kFirstAppName[] = "kFirstAppName";
constexpr char kSecondAppName[] = "kSecondAppName";

class ExtendedUpdatesControllerTest : public AshTestBase {
 public:
  ExtendedUpdatesControllerTest()
      : AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())),
        fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ExtendedUpdatesControllerTest(const ExtendedUpdatesControllerTest&) = delete;
  ExtendedUpdatesControllerTest& operator=(
      const ExtendedUpdatesControllerTest&) = delete;
  ~ExtendedUpdatesControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    ExtendedUpdatesController::ResetInstanceForTesting();

    test_clock_.SetNow(GetTime(kTimeNow));
    controller()->SetClockForTesting(&test_clock_);

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);

    notification_display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());

    // Enable arc for test profile.
    // Log in user to ensure ARC PlayStore can be enabled.
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile_->GetProfileUserName(), kGaiaId));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    arc::SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    arc::SetArcPlayStoreEnabledForProfile(profile_, true);
  }

  void TearDown() override {
    controller()->SetClockForTesting(base::DefaultClock::GetInstance());

    AshTestBase::TearDown();
  }

 protected:
  void RunPendingIsOwnerCallbacks(content::BrowserContext* context) {
    OwnerSettingsServiceAshFactory::GetForBrowserContext(context)
        ->RunPendingIsOwnerCallbacksForTesting(
            cros_settings_.device_settings()->current_user_is_owner());
  }

  ExtendedUpdatesController* controller() {
    return ExtendedUpdatesController::Get();
  }

  ExtendedUpdatesController::Params MakeEligibleParams() const {
    return ExtendedUpdatesController::Params{
        .eol_passed = false,
        .extended_date_passed = true,
        .opt_in_required = true,
    };
  }

  UpdateEngineClient::EolInfo MakeEligibleEolInfo() {
    return UpdateEngineClient::EolInfo{
        .eol_date = GetTime(kTimeFarFuture),
        .extended_date = GetTime(kTimePast),
        .extended_opt_in_required = true,
    };
  }

  std::unique_ptr<apps::App> MakeArcApp(const std::string& app_id) {
    auto app = std::make_unique<apps::App>(apps::AppType::kArc, app_id);
    app->readiness = apps::Readiness::kReady;
    return app;
  }

  base::Time GetTime(const char* time_str) {
    base::Time time;
    EXPECT_TRUE(base::Time::FromUTCString(time_str, &time));
    return time;
  }

  // Gets the number of notifications that are currently showing.
  int ShowingNotificationCount() {
    return std::ranges::count_if(
        notification_display_service_tester_->GetDisplayedNotificationsForType(
            ExtendedUpdatesNotification::kNotificationType),
        [](const message_center::Notification& note) {
          return note.id() == ExtendedUpdatesNotification::kNotificationId;
        });
  }

  void CloseNotification(bool by_user) {
    notification_display_service_tester_->RemoveNotification(
        ExtendedUpdatesNotification::kNotificationType,
        std::string(ExtendedUpdatesNotification::kNotificationId), by_user,
        /*silent=*/false);
  }

  bool IsQuickSettingsNoticeVisible() {
    return Shell::Get()
        ->system_tray_model()
        ->update_model()
        ->show_extended_updates_notice();
  }

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  TestingProfileManager profile_manager_;
  base::test::ScopedFeatureList feature_list_{
      features::kExtendedUpdatesOptInFeature};
  ScopedDeviceSettingsTestHelper device_settings_helper_;
  ScopedTestingCrosSettings cros_settings_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
  std::unique_ptr<NotificationDisplayServiceTester>
      notification_display_service_tester_;

  raw_ptr<TestingProfile> profile_;
  base::SimpleTestClock test_clock_;
};

}  // namespace

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_Eligible) {
  EXPECT_TRUE(controller()->IsOptInEligible(profile_, MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kExtendedUpdatesOptInFeature);
  EXPECT_FALSE(controller()->IsOptInEligible(profile_, MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_PastEol) {
  auto params = MakeEligibleParams();
  params.eol_passed = true;
  EXPECT_FALSE(controller()->IsOptInEligible(profile_, params));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_BeforeExtendedDate) {
  auto params = MakeEligibleParams();
  params.extended_date_passed = false;
  EXPECT_FALSE(controller()->IsOptInEligible(profile_, params));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_OptInNotRequired) {
  auto params = MakeEligibleParams();
  params.opt_in_required = false;
  EXPECT_FALSE(controller()->IsOptInEligible(profile_, params));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_AlreadyOptedIn) {
  OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_)->SetBoolean(
      kDeviceExtendedAutoUpdateEnabled, true);
  EXPECT_FALSE(controller()->IsOptInEligible(profile_, MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_NotOwner) {
  cros_settings_.device_settings()->SetCurrentUserIsOwner(false);
  EXPECT_FALSE(controller()->IsOptInEligible(profile_, MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_IsManaged) {
  test_install_attributes_.Get()->SetCloudManaged("fake_domain", "fake_id");
  EXPECT_FALSE(controller()->IsOptInEligible(profile_, MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptedIn_NotOptedInByDefault) {
  EXPECT_FALSE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, IsOptedIn_OptedIn) {
  OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_)->SetBoolean(
      kDeviceExtendedAutoUpdateEnabled, true);
  EXPECT_TRUE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_Success) {
  EXPECT_FALSE(controller()->IsOptedIn());
  EXPECT_TRUE(controller()->OptIn(profile_));
  EXPECT_TRUE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kExtendedUpdatesOptInFeature);
  EXPECT_FALSE(controller()->OptIn(profile_));
  EXPECT_FALSE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_AlreadyOptedIn) {
  EXPECT_TRUE(controller()->OptIn(profile_));
  EXPECT_FALSE(controller()->OptIn(profile_));
  EXPECT_TRUE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_NotOwner) {
  cros_settings_.device_settings()->SetCurrentUserIsOwner(false);
  EXPECT_FALSE(controller()->OptIn(profile_));
  EXPECT_FALSE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_IsManaged) {
  test_install_attributes_.Get()->SetCloudManaged("fake_domain", "fake_id");
  EXPECT_FALSE(controller()->OptIn(profile_));
  EXPECT_FALSE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_EligibleThenOptIn) {
  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_, eol_info);

  // No notification before owner key is loaded.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
  EXPECT_FALSE(IsQuickSettingsNoticeVisible());

  // Simulate owner key loaded.
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));
  EXPECT_TRUE(IsQuickSettingsNoticeVisible());

  EXPECT_TRUE(controller()->OptIn(profile_));
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(IsQuickSettingsNoticeVisible());
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_BeforeExtendedDate) {
  auto eol_info = MakeEligibleEolInfo();
  eol_info.eol_date = GetTime(kTimeFarFuture);
  eol_info.extended_date = GetTime(kTimeFuture);
  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
  EXPECT_FALSE(IsQuickSettingsNoticeVisible());
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_AfterEol) {
  auto eol_info = MakeEligibleEolInfo();
  eol_info.eol_date = GetTime(kTimePast);
  eol_info.extended_date = GetTime(kTimeFarPast);
  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
  EXPECT_FALSE(IsQuickSettingsNoticeVisible());
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_OptInNotRequired) {
  auto eol_info = MakeEligibleEolInfo();
  eol_info.extended_opt_in_required = false;
  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
  EXPECT_FALSE(IsQuickSettingsNoticeVisible());
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kExtendedUpdatesOptInFeature);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
  EXPECT_FALSE(IsQuickSettingsNoticeVisible());
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_NotOwner) {
  cros_settings_.device_settings()->SetCurrentUserIsOwner(false);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
  EXPECT_FALSE(IsQuickSettingsNoticeVisible());
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_ArcAppsInitializedButNoApp) {
  ASSERT_TRUE(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_));
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->OnApps({}, apps::AppType::kArc, /*should_notify_initialized=*/true);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));

  EXPECT_TRUE(IsQuickSettingsNoticeVisible());
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_ArcAppsInitializedWithApps) {
  ASSERT_TRUE(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_));
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  std::vector<apps::AppPtr> deltas;
  deltas.push_back(MakeArcApp(kFirstAppName));
  deltas.push_back(MakeArcApp(kSecondAppName));
  proxy->OnApps(std::move(deltas), apps::AppType::kArc,
                /*should_notify_initialized=*/true);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));

  // Quick settings notice does not depend on having no android apps.
  EXPECT_TRUE(IsQuickSettingsNoticeVisible());
}

TEST_F(ExtendedUpdatesControllerTest,
       OnEolInfo_ArcAppsInitializedWithDisabledApps) {
  ASSERT_TRUE(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_));
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  std::vector<apps::AppPtr> deltas;
  auto app = MakeArcApp(kFirstAppName);
  app->readiness = apps::Readiness::kDisabledByUser;
  deltas.push_back(std::move(app));
  proxy->OnApps(std::move(deltas), apps::AppType::kArc,
                /*should_notify_initialized=*/true);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));

  EXPECT_TRUE(IsQuickSettingsNoticeVisible());
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_ArcDisabledButAppsInstalled) {
  ASSERT_TRUE(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_));
  // Install some arc apps.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  std::vector<apps::AppPtr> deltas;
  deltas.push_back(MakeArcApp(kFirstAppName));
  deltas.push_back(MakeArcApp(kSecondAppName));
  proxy->OnApps(std::move(deltas), apps::AppType::kArc,
                /*should_notify_initialized=*/true);

  // Turn off arc.
  profile_->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, false);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  // Notification should be visible, because arc is off.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_NoNotificationAfterDismiss) {
  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));

  CloseNotification(/*by_user=*/true);
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));

  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));
}

TEST_F(ExtendedUpdatesControllerTest,
       OnEolInfo_ReShowNotificationIfNotDismiss) {
  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));

  CloseNotification(/*by_user=*/false);
  EXPECT_THAT(ShowingNotificationCount(), Eq(0));

  controller()->OnEolInfo(profile_, eol_info);
  RunPendingIsOwnerCallbacks(profile_);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(ShowingNotificationCount(), Eq(1));
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_DoesNotCrashForOTRProfiles) {
  auto eol_info = MakeEligibleEolInfo();
  Profile* otr_profile = profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(), true);

  controller()->OnEolInfo(otr_profile, eol_info);

  EXPECT_TRUE(true);
}

}  // namespace ash
