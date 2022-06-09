// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_pixel_diff_test_base.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/power/power_status.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/command_line.h"
#include "base/time/time_override.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display.h"

namespace ash {

namespace {

// The fake user account.
constexpr char kUser[] = "user1@test.com";

// The fake file ids for wallpaper setting.
constexpr char kFakeFileId[] = "file-hash";
constexpr char kWallpaperFileName[] = "test-file";

constexpr SkColor kWallPaperColor = SK_ColorMAGENTA;

constexpr char kLocale[] = "en_US";
constexpr char kTimeZone[] = "America/Chicago";

// The string that represents the current time.
constexpr char kFakeNowTimeString[] = "Sun, 6 May 2018 14:30:00 CDT";

// Creates a pure color image of the specified size.
gfx::ImageSkia CreateImage(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
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

AshPixelDiffTestBase::AshPixelDiffTestBase(
    std::unique_ptr<base::test::TaskEnvironment> task_environment)
    : AshTestBase(std::move(task_environment)),
      kAccountId_(AccountId::FromUserEmailGaiaId(kUser, "test-hash")),
      scoped_locale_(kLocale),
      time_zone_(kTimeZone) {}

AshPixelDiffTestBase::~AshPixelDiffTestBase() = default;

bool AshPixelDiffTestBase::ComparePrimaryFullScreen(
    const std::string& screenshot_name) {
  aura::Window* primary_root_window = Shell::Get()->GetPrimaryRootWindow();
  return pixel_diff_.CompareNativeWindowScreenshot(
      screenshot_name, primary_root_window,
      gfx::Rect(primary_root_window->bounds().size()));
}

void AshPixelDiffTestBase::SetUp() {
  // In ash_pixeltests, we want to take screenshots then compare them with the
  // benchmark images. Therefore, enable pixel output in tests.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnablePixelOutputInTests);

  // Override the current time before setting up `AshTestBase` so that the views
  // relying on the current time, like the tray time view, show as expected.
  OverrideTime();

  // Do not start the user session in `AshTestBase::SetUp()`. Instead, perform
  // user login with `kAccountId_`.
  set_start_session(false);

  AshTestBase::SetUp();

  SimulateUserLogin(kAccountId_);

  // If the dark/light mode feature is enabled, ensure to use the dark mode.
  if (features::IsDarkLightModeEnabled()) {
    auto* color_provider = AshColorProvider::Get();
    if (!color_provider->IsDarkModeEnabled())
      color_provider->ToggleColorMode();
  }

  // Set variable UI components in explicit ways to stabilize screenshots.
  SetWallPaper();
  SetBatteryState();
}

void AshPixelDiffTestBase::SetWallPaper() {
  ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(online_wallpaper_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(custom_wallpaper_dir_.CreateUniqueTempDir());

  auto* controller = Shell::Get()->wallpaper_controller();
  controller->Init(user_data_dir_.GetPath(), online_wallpaper_dir_.GetPath(),
                   custom_wallpaper_dir_.GetPath(),
                   /*policy_wallpaper=*/base::FilePath());
  controller->set_wallpaper_reload_no_delay_for_test();
  controller->SetClient(&client_);
  client_.set_fake_files_id_for_account_id(kAccountId_, kFakeFileId);

  const gfx::Size display_size = GetPrimaryDisplay().size();
  gfx::ImageSkia wallpaper_image =
      CreateImage(display_size.width(), display_size.height(), kWallPaperColor);
  controller->SetCustomWallpaper(kAccountId_, kWallpaperFileName,
                                 WALLPAPER_LAYOUT_STRETCH, wallpaper_image,
                                 /*preview_mode=*/false);
}

void AshPixelDiffTestBase::OverrideTime() {
  ASSERT_TRUE(base::Time::FromString(kFakeNowTimeString,
                                     &TimeOverrideHelper::current_time));
  time_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
      &TimeOverrideHelper::TimeNow, /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);
}

void AshPixelDiffTestBase::SetBatteryState() {
  power_manager::PowerSupplyProperties proto;
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  proto.set_battery_percent(50.0);
  chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(proto);
}

}  // namespace ash
