// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_ui_delegate.h"

#include <memory>

#include "ash/webui/help_app_ui/help_app_ui.mojom-shared.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/borealis/testing/features.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class MockSetingsWindowManager : public chrome::SettingsWindowManager {
 public:
  MOCK_METHOD(void,
              ShowChromePageForProfile,
              (Profile * profile,
               const GURL& gurl,
               int64_t display_id,
               apps::LaunchCallback callback),
              (override));
};

}  // namespace

class HelpAppUiDelegateTest : public BrowserWithTestWindowTest {
 public:
  HelpAppUiDelegateTest() : web_ui_(std::make_unique<content::TestWebUI>()) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    user_manager_ = static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
    web_ui_->set_web_contents(contents);
    delegate_ = std::make_unique<ChromeHelpAppUIDelegate>(web_ui());
  }

  void TearDown() override {
    delegate_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  content::WebUI* web_ui() { return web_ui_.get(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<FakeChromeUserManager, DanglingUntriaged> user_manager_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<ChromeHelpAppUIDelegate> delegate_;
};

TEST_F(HelpAppUiDelegateTest, DeviceInfoWhenBorealisIsNotAllowed) {
  base::test::TestFuture<help_app::mojom::DeviceInfoPtr> info_future;
  delegate_->GetDeviceInfo(info_future.GetCallback());

  help_app::mojom::DeviceInfoPtr device_info_ptr = info_future.Take();
  ASSERT_EQ(device_info_ptr->is_steam_allowed, false);
}

TEST_F(HelpAppUiDelegateTest, DeviceInfoWhenBorealisIsAllowed) {
  borealis::AllowBorealis(profile(), &scoped_feature_list_, user_manager_,
                          /*also_enable=*/false);

  base::test::TestFuture<help_app::mojom::DeviceInfoPtr> info_future;
  delegate_->GetDeviceInfo(info_future.GetCallback());

  help_app::mojom::DeviceInfoPtr device_info_ptr = info_future.Take();
  ASSERT_EQ(device_info_ptr->is_steam_allowed, true);
}

struct OpenSettingsScenario {
  // Component to use when calling OpenSettings.
  help_app::mojom::SettingsComponent component;

  // Expected url string shown.
  std::string expected_url;
};

class HelpAppUiDelegateOpenSettingsTest
    : public HelpAppUiDelegateTest,
      public testing::WithParamInterface<OpenSettingsScenario> {};

const std::vector<OpenSettingsScenario> kOpenSettingsScenario{
    {.component = ash::help_app::mojom::SettingsComponent::HOME,
     .expected_url = "chrome://os-settings"},
    {.component = ash::help_app::mojom::SettingsComponent::ACCESSIBILITY,
     .expected_url = "chrome://os-settings/osAccessibility"},
    {.component = ash::help_app::mojom::SettingsComponent::BLUETOOTH,
     .expected_url = "chrome://os-settings/bluetoothDevices"},
    {.component = ash::help_app::mojom::SettingsComponent::DISPLAY,
     .expected_url = "chrome://os-settings/display"},
    {.component = ash::help_app::mojom::SettingsComponent::INPUT,
     .expected_url = "chrome://os-settings/osLanguages/input"},
    {.component = ash::help_app::mojom::SettingsComponent::MULTI_DEVICE,
     .expected_url = "chrome://os-settings/multidevice"},
    {.component = ash::help_app::mojom::SettingsComponent::PEOPLE,
     .expected_url = "chrome://os-settings/osPeople"},
    {.component = ash::help_app::mojom::SettingsComponent::PER_DEVICE_KEYBOARD,
     .expected_url = "chrome://os-settings/per-device-keyboard"},
    {.component = ash::help_app::mojom::SettingsComponent::PER_DEVICE_TOUCHPAD,
     .expected_url = "chrome://os-settings/per-device-touchpad"},
    {.component = ash::help_app::mojom::SettingsComponent::PERSONALIZATION,
     .expected_url = "chrome://os-settings/personalization"},
    {.component = ash::help_app::mojom::SettingsComponent::PRINTING,
     .expected_url = "chrome://os-settings/cupsPrinters"},
    {.component = ash::help_app::mojom::SettingsComponent::SECURITY_AND_SIGN_IN,
     .expected_url = "chrome://os-settings/osPrivacy/lockScreen"},
    {.component =
         ash::help_app::mojom::SettingsComponent::TOUCHPAD_REVERSE_SCROLLING,
     .expected_url = "chrome://os-settings/per-device-touchpad?settingId=402"},
    {.component =
         ash::help_app::mojom::SettingsComponent::TOUCHPAD_SIMULATE_RIGHT_CLICK,
     .expected_url = "chrome://os-settings/per-device-touchpad?settingId=446"}};

TEST_P(HelpAppUiDelegateOpenSettingsTest, ShouldShowPageForSettingsComponent) {
  MockSetingsWindowManager mock_settings_window_manager;
  chrome::SettingsWindowManager::SetInstanceForTesting(
      &mock_settings_window_manager);

  EXPECT_CALL(
      mock_settings_window_manager,
      ShowChromePageForProfile(testing::_, GURL(GetParam().expected_url),
                               testing::_, testing::_));

  delegate_->OpenSettings(GetParam().component);
}

INSTANTIATE_TEST_SUITE_P(All,
                         HelpAppUiDelegateOpenSettingsTest,
                         testing::ValuesIn(kOpenSettingsScenario));

}  // namespace ash
