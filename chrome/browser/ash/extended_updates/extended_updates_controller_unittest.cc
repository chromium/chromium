// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/extended_updates/extended_updates_notification.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr char kTimeNow[] = "2024-03-20";
constexpr char kTimePast[] = "2024-02-10";
constexpr char kTimeFarPast[] = "2023-12-25";
constexpr char kTimeFuture[] = "2024-04-30";
constexpr char kTimeFarFuture[] = "2025-05-15";

class ExtendedUpdatesControllerTest
    : public DeviceSettingsTestBase,
      public NotificationDisplayService::Observer {
 public:
  ExtendedUpdatesControllerTest() = default;
  ExtendedUpdatesControllerTest(const ExtendedUpdatesControllerTest&) = delete;
  ExtendedUpdatesControllerTest& operator=(
      const ExtendedUpdatesControllerTest&) = delete;
  ~ExtendedUpdatesControllerTest() override = default;

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kExtendedUpdatesOptInFeature);

    owner_key_util_->ImportPrivateKeyAndSetPublicKey(
        device_policy_->GetSigningKey());
    InitOwner(
        AccountId::FromUserEmail(device_policy_->policy_data().username()),
        true);
    FlushDeviceSettings();

    test_clock_.SetNow(GetTime(kTimeNow));
    controller()->SetClockForTesting(&test_clock_);

    notification_display_service_observation_.Observe(
        NotificationDisplayService::GetForProfile(profile_.get()));
  }

  void TearDown() override {
    notification_display_service_observation_.Reset();

    controller()->SetClockForTesting(base::DefaultClock::GetInstance());

    DeviceSettingsTestBase::TearDown();
  }

  // NotificationDisplayService::Observer overrides.
  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata) override {
    displayed_notifications_.push_back(notification.id());
  }
  void OnNotificationClosed(const std::string& notification_id) override {}
  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override {}

 protected:
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

  void ExpectNotificationShown() {
    EXPECT_THAT(displayed_notifications_,
                ElementsAre(ExtendedUpdatesNotification::kNotificationId));
  }

  void ExpectNoNotification() {
    EXPECT_THAT(displayed_notifications_, IsEmpty());
  }

  base::test::ScopedFeatureList feature_list_;
  ScopedTestingCrosSettings cros_settings_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
  base::ScopedObservation<NotificationDisplayService,
                          NotificationDisplayService::Observer>
      notification_display_service_observation_{this};

  base::SimpleTestClock test_clock_;
  std::vector<std::string> displayed_notifications_;
};

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_Eligible) {
  EXPECT_TRUE(
      controller()->IsOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kExtendedUpdatesOptInFeature);
  EXPECT_FALSE(
      controller()->IsOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_PastEol) {
  auto params = MakeEligibleParams();
  params.eol_passed = true;
  EXPECT_FALSE(controller()->IsOptInEligible(profile_.get(), params));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_BeforeExtendedDate) {
  auto params = MakeEligibleParams();
  params.extended_date_passed = false;
  EXPECT_FALSE(controller()->IsOptInEligible(profile_.get(), params));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_OptInNotRequired) {
  auto params = MakeEligibleParams();
  params.opt_in_required = false;
  EXPECT_FALSE(controller()->IsOptInEligible(profile_.get(), params));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_AlreadyOptedIn) {
  OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get())
      ->SetBoolean(kDeviceExtendedAutoUpdateEnabled, true);
  EXPECT_FALSE(
      controller()->IsOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_NotOwner) {
  cros_settings_.device_settings()->SetCurrentUserIsOwner(false);
  EXPECT_FALSE(
      controller()->IsOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptInEligible_IsManaged) {
  test_install_attributes_.Get()->SetCloudManaged("fake_domain", "fake_id");
  EXPECT_FALSE(
      controller()->IsOptInEligible(profile_.get(), MakeEligibleParams()));
}

TEST_F(ExtendedUpdatesControllerTest, IsOptedIn_NotOptedInByDefault) {
  EXPECT_FALSE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, IsOptedIn_OptedIn) {
  OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get())
      ->SetBoolean(kDeviceExtendedAutoUpdateEnabled, true);
  EXPECT_TRUE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_Success) {
  EXPECT_FALSE(controller()->IsOptedIn());
  EXPECT_TRUE(controller()->OptIn(profile_.get()));
  EXPECT_TRUE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kExtendedUpdatesOptInFeature);
  EXPECT_FALSE(controller()->OptIn(profile_.get()));
  EXPECT_FALSE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_AlreadyOptedIn) {
  EXPECT_TRUE(controller()->OptIn(profile_.get()));
  EXPECT_FALSE(controller()->OptIn(profile_.get()));
  EXPECT_TRUE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_NotOwner) {
  cros_settings_.device_settings()->SetCurrentUserIsOwner(false);
  EXPECT_FALSE(controller()->OptIn(profile_.get()));
  EXPECT_FALSE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OptIn_IsManaged) {
  test_install_attributes_.Get()->SetCloudManaged("fake_domain", "fake_id");
  EXPECT_FALSE(controller()->OptIn(profile_.get()));
  EXPECT_FALSE(controller()->IsOptedIn());
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_ShowsNotification) {
  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_.get(), eol_info);

  task_environment_.RunUntilIdle();
  ExpectNotificationShown();
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_BeforeExtendedDate) {
  auto eol_info = MakeEligibleEolInfo();
  eol_info.eol_date = GetTime(kTimeFarFuture);
  eol_info.extended_date = GetTime(kTimeFuture);
  controller()->OnEolInfo(profile_.get(), eol_info);

  task_environment_.RunUntilIdle();
  ExpectNoNotification();
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_AfterEol) {
  auto eol_info = MakeEligibleEolInfo();
  eol_info.eol_date = GetTime(kTimePast);
  eol_info.extended_date = GetTime(kTimeFarPast);
  controller()->OnEolInfo(profile_.get(), eol_info);

  task_environment_.RunUntilIdle();
  ExpectNoNotification();
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_OptInNotRequired) {
  auto eol_info = MakeEligibleEolInfo();
  eol_info.extended_opt_in_required = false;
  controller()->OnEolInfo(profile_.get(), eol_info);

  task_environment_.RunUntilIdle();
  ExpectNoNotification();
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kExtendedUpdatesOptInFeature);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_.get(), eol_info);

  task_environment_.RunUntilIdle();
  ExpectNoNotification();
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_NotOwner) {
  cros_settings_.device_settings()->SetCurrentUserIsOwner(false);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_.get(), eol_info);

  task_environment_.RunUntilIdle();
  ExpectNoNotification();
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_OwnershipNotLoadedYet) {
  // Simulate owner private key not loaded yet.
  OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get())
      ->SetPrivateKeyForTesting(nullptr);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_.get(), eol_info);

  // Verify no notification yet.
  task_environment_.RunUntilIdle();
  ExpectNoNotification();

  // Finish loading owner private key.
  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  FlushDeviceSettings();

  // Verify notification is now shown.
  task_environment_.RunUntilIdle();
  ExpectNotificationShown();
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_ArcAppsInitializedButNoApp) {
  ASSERT_TRUE(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      profile_.get()));
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_.get());
  proxy->OnApps({}, apps::AppType::kArc, /*should_notify_initialized=*/true);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_.get(), eol_info);

  task_environment_.RunUntilIdle();
  ExpectNotificationShown();
}

