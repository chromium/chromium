// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/pixel/ash_pixel_test_helper.h"

#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_util.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/run_loop.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace ash {

namespace {

// The color of the default wallpaper in pixel tests.
constexpr SkColor kWallpaperColor = SK_ColorMAGENTA;

// 1x1 wallpaper will tile to cover the display.
constexpr int kWallpaperSize = 1;

// Specify the locale and the time zone used in pixel tests.
constexpr char kLocale[] = "en_US";
constexpr char kTimeZone[] = "America/Chicago";

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

void AshPixelTestHelper::StabilizeUi() {
  MaybeSetDarkMode();
  SetWallpaper();
  SetBatteryState();
}

void AshPixelTestHelper::MaybeSetDarkMode() {
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  if (!dark_light_mode_controller->IsDarkModeEnabled())
    dark_light_mode_controller->ToggleColorMode();
}

void AshPixelTestHelper::SetWallpaper() {
  auto* controller = Shell::Get()->wallpaper_controller();
  // Reset any wallpaper from other test setup.
  controller->CreateEmptyWallpaperForTesting();
  controller->set_wallpaper_reload_no_delay_for_test();

  switch (params_.wallpaper_init_type) {
    case pixel_test::WallpaperInitType::kRegular: {
      gfx::ImageSkia wallpaper_image = CreateSolidColorTestImage(
          {kWallpaperSize, kWallpaperSize}, kWallpaperColor);
      controller->blur_manager()->set_allow_blur_for_testing();
      controller->set_allow_shield_for_testing();

      // Use the one shot wallpaper to ensure that the custom wallpaper set by
      // pixel tests does not go away after changing display metrics.
      controller->ShowWallpaperImage(
          wallpaper_image,
          WallpaperInfo{/*in_location=*/std::string(),
                        /*in_layout=*/WALLPAPER_LAYOUT_TILE,
                        /*in_type=*/WallpaperType::kOneShot,
                        /*in_date=*/base::Time::Now().LocalMidnight()},
          /*preview_mode=*/false, /*always_on_top=*/false);

      if (controller->ShouldCalculateColors()) {
        // Wait for `WallpaperControllerObserver::OnWallpaperColorsChanged` so
        // that colors are finalized before pixel testing views.
        DCHECK(!wallpaper_controller_observation_.IsObserving());
        wallpaper_controller_observation_.Observe(controller);

        base::RunLoop loop;
        DCHECK(!on_wallpaper_finalized_);
        on_wallpaper_finalized_ = loop.QuitClosure();
        loop.Run();
        DCHECK(!wallpaper_controller_observation_.IsObserving());
      }
      break;
    }
    case pixel_test::WallpaperInitType::kPolicy:
      controller->set_bypass_decode_for_testing();

      // A dummy file path is sufficient for setting a default policy wallpaper.
      // Do not wait for resize or color calculation, as this is not a real png
      // and it will never load.
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

void AshPixelTestHelper::OnWallpaperColorsChanged() {
  DCHECK(on_wallpaper_finalized_);
  wallpaper_controller_observation_.Reset();
  std::move(on_wallpaper_finalized_).Run();
}

}  // namespace ash
