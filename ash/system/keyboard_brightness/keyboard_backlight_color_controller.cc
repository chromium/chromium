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
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

namespace {

PrefService* GetActivePrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
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
    personalization_app::mojom::BacklightColor backlight_color) {
  auto* rgb_keyboard_manager = Shell::Get()->rgb_keyboard_manager();
  DCHECK(rgb_keyboard_manager);
  DVLOG(3) << __func__ << " backlight_color=" << backlight_color;
  switch (backlight_color) {
    case personalization_app::mojom::BacklightColor::kWallpaper:
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
      break;
    }
    case personalization_app::mojom::BacklightColor::kRainbow:
      rgb_keyboard_manager->SetRainbowMode();
      break;
  }

  GetActivePrefService()->SetInteger(
      prefs::kPersonalizationKeyboardBacklightColor,
      static_cast<int>(backlight_color));
}

personalization_app::mojom::BacklightColor
KeyboardBacklightColorController::GetBacklightColor() {
  return static_cast<personalization_app::mojom::BacklightColor>(
      GetActivePrefService()->GetInteger(
          prefs::kPersonalizationKeyboardBacklightColor));
}

void KeyboardBacklightColorController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  auto* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller);
  PrefService* pref_service =
      session_controller->GetUserPrefServiceForUser(account_id);
  DCHECK(pref_service);
  auto backlight_color =
      static_cast<personalization_app::mojom::BacklightColor>(
          pref_service->GetInteger(
              prefs::kPersonalizationKeyboardBacklightColor));
  SetBacklightColor(backlight_color);
}

void KeyboardBacklightColorController::OnWallpaperColorsChanged() {
  auto* wallpaper_controller = Shell::Get()->wallpaper_controller();
  DCHECK(wallpaper_controller);
  auto backlight_color =
      static_cast<personalization_app::mojom::BacklightColor>(
          GetActivePrefService()->GetInteger(
              prefs::kPersonalizationKeyboardBacklightColor));
  if (backlight_color != personalization_app::mojom::BacklightColor::kWallpaper)
    return;
  SetBacklightColor(personalization_app::mojom::BacklightColor::kWallpaper);
}

}  // namespace ash
