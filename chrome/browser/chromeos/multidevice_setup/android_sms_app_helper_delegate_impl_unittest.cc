// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/android_sms_app_helper_delegate_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

class AndroidSmsAppHelperDelegateImplTest : public testing::Test {
 protected:
  AndroidSmsAppHelperDelegateImplTest()
      : host_content_settings_map_(
            HostContentSettingsMapFactory::GetForProfile(&profile_)) {}

  ~AndroidSmsAppHelperDelegateImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    host_content_settings_map_->ClearSettingsForOneType(
        ContentSettingsType::CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
    test_pending_app_manager_ =
        std::make_unique<web_app::TestPendingAppManager>();
    android_sms_app_helper_delegate_ =
        base::WrapUnique(new AndroidSmsAppHelperDelegateImpl(
            test_pending_app_manager_.get(), host_content_settings_map_));
  }

  web_app::TestPendingAppManager* test_pending_app_manager() {
    return test_pending_app_manager_.get();
  }

  void InstallApp() {
    android_sms_app_helper_delegate_->InstallAndroidSmsApp();
  }

  void InstallAndLaunchApp() {
    android_sms_app_helper_delegate_->InstallAndLaunchAndroidSmsApp();
  }

  ContentSetting GetNotificationSetting() {
    std::unique_ptr<base::Value> notification_settings_value =
        host_content_settings_map_->GetWebsiteSetting(
            chromeos::android_sms::GetAndroidMessagesURL(),
            GURL() /* top_level_url */,
            ContentSettingsType::CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
            content_settings::ResourceIdentifier(), nullptr);
    return static_cast<ContentSetting>(notification_settings_value->GetInt());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  HostContentSettingsMap* host_content_settings_map_;
  std::unique_ptr<web_app::TestPendingAppManager> test_pending_app_manager_;
  std::unique_ptr<AndroidSmsAppHelperDelegate> android_sms_app_helper_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppHelperDelegateImplTest);
};

TEST_F(AndroidSmsAppHelperDelegateImplTest, TestInstallMessagesApp) {
  EXPECT_NE(ContentSetting::CONTENT_SETTING_ALLOW, GetNotificationSetting());
  InstallApp();

  std::vector<web_app::PendingAppManager::AppInfo> expected_apps_to_install;
  expected_apps_to_install.emplace_back(
      chromeos::android_sms::GetAndroidMessagesURLWithParams(),
      web_app::LaunchContainer::kWindow, web_app::InstallSource::kInternal,
      web_app::PendingAppManager::AppInfo::kDefaultCreateShortcuts,
      true /* override_previous_user_uninstall */,
      true /* bypass_service_worker_check */);
  EXPECT_EQ(expected_apps_to_install,
            test_pending_app_manager()->install_requests());
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW, GetNotificationSetting());
}

TEST_F(AndroidSmsAppHelperDelegateImplTest, TestInstallAndLaunchMessagesApp) {
  // TODO(crbug/876972): Figure out how to actually test the launching of the
  // app here.
}

}  // namespace multidevice_setup

}  // namespace chromeos
