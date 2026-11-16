// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_keyboard_backlight_provider_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/shell.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"
#include "ash/test/ash_test_helper.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/users/profile_user_manager_controller.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash::personalization_app {

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@personalization";
constexpr AccountId::Literal kAccountId =
    AccountId::Literal::FromUserEmailGaiaId(kFakeTestEmail,
                                            GaiaId::Literal("1111"));

class TestKeyboardBacklightObserver
    : public ash::personalization_app::mojom::KeyboardBacklightObserver {
 public:
  void OnBacklightStateChanged(
      mojom::CurrentBacklightStatePtr current_backlight_state) override {
    current_backlight_state_ = std::move(current_backlight_state);
  }

  void OnWallpaperColorChanged(SkColor wallpaper_color) override {
    wallpaper_color_ = wallpaper_color;
  }

  mojo::PendingRemote<
      ash::personalization_app::mojom::KeyboardBacklightObserver>
  pending_remote() {
    if (keyboard_backlight_observer_receiver_.is_bound()) {
      keyboard_backlight_observer_receiver_.reset();
    }

    return keyboard_backlight_observer_receiver_.BindNewPipeAndPassRemote();
  }

  mojom::CurrentBacklightState* current_backlight_state() {
    keyboard_backlight_observer_receiver_.FlushForTesting();
    return current_backlight_state_.get();
  }

  std::optional<SkColor> wallpaper_color() {
    keyboard_backlight_observer_receiver_.FlushForTesting();
    return wallpaper_color_;
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::KeyboardBacklightObserver>
      keyboard_backlight_observer_receiver_{this};
  mojom::CurrentBacklightStatePtr current_backlight_state_ =
      mojom::CurrentBacklightState::NewColor(mojom::BacklightColor::kRed);
  std::optional<SkColor> wallpaper_color_;
};

}  // namespace

