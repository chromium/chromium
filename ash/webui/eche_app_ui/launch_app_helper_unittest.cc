// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/launch_app_helper.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/screen_lock_manager.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace eche_app {

namespace {

constexpr auto kOneDay = base::Days(1u);
constexpr char kUniqueAppsMetricName[] = "Eche.UniqueAppsStreamed.PerDay";

}  // namespace

class Callback {
 public:
  static void LaunchEcheAppFunction(
      const std::optional<int64_t>& notification_id,
      const std::string& package_name,
      const std::u16string& visible_name,
      const std::optional<int64_t>& user_id,
      const gfx::Image& icon,
      const std::u16string& phone_name,
      AppsLaunchInfoProvider* apps_launch_info_provider) {
    launchEcheApp_ = true;
  }

  static void ShowNotificationFunction(
      const std::optional<std::u16string>& title,
      const std::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info) {
    showNotification_ = true;
  }

  static void CloseNotificationFunction(const std::string& notification_id) {
    closeNotification_ = true;
  }

  static bool getCloseNotification() { return closeNotification_; }
  static bool getShowNotification() { return showNotification_; }
  static bool getLaunchEcheApp() { return launchEcheApp_; }

 private:
  static bool showNotification_;
  static bool closeNotification_;
  static bool launchEcheApp_;
};

bool ash::eche_app::Callback::showNotification_ = false;
bool ash::eche_app::Callback::closeNotification_ = false;
bool ash::eche_app::Callback::launchEcheApp_ = false;

class LaunchAppHelperTest : public ash::AshTestBase {
 protected:
  LaunchAppHelperTest()
      : ash::AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  LaunchAppHelperTest(const LaunchAppHelperTest&) = delete;
  LaunchAppHelperTest& operator=(const LaunchAppHelperTest&) = delete;
  ~LaunchAppHelperTest() override = default;

  // ash::AshTestBase:
  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA},
        /*disabled_features=*/{});

    fake_phone_hub_manager_ = std::make_unique<phonehub::FakePhoneHubManager>();
    connection_handler_ = std::make_unique<EcheConnectionStatusHandler>();
    apps_launch_info_provider_ =
        std::make_unique<AppsLaunchInfoProvider>(connection_handler_.get());
    launch_app_helper_ = std::make_unique<LaunchAppHelper>(
        fake_phone_hub_manager_.get(),
        base::BindRepeating(&Callback::LaunchEcheAppFunction),
        base::BindRepeating(&Callback::ShowNotificationFunction),
        base::BindRepeating(&Callback::CloseNotificationFunction));
    toast_manager_ = Shell::Get()->toast_manager();
  }

  void TearDown() override { AshTestBase::TearDown(); }

  LaunchAppHelper::AppLaunchProhibitedReason ProhibitedByPolicy(
      FeatureStatus status) const {
    return launch_app_helper_->CheckAppLaunchProhibitedReason(status);
  }

  void SetLockStatus(phonehub::ScreenLockManager::LockStatus lock_status) {
    fake_phone_hub_manager_->fake_screen_lock_manager()->SetLockStatusInternal(
        lock_status);
  }

  void ShowToast(const std::u16string& text) {
    launch_app_helper_->ShowToast(text);
  }

  void VerifyShowToast(const std::u16string& text) {
    ToastOverlay* overlay = toast_manager_->GetCurrentOverlayForTesting();
    ASSERT_NE(nullptr, overlay);
    EXPECT_EQ(overlay->GetText(), text);
  }

  void LaunchEcheApp(const std::optional<int64_t>& notification_id,
                     const std::string& package_name,
                     const std::u16string& visible_name,
                     const std::optional<int64_t>& user_id,
                     const gfx::Image& icon,
                     const std::u16string& phone_name) {
    launch_app_helper_->LaunchEcheApp(notification_id, package_name,
                                      visible_name, user_id, icon, phone_name,
                                      apps_launch_info_provider_.get());
  }

  void ShowNotification(
      const std::optional<std::u16string>& title,
      const std::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info) {
    launch_app_helper_->ShowNotification(title, message, std::move(info));
  }

  void CloseNotification(const std::string& notification_id) {
    launch_app_helper_->CloseNotification(notification_id);
  }

  const base::flat_set<std::string> GetLaunchAppHelperPackageSet() {
    return launch_app_helper_->GetSessionPackagesLaunchedForTest();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<phonehub::FakePhoneHubManager> fake_phone_hub_manager_;
  std::unique_ptr<EcheConnectionStatusHandler> connection_handler_;
  std::unique_ptr<AppsLaunchInfoProvider> apps_launch_info_provider_;
  std::unique_ptr<LaunchAppHelper> launch_app_helper_;
  raw_ptr<ToastManagerImpl, DanglingUntriaged> toast_manager_ = nullptr;
};

