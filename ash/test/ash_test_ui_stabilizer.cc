// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_ui_stabilizer.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/time/time_override.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

// The fake user account only used for pixel tests.
constexpr char kUserForPixelTest[] = "user1@test.com";

// The fake file ids for wallpaper setting in pixel tests.
constexpr char kFakeFileId[] = "file-hash";
constexpr char kWallpaperFileName[] = "test-file";

// The color of the default wallpaper in pixel tests.
constexpr SkColor kWallPaperColor = SK_ColorMAGENTA;

// The string that represents the current time. Used in pixel tests.
constexpr char kFakeNowTimeString[] = "Sun, 6 May 2018 14:30:00 CDT";

// Specify the locale and the time zone used in pixel tests.
constexpr char kLocale[] = "en_US";
constexpr char kTimeZone[] = "America/Chicago";

// Creates a pure color image of the specified size.
gfx::ImageSkia CreateImage(const gfx::Size& image_size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(image_size.width(), image_size.height());
  bitmap.eraseColor(color);
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  return image;
}

// TimeOverrideHelper ----------------------------------------------------------

struct TimeOverrideHelper {
  static base::Time TimeNow() { return current_time; }

  // Used as the current time in ash pixel diff tests.
  static base::Time current_time;
};

base::Time TimeOverrideHelper::current_time;

}  // namespace

AshTestUiStabilizer::AshTestUiStabilizer()
    : scoped_locale_(base::test::ScopedRestoreICUDefaultLocale(kLocale)),
      time_zone_(base::test::ScopedRestoreDefaultTimezone(kTimeZone)),
      account_id_(
          AccountId::FromUserEmailGaiaId(kUserForPixelTest, "test-hash")) {}

AshTestUiStabilizer::~AshTestUiStabilizer() = default;

void AshTestUiStabilizer::StabilizeUi(const gfx::Size& wallpaper_size) {
  MaybeSetDarkMode();
  SetWallPaper(wallpaper_size);
  SetBatteryState();
}

// Overrides the current time. It ensures that `Time::Now()` is constant.
void AshTestUiStabilizer::OverrideTime() {
  ASSERT_TRUE(base::Time::FromString(kFakeNowTimeString,
                                     &TimeOverrideHelper::current_time));
  time_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
      &TimeOverrideHelper::TimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
}

void AshTestUiStabilizer::MaybeSetDarkMode() {
  // If the dark/light mode feature is not enabled, the dark mode is used as
  // default so return early.
  if (!features::IsDarkLightModeEnabled())
    return;

  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  if (!dark_light_mode_controller->IsDarkModeEnabled())
    dark_light_mode_controller->ToggleColorMode();
}

void AshTestUiStabilizer::SetWallPaper(const gfx::Size& wallpaper_size) {
  ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(online_wallpaper_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(custom_wallpaper_dir_.CreateUniqueTempDir());

  auto* controller = Shell::Get()->wallpaper_controller();
  controller->Init(user_data_dir_.GetPath(), online_wallpaper_dir_.GetPath(),
                   custom_wallpaper_dir_.GetPath(),
                   /*device_policy_wallpaper=*/base::FilePath());
  controller->set_wallpaper_reload_no_delay_for_test();
  controller->SetClient(&client_);
  client_.set_fake_files_id_for_account_id(account_id_, kFakeFileId);

  gfx::ImageSkia wallpaper_image = CreateImage(wallpaper_size, kWallPaperColor);
  controller->SetCustomWallpaper(account_id_, kWallpaperFileName,
                                 WALLPAPER_LAYOUT_STRETCH, wallpaper_image,
                                 /*preview_mode=*/false);
}

void AshTestUiStabilizer::SetBatteryState() {
  power_manager::PowerSupplyProperties proto;
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  proto.set_battery_percent(50.0);
  chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(proto);
}

}  // namespace ash