class PersonalizationAppKeyboardBacklightProviderImplTest
    : public ChromeAshTestBase {
 public:
  PersonalizationAppKeyboardBacklightProviderImplTest() = default;
  PersonalizationAppKeyboardBacklightProviderImplTest(
      const PersonalizationAppKeyboardBacklightProviderImplTest&) = delete;
  PersonalizationAppKeyboardBacklightProviderImplTest& operator=(
      const PersonalizationAppKeyboardBacklightProviderImplTest&) = delete;
  ~PersonalizationAppKeyboardBacklightProviderImplTest() override = default;

 protected:
  // ChromeAshTestBase:
  void SetUpInitParams(ash::AshTestHelper::InitParams& init_params) override {
    ChromeAshTestBase::SetUpInitParams(init_params);
    init_params.users.clear();
  }

  void SetUp() override {
    ChromeAshTestBase::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_user_manager_controller_ =
        std::make_unique<ash::ProfileUserManagerController>(
            profile_manager_.profile_manager(),
            user_manager::UserManager::Get());

    ASSERT_TRUE(
        ash_test_helper()->user_session_test_environment().AddRegularUser(
            kAccountId));
    ash_test_helper()->user_session_test_environment().LogIn(kAccountId);
    SimulateUserLogin(
        ash::LoginInfo{.account_id = kAccountId,
                       .display_email = kFakeTestEmail,
                       .user_type = user_manager::UserType::kRegular});

    ash::ScopedAccountIdAnnotator annotator(profile_manager_.profile_manager(),
                                            kAccountId);
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    keyboard_backlight_provider_ =
        std::make_unique<PersonalizationAppKeyboardBacklightProviderImpl>(
            &web_ui_);
    keyboard_backlight_color_controller_ =
        std::make_unique<KeyboardBacklightColorController>(
            TestingBrowserProcess::GetGlobal()->local_state());
    keyboard_backlight_color_controller_->OnRgbKeyboardSupportedChanged(true);
    keyboard_backlight_provider_->SetKeyboardBacklightColorControllerForTesting(
        keyboard_backlight_color_controller_.get());
    keyboard_backlight_provider_->BindInterface(
        keyboard_backlight_provider_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    keyboard_backlight_provider_remote_.reset();
    keyboard_backlight_provider_.reset();
    keyboard_backlight_color_controller_.reset();
    web_ui_.set_web_contents(nullptr);
    web_contents_.reset();
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
    profile_user_manager_controller_.reset();
    ChromeAshTestBase::TearDown();
  }

  TestingProfile* profile() { return profile_; }

  mojo::Remote<ash::personalization_app::mojom::KeyboardBacklightProvider>*
  keyboard_backlight_provider_remote() {
    return &keyboard_backlight_provider_remote_;
  }

  PersonalizationAppKeyboardBacklightProviderImpl*
  keyboard_backlight_provider() {
    return keyboard_backlight_provider_.get();
  }

  void SetKeyboardBacklightObserver() {
    keyboard_backlight_provider_remote_->SetKeyboardBacklightObserver(
        test_keyboard_backlight_observer_.pending_remote());
  }

  mojom::CurrentBacklightState* ObservedBacklightColor() {
    keyboard_backlight_provider_remote_.FlushForTesting();
    return test_keyboard_backlight_observer_.current_backlight_state();
  }

  std::optional<SkColor> ObservedWallpaperColor() {
    keyboard_backlight_provider_remote_.FlushForTesting();
    return test_keyboard_backlight_observer_.wallpaper_color();
  }

  void set_rgb_capability(rgbkbd::RgbKeyboardCapabilities capability) {
    RgbKeyboardManager* rgb_keyboard_manager =
        Shell::Get()->rgb_keyboard_manager();
    rgb_keyboard_manager->OnCapabilityUpdatedForTesting(capability);
  }

 private:
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  std::unique_ptr<ash::ProfileUserManagerController>
      profile_user_manager_controller_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  std::unique_ptr<PersonalizationAppKeyboardBacklightProviderImpl>
      keyboard_backlight_provider_;
  std::unique_ptr<KeyboardBacklightColorController>
      keyboard_backlight_color_controller_;
  mojo::Remote<ash::personalization_app::mojom::KeyboardBacklightProvider>
      keyboard_backlight_provider_remote_;
  TestKeyboardBacklightObserver test_keyboard_backlight_observer_;
};

TEST_F(PersonalizationAppKeyboardBacklightProviderImplTest, SetBacklightColor) {
  SetKeyboardBacklightObserver();
  keyboard_backlight_provider_remote()->FlushForTesting();
  keyboard_backlight_provider()->SetBacklightColor(
      mojom::BacklightColor::kBlue);

  // Verify JS side is notified.
  EXPECT_TRUE(ObservedBacklightColor()->is_color());
  EXPECT_EQ(mojom::BacklightColor::kBlue,
            ObservedBacklightColor()->get_color());
}

TEST_F(PersonalizationAppKeyboardBacklightProviderImplTest,
       SetBacklightZoneColor) {
  SetKeyboardBacklightObserver();
  set_rgb_capability(rgbkbd::RgbKeyboardCapabilities::kFourZoneFortyLed);
  keyboard_backlight_provider_remote()->FlushForTesting();

  keyboard_backlight_provider()->SetBacklightColor(
      mojom::BacklightColor::kBlue);
  EXPECT_TRUE(ObservedBacklightColor()->is_color());

  keyboard_backlight_provider()->SetBacklightZoneColor(
      1, mojom::BacklightColor::kRed);
  // Verify JS side is notified.
  EXPECT_TRUE(ObservedBacklightColor()->is_zone_colors());
  const auto zone_colors = ObservedBacklightColor()->get_zone_colors();
  for (size_t i = 0; i < zone_colors.size(); i++) {
    EXPECT_EQ(zone_colors[i], i == 1 ? mojom::BacklightColor::kRed
                                     : mojom::BacklightColor::kBlue);
  }
}

TEST_F(PersonalizationAppKeyboardBacklightProviderImplTest,
       ObserveWallpaperColor) {
  SetKeyboardBacklightObserver();
  keyboard_backlight_provider_remote()->FlushForTesting();
  keyboard_backlight_provider()->OnWallpaperColorsChanged();

  // Verify JS side is notified.
  EXPECT_TRUE(ObservedWallpaperColor().has_value());
}

}  // namespace ash::personalization_app
