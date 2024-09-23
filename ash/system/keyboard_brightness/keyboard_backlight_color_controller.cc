// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
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
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"

namespace ash {

namespace {

// Convenience alias for DisplayType enum.
using DisplayType = KeyboardBacklightColorController::DisplayType;

AccountId GetActiveAccountId() {
  return Shell::Get()->session_controller()->GetActiveAccountId();
}

PrefService* GetUserPrefService(const AccountId& account_id) {
  return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
      account_id);
}

// Determines whether to use the |kDefaultColor| instead of |color|.
bool ShouldUseDefaultColor(SkColor color) {
  if (color == kInvalidWallpaperColor) {
    return true;
  }
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(color, &hsl);
  // Determines if the color is nearly black or white.
  return hsl.l >= 0.9 || hsl.l <= 0.08;
}

}  // namespace

KeyboardBacklightColorController::KeyboardBacklightColorController(
    PrefService* local_state)
    : keyboard_backlight_color_nudge_controller_(
          std::make_unique<KeyboardBacklightColorNudgeController>()),
      local_state_(local_state) {
  Shell::Get()->rgb_keyboard_manager()->AddObserver(this);

  // local_state may be null in tests.
  if (local_state_) {
    pref_change_registrar_local_.Init(local_state_);
    pref_change_registrar_local_.Add(
        prefs::kPersonalizationKeyboardBacklightColor,
        base::BindRepeating(&KeyboardBacklightColorController::
                                OnKeyboardBacklightColorLocalStateChanged,
                            base::Unretained(this)));
  }
}

KeyboardBacklightColorController::~KeyboardBacklightColorController() {
  Shell::Get()->rgb_keyboard_manager()->RemoveObserver(this);
}

void KeyboardBacklightColorController::
    OnKeyboardBacklightColorLocalStateChanged() {
  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::LOGIN_PRIMARY) {
    DisplayBacklightColor(
        static_cast<personalization_app::mojom::BacklightColor>(
            local_state_->GetInteger(
                prefs::kPersonalizationKeyboardBacklightColor)));
  }
}

// static
void KeyboardBacklightColorController::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kPersonalizationKeyboardBacklightColor,
      static_cast<int>(personalization_app::mojom::BacklightColor::kWallpaper));
  registry->RegisterIntegerPref(
      prefs::kPersonalizationKeyboardBacklightColorDisplayType,
      static_cast<int>(DisplayType::kStatic));
  registry->RegisterDictionaryPref(
      prefs::kPersonalizationKeyboardBacklightZoneColors);
}

void KeyboardBacklightColorController::SetBacklightColor(
    personalization_app::mojom::BacklightColor backlight_color,
    const AccountId& account_id) {
  DisplayBacklightColor(backlight_color);
  SetBacklightColorPref(backlight_color, account_id);
  SetDisplayType(DisplayType::kStatic, account_id);
  if (features::IsMultiZoneRgbKeyboardEnabled()) {
    UpdateAllBacklightZoneColors(backlight_color, account_id);
  }
  MaybeToggleOnKeyboardBrightness();
}

personalization_app::mojom::BacklightColor
KeyboardBacklightColorController::GetBacklightColor(
    const AccountId& account_id) {
  // |account_id| may be empty in tests.
  if (account_id.empty()) {
    return personalization_app::mojom::BacklightColor::kWallpaper;
  }
  auto* pref_service = GetUserPrefService(account_id);
  DCHECK(pref_service);
  return static_cast<personalization_app::mojom::BacklightColor>(
      pref_service->GetInteger(prefs::kPersonalizationKeyboardBacklightColor));
}

void KeyboardBacklightColorController::SetBacklightZoneColor(
    int zone,
    personalization_app::mojom::BacklightColor backlight_color,
    const AccountId& account_id) {
  DCHECK_LT(zone, Shell::Get()->rgb_keyboard_manager()->GetZoneCount());
  SetDisplayType(DisplayType::kMultiZone, account_id);
  UpdateBacklightZoneColorPref(zone, backlight_color, account_id);
  DisplayBacklightZoneColor(zone, backlight_color);
  MaybeToggleOnKeyboardBrightness();
}

std::vector<personalization_app::mojom::BacklightColor>
KeyboardBacklightColorController::GetBacklightZoneColors(
    const AccountId& account_id) {
  auto* pref_service = GetUserPrefService(account_id);
  DCHECK(pref_service);
  auto* rgb_keyboard_manager = Shell::Get()->rgb_keyboard_manager();
  DCHECK(rgb_keyboard_manager);

  const base::Value::Dict& color_dict =
      pref_service->GetDict(prefs::kPersonalizationKeyboardBacklightZoneColors);
  const int zone_count = rgb_keyboard_manager->GetZoneCount();
  std::vector<personalization_app::mojom::BacklightColor> colors;
  colors.reserve(zone_count);
  for (int i = 0; i < zone_count; ++i) {
    auto* color_value = color_dict.Find(base::StringPrintf("zone-%d", i));
    DCHECK(color_value);
    auto zone_color = static_cast<personalization_app::mojom::BacklightColor>(
        color_value->GetInt());
    colors.push_back(zone_color);
  }
  return colors;
}