TEST_F(ExtendedUpdatesControllerTest, OnEolInfo_ArcAppsInitializedWithApps) {
  ASSERT_TRUE(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      profile_.get()));
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_.get());
  std::vector<apps::AppPtr> deltas;
  deltas.push_back(MakeArcApp("app1"));
  deltas.push_back(MakeArcApp("app2"));
  proxy->OnApps(std::move(deltas), apps::AppType::kArc,
                /*should_notify_initialized=*/true);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_.get(), eol_info);

  task_environment_.RunUntilIdle();
  ExpectNoNotification();
}

TEST_F(ExtendedUpdatesControllerTest,
       OnEolInfo_ArcAppsInitializedWithDisabledApps) {
  ASSERT_TRUE(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      profile_.get()));
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_.get());
  std::vector<apps::AppPtr> deltas;
  auto app = MakeArcApp("app1");
  app->readiness = apps::Readiness::kDisabledByUser;
  deltas.push_back(std::move(app));
  proxy->OnApps(std::move(deltas), apps::AppType::kArc,
                /*should_notify_initialized=*/true);

  auto eol_info = MakeEligibleEolInfo();
  controller()->OnEolInfo(profile_.get(), eol_info);

  task_environment_.RunUntilIdle();
  ExpectNotificationShown();
}

}  // namespace ash