TEST_F(LaunchAppHelperTest, TestProhibitedByPolicy) {
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);
  SetLockStatus(phonehub::ScreenLockManager::LockStatus::kLockedOn);

  constexpr FeatureStatus kConvertableStatus[] = {
      FeatureStatus::kIneligible,       FeatureStatus::kDisabled,
      FeatureStatus::kConnecting,       FeatureStatus::kConnected,
      FeatureStatus::kDependentFeature, FeatureStatus::kDependentFeaturePending,
  };

  for (const auto status : kConvertableStatus) {
    EXPECT_EQ(LaunchAppHelper::AppLaunchProhibitedReason::kNotProhibited,
              ProhibitedByPolicy(status));
  }

  // The screenLock is required.
  SetCanLockScreen(false);
  SetShouldLockScreenAutomatically(false);

  for (const auto status : kConvertableStatus) {
    EXPECT_EQ(LaunchAppHelper::AppLaunchProhibitedReason::kDisabledByScreenLock,
              ProhibitedByPolicy(status));
  }
}

TEST_F(LaunchAppHelperTest, VerifyShowToast) {
  const std::u16string text = u"text";

  ShowToast(text);

  VerifyShowToast(text);
}

TEST_F(LaunchAppHelperTest, LaunchEcheApp) {
  const std::optional<int64_t> notification_id = 0;
  const std::string package_name = "package_name";
  const std::u16string visible_name = u"visible_name";
  const std::optional<int64_t> user_id = 0;
  const std::u16string phone_name = u"your phone";

  LaunchEcheApp(notification_id, package_name, visible_name, user_id,
                gfx::Image(), phone_name);

  EXPECT_TRUE(Callback::getLaunchEcheApp());
}

TEST_F(LaunchAppHelperTest, ShowNotification) {
  const std::optional<std::u16string> title = u"title";
  const std::optional<std::u16string> message = u"message";

  ShowNotification(
      title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kNative,
          LaunchAppHelper::NotificationInfo::NotificationType::kScreenLock));

  EXPECT_TRUE(Callback::getShowNotification());
}

TEST_F(LaunchAppHelperTest, CloseNotification) {
  const std::string notification_id = "notification.id";

  CloseNotification(notification_id);

  EXPECT_TRUE(Callback::getCloseNotification());
}

TEST_F(LaunchAppHelperTest, UniqueAppPackages) {
  base::HistogramTester histogram_tester;

  const std::optional<int64_t> notification_id = 0;
  const std::string package_name = "package_name";
  const std::u16string visible_name = u"visible_name";
  const std::optional<int64_t> user_id = 0;
  const std::u16string phone_name = u"your phone";

  const std::optional<int64_t> notification_id2 = 1;
  const std::string package_name2 = "package_name2";
  const std::u16string visible_name2 = u"visible_name2";

  LaunchEcheApp(notification_id, package_name, visible_name, user_id,
                gfx::Image(), phone_name);

  histogram_tester.ExpectTotalCount(kUniqueAppsMetricName, 1);
  EXPECT_EQ(1u, GetLaunchAppHelperPackageSet().size());

  LaunchEcheApp(notification_id, package_name, visible_name, user_id,
                gfx::Image(), phone_name);

  histogram_tester.ExpectTotalCount(kUniqueAppsMetricName, 1);
  EXPECT_EQ(1u, GetLaunchAppHelperPackageSet().size());

  LaunchEcheApp(notification_id2, package_name2, visible_name2, user_id,
                gfx::Image(), phone_name);

  histogram_tester.ExpectTotalCount(kUniqueAppsMetricName, 2);
  EXPECT_EQ(2u, GetLaunchAppHelperPackageSet().size());
}

TEST_F(LaunchAppHelperTest, SessionPackagesResetsAfterOneDay) {
  base::HistogramTester histogram_tester;

  const std::optional<int64_t> notification_id = 0;
  const std::string package_name = "package_name";
  const std::u16string visible_name = u"visible_name";
  const std::optional<int64_t> user_id = 0;
  const std::u16string phone_name = u"your phone";

  const std::optional<int64_t> notification_id2 = 1;
  const std::string package_name2 = "package_name2";
  const std::u16string visible_name2 = u"visible_name2";

  LaunchEcheApp(notification_id, package_name, visible_name, user_id,
                gfx::Image(), phone_name);

  histogram_tester.ExpectTotalCount(kUniqueAppsMetricName, 1);
  EXPECT_EQ(1u, GetLaunchAppHelperPackageSet().size());

  LaunchEcheApp(notification_id2, package_name2, visible_name2, user_id,
                gfx::Image(), phone_name);

  histogram_tester.ExpectTotalCount(kUniqueAppsMetricName, 2);
  EXPECT_EQ(2u, GetLaunchAppHelperPackageSet().size());

  task_environment()->FastForwardBy(kOneDay);

  LaunchEcheApp(notification_id, package_name, visible_name, user_id,
                gfx::Image(), phone_name);

  histogram_tester.ExpectTotalCount(kUniqueAppsMetricName, 3);
  EXPECT_EQ(1u, GetLaunchAppHelperPackageSet().size());

  LaunchEcheApp(notification_id, package_name, visible_name, user_id,
                gfx::Image(), phone_name);

  histogram_tester.ExpectTotalCount(kUniqueAppsMetricName, 3);
  EXPECT_EQ(1u, GetLaunchAppHelperPackageSet().size());
}

}  // namespace eche_app
}  // namespace ash
