// Copyright 2022 The Chromium Authors
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
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  Shell::Get()->rgb_keyboard_manager()->AddObserver(this);
}

KeyboardBacklightColorController::~KeyboardBacklightColorController() {
  Shell::Get()->rgb_keyboard_manager()->RemoveObserver(this);
}

// static
void KeyboardBacklightColorController::RegisterPrefs(
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
  MaybeToggleOnKeyboardBrightness();
}

personalization_app::mojom::BacklightColor
KeyboardBacklightColorController::GetBacklightColor(
    const AccountId& account_id) {
  // |account_id| may be empty in tests.
  if (account_id.empty())
    return personalization_app::mojom::BacklightColor::kWallpaper;
  auto* pref_service = GetUserPrefService(account_id);
  DCHECK(pref_service);
  return static_cast<personalization_app::mojom::BacklightColor>(
      pref_service->GetInteger(prefs::kPersonalizationKeyboardBacklightColor));
}

void KeyboardBacklightColorController::OnRgbKeyboardSupportedChanged(
    bool supported) {
  if (supported) {
    if (!session_observer_.IsObserving()) {
      auto* session_controller = Shell::Get()->session_controller();
      DCHECK(session_controller);

      session_observer_.Observe(session_controller);

      // Since |session_observer_| does not start observing until after Chrome
      // is initially started, the rgb keyboard needs to be initiallized based
      // on state from the |SessionController|.
      OnSessionStateChanged(session_controller->GetSessionState());
      if (session_controller->IsActiveUserSessionStarted()) {
        OnActiveUserPrefServiceChanged(
            session_controller->GetActivePrefService());
      }
    }
    if (!wallpaper_controller_observation_.IsObserving()) {
      auto* wallpaper_controller = Shell::Get()->wallpaper_controller();
      DCHECK(wallpaper_controller);

      wallpaper_controller_observation_.Observe(wallpaper_controller);

      // Since |wallpaper_controller_observation_| does not start observering
      // until after Chrome is initially started, the rgb keyboard needs to be
      // initialized to match the wallpaper if the colors have been calculated
      // before.
      if (wallpaper_controller->GetKMeanColor() != kInvalidWallpaperColor) {
        OnWallpaperColorsChanged();
      }
    }
  } else {
    session_observer_.Reset();
    wallpaper_controller_observation_.Reset();
  }
}

void KeyboardBacklightColorController::OnSessionStateChanged(
    session_manager::SessionState state) {
  // If we are in OOBE, we should set the backlight to a default of white.
  if (state != session_manager::SessionState::OOBE)
    return;
  DisplayBacklightColor(personalization_app::mojom::BacklightColor::kWhite);
}

void KeyboardBacklightColorController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  const auto backlight_color = GetBacklightColor(GetActiveAccountId());
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

void KeyboardBacklightColorController::MaybeToggleOnKeyboardBrightness() {
  DVLOG(1) << __func__ << " getting keyboard brightness";
  chromeos::PowerManagerClient::Get()->GetKeyboardBrightnessPercent(
      base::BindOnce(
          &KeyboardBacklightColorController::KeyboardBrightnessPercentReceived,
          weak_ptr_factory_.GetWeakPtr()));
}

void KeyboardBacklightColorController::KeyboardBrightnessPercentReceived(
    absl::optional<double> percentage) {
  if (!percentage.has_value() || percentage.value() == 0.0) {
    DVLOG(1) << __func__ << " Toggling on the keyboard brightness.";
    // TODO(b/244139677): Calls API to turn on the keyboard brightness.
    keyboard_brightness_on_for_testing_ = true;
  }
}

}  // namespace ash
