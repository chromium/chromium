// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/pixel/ash_pixel_test_helper.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/command_line.h"
#include "base/i18n/base_i18n_switches.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

// The color of the default wallpaper in pixel tests.
constexpr SkColor kWallPaperColor = SK_ColorMAGENTA;

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

}  // namespace

AshPixelTestHelper::AshPixelTestHelper(pixel_test::InitParams params)
    : params_(std::move(params)),
      scoped_locale_(base::test::ScopedRestoreICUDefaultLocale(kLocale)),
      time_zone_(base::test::ScopedRestoreDefaultTimezone(kTimeZone)) {
  if (params_.under_rtl) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kForceUIDirection, ::switches::kForceDirectionRTL);
  }
}

AshPixelTestHelper::~AshPixelTestHelper() = default;

void AshPixelTestHelper::StabilizeUi(const gfx::Size& wallpaper_size) {
  MaybeSetDarkMode();
  SetWallPaper(wallpaper_size);
  SetBatteryState();
}

void AshPixelTestHelper::MaybeSetDarkMode() {
  // If the dark/light mode feature is not enabled, the dark mode is used as
  // default so return early.
  if (!features::IsDarkLightModeEnabled())
    return;

  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  if (!dark_light_mode_controller->IsDarkModeEnabled())
    dark_light_mode_controller->ToggleColorMode();
}

void AshPixelTestHelper::SetWallPaper(const gfx::Size& wallpaper_size) {
  auto* controller = Shell::Get()->wallpaper_controller();
  controller->set_wallpaper_reload_no_delay_for_test();

  switch (params_.wallpaper_init_type) {
    case pixel_test::WallpaperInitType::kRegular: {
      gfx::ImageSkia wallpaper_image =
          CreateImage(wallpaper_size, kWallPaperColor);
      controller->set_allow_blur_or_shield_for_testing();

      // Use the one shot wallpaper to ensure that the custom wallpaper set by
      // pixel tests does not go away after changing display metrics.
      controller->ShowWallpaperImage(
          wallpaper_image,
          WallpaperInfo{/*in_location=*/std::string(),
                        /*in_layout=*/WALLPAPER_LAYOUT_STRETCH,
                        /*in_type=*/WallpaperType::kOneShot,
                        /*in_date=*/base::Time::Now().LocalMidnight()},
          /*preview_mode=*/false, /*always_on_top=*/false);
      break;
    }
    case pixel_test::WallpaperInitType::kPolicy:
      controller->set_bypass_decode_for_testing();

      // A dummy file path is sufficient for setting a default policy wallpaper.
      controller->SetDevicePolicyWallpaperPath(base::FilePath("tmp.png"));

      break;
  }
}

void AshPixelTestHelper::SetBatteryState() {
  power_manager::PowerSupplyProperties proto;
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  proto.set_battery_percent(50.0);
  chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(proto);
}

}  // namespace ash
