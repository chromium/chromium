// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/eche_alert_generator.h"
#include "ash/webui/eche_app_ui/system_info.h"
#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/eche_app/eche_app_notification_controller.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace eche_app {

class EcheAppManagerFactoryTest : public ChromeAshTestBase {
 protected:
  EcheAppManagerFactoryTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA},
        /*disabled_features=*/{});
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    if (profile_manager_->SetUp()) {
      profile_ = profile_manager_->CreateTestingProfile("testing_profile");
    }
  }
  ~EcheAppManagerFactoryTest() override = default;
  EcheAppManagerFactoryTest(const EcheAppManagerFactoryTest&) = delete;
  EcheAppManagerFactoryTest& operator=(const EcheAppManagerFactoryTest&) =
      delete;

  // ChromeAshTestBase::
  void SetUp() override {
    DCHECK(profile_);
    DCHECK(test_web_view_factory_.get());
    ChromeAshTestBase::SetUp();
    connection_handler_ = std::make_unique<EcheConnectionStatusHandler>();
    apps_launch_info_provider_ =
        std::make_unique<AppsLaunchInfoProvider>(connection_handler_.get());
    apps_launch_info_provider_->SetAppLaunchInfo(
        mojom::AppStreamLaunchEntryPoint::APPS_LIST);
    eche_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();
    phone_hub_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->phone_hub_tray();
  }

  void TearDown() override {
    phone_hub_tray_ = nullptr;
    eche_tray_ = nullptr;
    ChromeAshTestBase::TearDown();
  }

  const message_center::Notification* GetNotification(
      const std::string& notification_id) {
    const user_manager::User& user = CHECK_DEREF(
        BrowserContextHelper::Get()->GetUserByBrowserContext(GetProfile()));
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        CreateUserScopedNotificationId(notification_id, user.username_hash()));
  }

  FakeChromeUserManager* GetFakeUserManager() {
    return fake_user_manager_.Get();
  }

  base::WeakPtr<EcheAppManagerFactory> GetEcheAppManagerFactoryWeakPtr() {
    return EcheAppManagerFactory::GetInstance()->weak_ptr_factory_.GetWeakPtr();
  }

  TestingProfile* GetProfile() { return profile_; }
  AppsLaunchInfoProvider* GetAppsLaunchInfoProvider() {
    return apps_launch_info_provider_.get();
  }
  EcheTray* eche_tray() { return eche_tray_; }
  PhoneHubTray* phone_hub_tray() { return phone_hub_tray_; }

 private:
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_{std::make_unique<FakeChromeUserManager>()};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<EcheConnectionStatusHandler> connection_handler_;
  std::unique_ptr<AppsLaunchInfoProvider> apps_launch_info_provider_;
  raw_ptr<EcheTray> eche_tray_ = nullptr;
  raw_ptr<PhoneHubTray> phone_hub_tray_ = nullptr;
  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