void KeyboardBacklightColorController::SetDisplayType(
    DisplayType type,
    const AccountId& account_id) {
  DCHECK(features::IsMultiZoneRgbKeyboardEnabled());
  GetUserPrefService(account_id)
      ->SetInteger(prefs::kPersonalizationKeyboardBacklightColorDisplayType,
                   static_cast<int>(type));
}

DisplayType KeyboardBacklightColorController::GetDisplayType(
    const AccountId& account_id) {
  // |account_id| may be empty in tests or login screen.
  if (account_id.empty()) {
    return DisplayType::kStatic;
  }
  auto* pref_service = GetUserPrefService(account_id);
  DCHECK(pref_service);
  return static_cast<DisplayType>(pref_service->GetInteger(
      prefs::kPersonalizationKeyboardBacklightColorDisplayType));
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
      // initialized to match the wallpaper.
      OnWallpaperColorsChanged();
    }
    if (Shell::Get()->session_controller()->GetSessionState() ==
        session_manager::SessionState::LOGIN_PRIMARY) {
      DisplayBacklightColor(
          static_cast<personalization_app::mojom::BacklightColor>(
              local_state_->GetInteger(
                  prefs::kPersonalizationKeyboardBacklightColor)));
    }
  } else {
    session_observer_.Reset();
    wallpaper_controller_observation_.Reset();
  }
}

void KeyboardBacklightColorController::OnSessionStateChanged(
    session_manager::SessionState state) {
  // If we are in OOBE, we should set the backlight to a default of white.
  if (state != session_manager::SessionState::OOBE) {
    return;
  }
  DisplayBacklightColor(personalization_app::mojom::BacklightColor::kWhite);
}

void KeyboardBacklightColorController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  const AccountId account_id = GetActiveAccountId();
  const auto display_type = GetDisplayType(account_id);
  switch (display_type) {
    case DisplayType::kStatic: {
      const auto backlight_color = GetBacklightColor(account_id);
      if (features::IsMultiZoneRgbKeyboardEnabled()) {
        // Defaults the zone color to be the currently set backlight color.
        UpdateAllBacklightZoneColors(backlight_color, account_id);
      }
      switch (backlight_color) {
        case personalization_app::mojom::BacklightColor::kWallpaper: {
          // Displaying the wallpaper color is handled by
          // |OnWallpaperColorsChanged()|.
          return;
        }
        case personalization_app::mojom::BacklightColor::kWhite:
        case personalization_app::mojom::BacklightColor::kRed:
        case personalization_app::mojom::BacklightColor::kYellow:
        case personalization_app::mojom::BacklightColor::kGreen:
        case personalization_app::mojom::BacklightColor::kBlue:
        case personalization_app::mojom::BacklightColor::kIndigo:
        case personalization_app::mojom::BacklightColor::kPurple:
        case personalization_app::mojom::BacklightColor::kRainbow: {
          DisplayBacklightColor(backlight_color);
          return;
        }
      }
      return;
    }
    case DisplayType::kMultiZone: {
      const std::vector<personalization_app::mojom::BacklightColor>
          zone_colors = GetBacklightZoneColors(account_id);
      for (size_t zone = 0; zone < zone_colors.size(); ++zone) {
        DisplayBacklightZoneColor(zone, zone_colors.at(zone));
      }
      return;
    }
  }
}

void KeyboardBacklightColorController::OnWallpaperColorsChanged() {
  const AccountId account_id = GetActiveAccountId();
  const auto display_type = GetDisplayType(account_id);
  switch (display_type) {
    case DisplayType::kStatic: {
      const auto backlight_color = GetBacklightColor(account_id);
      switch (backlight_color) {
        case personalization_app::mojom::BacklightColor::kWallpaper: {
          DisplayBacklightColor(backlight_color);
          return;
        }
        case personalization_app::mojom::BacklightColor::kWhite:
        case personalization_app::mojom::BacklightColor::kRed:
        case personalization_app::mojom::BacklightColor::kYellow:
        case personalization_app::mojom::BacklightColor::kGreen:
        case personalization_app::mojom::BacklightColor::kBlue:
        case personalization_app::mojom::BacklightColor::kIndigo:
        case personalization_app::mojom::BacklightColor::kPurple:
        case personalization_app::mojom::BacklightColor::kRainbow: {
          return;
        }
      }
    }
    case DisplayType::kMultiZone: {
      const std::vector<personalization_app::mojom::BacklightColor>
          zone_colors = GetBacklightZoneColors(account_id);
      for (size_t zone = 0; zone < zone_colors.size(); ++zone) {
        DisplayBacklightZoneColor(zone, zone_colors.at(zone));
      }
      return;
    }
  }
}

