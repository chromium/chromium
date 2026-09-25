// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_ui_delegate.h"

#include <memory>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/webui/help_app_ui/help_app_ui.mojom-shared.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class MockSettingsWindowManager : public chrome::SettingsWindowManager {
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

class HelpAppUiDelegateTest : public testing::Test {
 public:
  HelpAppUiDelegateTest() = default;
  ~HelpAppUiDelegateTest() override = default;

  void SetUp() override {
    user_session_test_env_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            TestingBrowserProcess::GetGlobal()));

    const AccountId account_id =
        AccountId::FromUserEmailGaiaId("user@test.com", GaiaId("1234567890"));
    ASSERT_TRUE(user_session_test_env_->AddRegularUser(account_id));
    user_session_test_env_->LogIn(account_id);

    Profile* profile = Profile::FromBrowserContext(
        BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id));
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile, nullptr);
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());
    delegate_ = std::make_unique<ChromeHelpAppUIDelegate>(web_ui_.get());
  }

  void TearDown() override {
    delegate_.reset();
    web_ui_.reset();
    web_contents_.reset();
    user_session_test_env_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  std::unique_ptr<ash::test::UserSessionTestEnvironment> user_session_test_env_;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;
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
  scoped_feature_list_.InitWithFeatures(
      {ash::features::kBorealis, ash::features::kBorealisPermitted}, {});

  base::test::TestFuture<help_app::mojom::DeviceInfoPtr> info_future;
  delegate_->GetDeviceInfo(info_future.GetCallback());

  help_app::mojom::DeviceInfoPtr device_info_ptr = info_future.Take();
  ASSERT_EQ(device_info_ptr->is_steam_allowed, true);
}

struct OpenSettingsScenario {
  // Component to use when calling OpenSettings.
  help_app::mojom::SettingsComponent component;

  // Expected url string shown.
  std::string_view expected_url;
};

class HelpAppUiDelegateOpenSettingsTest
    : public HelpAppUiDelegateTest,
      public testing::WithParamInterface<OpenSettingsScenario> {};

constexpr OpenSettingsScenario kOpenSettingsScenario[] = {
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
  MockSettingsWindowManager mock_settings_window_manager;

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
