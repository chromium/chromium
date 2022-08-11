// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/rgb_keyboard/rgb_keyboard_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_nudge_controller.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

namespace {

AccountId GetActiveAccountId() {
  return Shell::Get()->session_controller()->GetActiveAccountId();
}

PrefService* GetUserPrefService(const AccountId& account_id) {
  return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
      account_id);
}

// Determines whether to use the |kDefaultColor| instead of |color|.
bool ShouldUseDefaultColor(SkColor color) {
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(color, &hsl);
  // Determines if the color is nearly black or white.
  return hsl.l >= 0.9 || hsl.l <= 0.08;
}

}  // namespace

KeyboardBacklightColorController::KeyboardBacklightColorController()
    : keyboard_backlight_color_nudge_controller_(
          std::make_unique<KeyboardBacklightColorNudgeController>()) {
  wallpaper_controller_observation_.Observe(WallpaperController::Get());
}

KeyboardBacklightColorController::~KeyboardBacklightColorController() = default;

// static
void KeyboardBacklightColorController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kPersonalizationKeyboardBacklightColor,
      static_cast<int>(personalization_app::mojom::BacklightColor::kWallpaper));
}

void KeyboardBacklightColorController::SetBacklightColor(
    personalization_app::mojom::BacklightColor backlight_color,
    const AccountId& account_id) {
  DisplayBacklightColor(backlight_color);
  SetBacklightColorPref(backlight_color, account_id);
}

personalization_app::mojom::BacklightColor
KeyboardBacklightColorController::GetBacklightColor(
    const AccountId& account_id) {
  // |account_id| may be empty in tests.
  if (account_id.empty())
    return personalization_app::mojom::BacklightColor::kWallpaper;
  auto* pref_service = GetUserPrefService(account_id);
  if (!pref_service) {
    // TODO(b/238463679): Migrate to local state pref. There may be a timing
    // issue that results in null pref service. Defaults to |kWallpaper| when
    // that happens.
    return personalization_app::mojom::BacklightColor::kWallpaper;
  }
  return static_cast<personalization_app::mojom::BacklightColor>(
      pref_service->GetInteger(prefs::kPersonalizationKeyboardBacklightColor));
}

void KeyboardBacklightColorController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  const auto backlight_color = GetBacklightColor(GetActiveAccountId());
  DisplayBacklightColor(backlight_color);
}

void KeyboardBacklightColorController::OnUserSessionUpdated(
    const AccountId& account_id) {
  const auto backlight_color = GetBacklightColor(account_id);
  DisplayBacklightColor(backlight_color);
}

void KeyboardBacklightColorController::OnWallpaperColorsChanged() {
  const auto backlight_color = GetBacklightColor(GetActiveAccountId());
  if (backlight_color != personalization_app::mojom::BacklightColor::kWallpaper)
    return;
  DisplayBacklightColor(personalization_app::mojom::BacklightColor::kWallpaper);
}

void KeyboardBacklightColorController::DisplayBacklightColor(
    personalization_app::mojom::BacklightColor backlight_color) {
  auto* rgb_keyboard_manager = Shell::Get()->rgb_keyboard_manager();
  DCHECK(rgb_keyboard_manager);
  DVLOG(3) << __func__ << " backlight_color=" << backlight_color;
  switch (backlight_color) {
    case personalization_app::mojom::BacklightColor::kWallpaper: {
      SkColor color = ConvertBacklightColorToSkColor(backlight_color);
      bool valid_color = color != kInvalidWallpaperColor;
      base::UmaHistogramBoolean(
          "Ash.Personalization.KeyboardBacklight.WallpaperColor.Valid",
          valid_color);
      // Default to |kDefaultColor| if |color| is invalid or
      // |ShouldUseDefaultColor| is true.
      if (!valid_color || ShouldUseDefaultColor(color)) {
        color = kDefaultColor;
      }
      rgb_keyboard_manager->SetStaticBackgroundColor(
          SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
      displayed_color_for_testing_ = color;
      break;
    }
    case personalization_app::mojom::BacklightColor::kWhite:
    case personalization_app::mojom::BacklightColor::kRed:
    case personalization_app::mojom::BacklightColor::kYellow:
    case personalization_app::mojom::BacklightColor::kGreen:
    case personalization_app::mojom::BacklightColor::kBlue:
    case personalization_app::mojom::BacklightColor::kIndigo:
    case personalization_app::mojom::BacklightColor::kPurple: {
      SkColor color = ConvertBacklightColorToSkColor(backlight_color);
      rgb_keyboard_manager->SetStaticBackgroundColor(
          SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
      displayed_color_for_testing_ = color;
      break;
    }
    case personalization_app::mojom::BacklightColor::kRainbow:
      rgb_keyboard_manager->SetRainbowMode();
      break;
  }
}

void KeyboardBacklightColorController::SetBacklightColorPref(
    personalization_app::mojom::BacklightColor backlight_color,
    const AccountId& account_id) {
  GetUserPrefService(account_id)
      ->SetInteger(prefs::kPersonalizationKeyboardBacklightColor,
                   static_cast<int>(backlight_color));
}

}  // namespace ash