void KeyboardBacklightColorController::DisplayBacklightColor(
    personalization_app::mojom::BacklightColor backlight_color) {
  auto* rgb_keyboard_manager = Shell::Get()->rgb_keyboard_manager();
  DCHECK(rgb_keyboard_manager);
  DVLOG(3) << __func__ << " backlight_color=" << backlight_color;
  switch (backlight_color) {
    case personalization_app::mojom::BacklightColor::kWallpaper: {
      SkColor wallpaper_color = GetCurrentWallpaperColor();
      rgb_keyboard_manager->SetStaticBackgroundColor(
          SkColorGetR(wallpaper_color), SkColorGetG(wallpaper_color),
          SkColorGetB(wallpaper_color));
      displayed_color_for_testing_ = wallpaper_color;
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

void KeyboardBacklightColorController::DisplayBacklightZoneColor(
    int zone,
    personalization_app::mojom::BacklightColor backlight_color) {
  auto* rgb_keyboard_manager = Shell::Get()->rgb_keyboard_manager();
  DCHECK(rgb_keyboard_manager);
  DVLOG(3) << __func__ << " zone=" << zone
           << " backlight_color=" << backlight_color;
  switch (backlight_color) {
    case personalization_app::mojom::BacklightColor::kWallpaper: {
      SkColor wallpaper_color = GetCurrentWallpaperColor();
      rgb_keyboard_manager->SetZoneColor(zone, SkColorGetR(wallpaper_color),
                                         SkColorGetG(wallpaper_color),
                                         SkColorGetB(wallpaper_color));
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
      rgb_keyboard_manager->SetZoneColor(
          zone, SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
      break;
    }
    case personalization_app::mojom::BacklightColor::kRainbow: {
      NOTREACHED() << " Attempted to display an invalid color option";
    }
  }
}

void KeyboardBacklightColorController::SetBacklightColorPref(
    personalization_app::mojom::BacklightColor backlight_color,
    const AccountId& account_id) {
  GetUserPrefService(account_id)
      ->SetInteger(prefs::kPersonalizationKeyboardBacklightColor,
                   static_cast<int>(backlight_color));
}

void KeyboardBacklightColorController::UpdateAllBacklightZoneColors(
    personalization_app::mojom::BacklightColor backlight_color,
    const AccountId& account_id) {
  auto* rgb_keyboard_manager = Shell::Get()->rgb_keyboard_manager();
  DCHECK(rgb_keyboard_manager);
  const int zone_count = rgb_keyboard_manager->GetZoneCount();
  for (int zone = 0; zone < zone_count; ++zone) {
    UpdateBacklightZoneColorPref(zone, backlight_color, account_id);
  }
}

void KeyboardBacklightColorController::UpdateBacklightZoneColorPref(
    int zone,
    personalization_app::mojom::BacklightColor backlight_color,
    const AccountId& account_id) {
  ScopedDictPrefUpdate color_dict(
      GetUserPrefService(account_id),
      prefs::kPersonalizationKeyboardBacklightZoneColors);
  color_dict->Set(base::StringPrintf("zone-%d", zone),
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
    std::optional<double> percentage) {
  if (!percentage.has_value() || percentage.value() == 0.0) {
    DVLOG(1) << __func__ << " Toggling on the keyboard brightness.";
    power_manager::SetBacklightBrightnessRequest request;
    request.set_percent(kDefaultBacklightBrightness);
    request.set_transition(
        power_manager::SetBacklightBrightnessRequest_Transition_FAST);
    request.set_cause(
        power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
    chromeos::PowerManagerClient::Get()->SetKeyboardBrightness(
        std::move(request));
  }
}

SkColor KeyboardBacklightColorController::GetCurrentWallpaperColor() {
  const auto* wallpaper_controller = Shell::Get()->wallpaper_controller();
  DCHECK(wallpaper_controller);
  SkColor color = kDefaultColor;
  bool missing_wallpaper_color =
      !wallpaper_controller->calculated_colors().has_value();
  if (!missing_wallpaper_color) {
    color = ConvertBacklightColorToSkColor(
        personalization_app::mojom::BacklightColor::kWallpaper);
    bool invalid_color = color == kInvalidWallpaperColor;
    base::UmaHistogramBoolean(
        "Ash.Personalization.KeyboardBacklight.WallpaperColor.Valid2",
        !invalid_color);
  }
  if (ShouldUseDefaultColor(color)) {
    color = kDefaultColor;
  }
  return color;
}

}  // namespace ash