class EcheAppManagerFactoryWithBackgroundTest : public ChromeAshTestBase {
 protected:
  EcheAppManagerFactoryWithBackgroundTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA},
        /*disabled_features=*/{});
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    if (profile_manager_->SetUp()) {
      profile_ = profile_manager_->CreateTestingProfile("testing_profile");
    }
  }
  ~EcheAppManagerFactoryWithBackgroundTest() override = default;
  EcheAppManagerFactoryWithBackgroundTest(
      const EcheAppManagerFactoryWithBackgroundTest&) = delete;
  EcheAppManagerFactoryWithBackgroundTest& operator=(
      const EcheAppManagerFactoryWithBackgroundTest&) = delete;

  // ChromeAshTestBase:
  void SetUp() override {
    DCHECK(profile_);
    DCHECK(test_web_view_factory_.get());
    ChromeAshTestBase::SetUp();
    connection_handler_ = std::make_unique<EcheConnectionStatusHandler>();
    apps_launch_info_provider_ =
        std::make_unique<AppsLaunchInfoProvider>(connection_handler_.get());
    apps_launch_info_provider_->SetAppLaunchInfo(
        mojom::AppStreamLaunchEntryPoint::APPS_LIST);
    eche_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();
  }

  void TearDown() override {
    eche_tray_ = nullptr;
    ChromeAshTestBase::TearDown();
  }

  TestingProfile* GetProfile() { return profile_; }
  AppsLaunchInfoProvider* GetAppsLaunchInfoProvider() {
    return apps_launch_info_provider_.get();
  }

  EcheTray* eche_tray() { return eche_tray_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<EcheConnectionStatusHandler> connection_handler_;
  std::unique_ptr<AppsLaunchInfoProvider> apps_launch_info_provider_;
  raw_ptr<EcheTray> eche_tray_ = nullptr;
  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

TEST_F(EcheAppManagerFactoryTest, LaunchEcheApp) {
  const int64_t user_id = 1;
  const char16_t visible_name_1[] = u"Fake App 1";
  const char package_name_1[] = "com.fakeapp1";
  const char16_t phone_name[] = u"your phone";

  EcheAppManagerFactory::LaunchEcheApp(
      GetProfile(), /*notification_id=*/std::nullopt, package_name_1,
      visible_name_1, user_id, gfx::Image(), phone_name,
      GetAppsLaunchInfoProvider());
  // Wait for Eche Tray to load Eche Web to complete
  base::RunLoop().RunUntilIdle();
  // Eche icon should be visible after launch.
  EXPECT_TRUE(phone_hub_tray()->eche_icon_view()->GetVisible());

  // Launch different application should not recreate widget
  views::Widget* widget = eche_tray()->GetBubbleWidget();
  const char16_t visible_name_2[] = u"Fake App 2";
  const char package_name_2[] = "com.fakeapp2";
  EcheAppManagerFactory::LaunchEcheApp(
      GetProfile(), /*notification_id=*/std::nullopt, package_name_2,
      visible_name_2, user_id, gfx::Image(), phone_name,
      GetAppsLaunchInfoProvider());
  // Wait for Eche Tray to load Eche Web to complete
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(widget, eche_tray()->GetBubbleWidget());
}

TEST_F(EcheAppManagerFactoryTest, LaunchedAppInfo) {
  const int64_t user_id = 1;
  const std::u16string visible_name = u"Fake App";
  const std::string package_name = "com.fakeapp";
  const gfx::Image icon = gfx::test::CreateImage(100, 100);
  const std::u16string phone_name = u"your phone";

  EcheAppManagerFactory::LaunchEcheApp(
      GetProfile(), /*notification_id=*/std::nullopt, package_name,
      visible_name, user_id, icon, phone_name, GetAppsLaunchInfoProvider());

  std::unique_ptr<LaunchedAppInfo> launched_app_info =
      EcheAppManagerFactory::GetInstance()->GetLastLaunchedAppInfo();

  EXPECT_EQ(launched_app_info->user_id(), user_id);
  EXPECT_EQ(launched_app_info->visible_name(), visible_name);
  EXPECT_EQ(launched_app_info->package_name(), package_name);
  EXPECT_EQ(launched_app_info->icon(), icon);
}

TEST_F(EcheAppManagerFactoryTest, CloseConnectionOrLaunchErrorNotifications) {
  user_manager::User* user =
      GetFakeUserManager()->AddUser(user_manager::StubAccountId());
  GetFakeUserManager()->LoginUser(user->GetAccountId());
  AnnotatedAccountId::Set(GetProfile(), user->GetAccountId());

  base::WeakPtr<EcheAppManagerFactory> factory =
      GetEcheAppManagerFactoryWeakPtr();
  std::u16string title = u"title";
  std::u16string message = u"message";
  EcheAppManagerFactory::ShowNotification(
      factory, GetProfile(), title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kNative,
          LaunchAppHelper::NotificationInfo::NotificationType::kScreenLock));
  EcheAppManagerFactory::ShowNotification(
      factory, GetProfile(), title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kWebUI,
          mojom::WebNotificationType::CONNECTION_FAILED));
  EcheAppManagerFactory::ShowNotification(
      factory, GetProfile(), title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kWebUI,
          mojom::WebNotificationType::DEVICE_IDLE));
  EcheAppManagerFactory::ShowNotification(
      factory, GetProfile(), title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kWebUI,
          mojom::WebNotificationType::INVALID_NOTIFICATION));
  ASSERT_TRUE(factory);
  factory->CloseConnectionOrLaunchErrorNotifications();

  const message_center::Notification* notification =
      GetNotification(kEcheAppScreenLockNotifierId);
  ASSERT_TRUE(notification);
  notification = GetNotification(kEcheAppRetryConnectionNotifierId);
  ASSERT_FALSE(notification);
  notification = GetNotification(kEcheAppInactivityNotifierId);
  ASSERT_FALSE(notification);
  notification = GetNotification(kEcheAppFromWebWithoutButtonNotifierId);
  ASSERT_FALSE(notification);
}

TEST_F(EcheAppManagerFactoryWithBackgroundTest, LaunchEcheApp) {
  const int64_t user_id = 1;
  const char16_t visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  const char16_t phone_name[] = u"your phone";

  EcheAppManagerFactory::LaunchEcheApp(
      GetProfile(), /*notification_id=*/std::nullopt, package_name,
      visible_name, user_id, gfx::Image(), phone_name,
      GetAppsLaunchInfoProvider());
  // Wait for Eche Tray to load Eche Web to complete
  base::RunLoop().RunUntilIdle();
  // Eche tray should be visible when streaming is active, not ative when
  // launch.
  EXPECT_FALSE(eche_tray()->is_active());
}

TEST_F(EcheAppManagerFactoryTest, GetSystemInfo) {
  const char kLsbRelease[] =
      "CHROMEOS_RELEASE_NAME=Non Chrome OS\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";
  const base::Time lsb_release_time(
      base::Time::FromSecondsSinceUnixEpoch(12345.6));
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, lsb_release_time);
  std::unique_ptr<SystemInfo> system_info =
      EcheAppManagerFactory::GetInstance()->GetSystemInfo(GetProfile());

  EXPECT_EQ("1.2.3", system_info->GetOsVersion());
  EXPECT_EQ("Chrome device", system_info->GetDeviceType());
}

}  // namespace eche_app
}  // namespace ash
