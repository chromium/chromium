// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_palette_controller.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/color_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/color_palette.h"

namespace ash {

namespace {

class ColorPaletteControllerImpl;

using ColorMode = ui::ColorProviderManager::ColorMode;

// Returns the wallpaper colors for pre-Jelly.  Called for both dark and light.
SkColor GetWallpaperColor(bool is_dark_mode_enabled) {
  const SkColor default_color =
      is_dark_mode_enabled ? gfx::kGoogleGrey900 : SK_ColorWHITE;
  return ColorUtil::GetBackgroundThemedColor(default_color,
                                             is_dark_mode_enabled);
}

PrefService* GetUserPrefService(const AccountId& account_id) {
  DCHECK(account_id.is_valid());
  return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
      account_id);
}

// Returns the currently active user session (at index 0).
const UserSession* GetActiveUserSession() {
  return Shell::Get()->session_controller()->GetUserSession(/*index=*/0);
}

const AccountId& AccountFromSession(const UserSession* session) {
  CHECK(session);
  return session->user_info.account_id;
}

// TODO(b/258719005): Finish implementation with code that works/uses libmonet.
class ColorPaletteControllerImpl : public ColorPaletteController,
                                   public WallpaperControllerObserver,
                                   public ColorModeObserver {
 public:
  ColorPaletteControllerImpl(
      DarkLightModeController* dark_light_mode_controller,
      WallpaperControllerImpl* wallpaper_controller)
      : wallpaper_controller_(wallpaper_controller),
        dark_light_mode_controller_(dark_light_mode_controller) {
    wallpaper_observation_.Observe(wallpaper_controller);
    wallpaper_color_[ColorMode::kDark] = SK_ColorTRANSPARENT;
    wallpaper_color_[ColorMode::kLight] = SK_ColorTRANSPARENT;
  }

  ~ColorPaletteControllerImpl() override = default;

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void SetColorScheme(ColorScheme scheme,
                      const AccountId& account_id,
                      base::OnceClosure on_complete) override {
    DVLOG(1) << "Setting color scheme to: " << (int)scheme;
    PrefService* pref_service = GetUserPrefService(account_id);
    if (!pref_service) {
      DVLOG(1) << "No user pref service available.";
      return;
    }
    pref_service->SetInteger(prefs::kDynamicColorColorScheme,
                             static_cast<int>(scheme));
    // TODO(b/258719005): Call this after the native theme change has been
    // applied.
    NotifyObservers(account_id);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(on_complete), base::Milliseconds(100));
  }

  void SetStaticColor(SkColor seed_color,
                      const AccountId& account_id,
                      base::OnceClosure on_complete) override {
    DVLOG(1) << "Static color scheme: " << (int)seed_color;
    PrefService* pref_service = GetUserPrefService(account_id);
    if (!pref_service) {
      DVLOG(1) << "No user pref service available.";
      return;
    }
    // Set the color scheme before the seed color because there is a check in
    // |GetStaticColor| to only return a color if the color scheme is kStatic.
    pref_service->SetInteger(prefs::kDynamicColorColorScheme,
                             static_cast<int>(ColorScheme::kStatic));
    pref_service->SetUint64(prefs::kDynamicColorSeedColor, seed_color);
    // TODO(b/258719005): Call this after the native theme change has been
    // applied.
    NotifyObservers(account_id);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(on_complete), base::Milliseconds(100));
  }

  ColorPaletteSeed GetColorPaletteSeed(
      const AccountId& account_id) const override {
    ColorPaletteSeed seed;
    seed.color_mode = dark_light_mode_controller_->IsDarkModeEnabled()
                          ? ui::ColorProviderManager::ColorMode::kDark
                          : ui::ColorProviderManager::ColorMode::kLight;
    seed.seed_color = UsesWallpaperSeedColor(account_id)
                          ? wallpaper_color_.at(seed.color_mode)
                          : GetStaticSeedColor(account_id);
    seed.scheme = GetColorScheme(account_id);

    return seed;
  }

  bool UsesWallpaperSeedColor(const AccountId& account_id) const override {
    // Scheme tracks if wallpaper color is used.
    return GetColorScheme(account_id) != ColorScheme::kStatic;
  }

  ColorScheme GetColorScheme(const AccountId& account_id) const override {
    PrefService* pref_service = GetUserPrefService(account_id);
    if (!pref_service) {
      DVLOG(1)
          << "No user pref service available. Returning default color scheme.";
      return ColorScheme::kTonalSpot;
    }
    return static_cast<ColorScheme>(
        pref_service->GetInteger(prefs::kDynamicColorColorScheme));
  }

  absl::optional<SkColor> GetStaticColor(
      const AccountId& account_id) const override {
    PrefService* pref_service = GetUserPrefService(account_id);
    if (!pref_service) {
      DVLOG(1) << "No user pref service available.";
      return absl::nullopt;
    }
    if (GetColorScheme(account_id) == ColorScheme::kStatic) {
      return GetStaticSeedColor(account_id);
    }

    return absl::nullopt;
  }

  void GenerateSampleColorSchemes(
      base::span<const ColorScheme> color_scheme_buttons,
      SampleColorSchemeCallback callback) const override {
    std::vector<SampleColorScheme> samples;
    for (auto scheme : color_scheme_buttons) {
      samples.push_back(GenerateSampleColorScheme(scheme));
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(callback), samples),
        base::Milliseconds(20));
  }

  // WallpaperControllerObserver overrides:
  void OnWallpaperColorsChanged() override {
    if (!chromeos::features::IsJellyEnabled()) {
      SkColor dark_color = GetWallpaperColor(true);
      SkColor light_color = GetWallpaperColor(false);
      SetWallpaperColor(dark_color, light_color);
      return;
    }

    SkColor wallpaper_color =
        wallpaper_controller_->calculated_colors()->celebi_color;
    // When Jelly is enabled, light/dark changes are handled in palette
    // generation.  So it's the same color.
    SetWallpaperColor(wallpaper_color, wallpaper_color);
  }

  // ColorModeObserver overrides:
  void OnColorModeChanged(bool) override {
    // Change colors and notify.
    auto* session = GetActiveUserSession();
    if (session) {
      NotifyObservers(AccountFromSession(session));
    }
  }

 private:
  void SetWallpaperColor(SkColor dark_color, SkColor light_color) {
    wallpaper_color_[ColorMode::kDark] = dark_color;
    wallpaper_color_[ColorMode::kLight] = light_color;
    // TODO(b/258719005): Update Native Theme
    auto* session = GetActiveUserSession();
    if (session) {
      NotifyObservers(AccountFromSession(session));
    }
  }

  SkColor GetStaticSeedColor(const AccountId& account_id) const {
    PrefService* pref_service = GetUserPrefService(account_id);
    if (!pref_service) {
      DVLOG(1) << "No user pref service available. Returning default color "
                  "palette seed.";
      return SK_ColorBLUE;
    }
    return static_cast<SkColor>(
        pref_service->GetUint64(prefs::kDynamicColorSeedColor));
  }

  SampleColorScheme GenerateSampleColorScheme(ColorScheme scheme) const {
    // TODO(b/258719005): Return correct and different schemes for each
    // `scheme`.
    DCHECK_NE(scheme, ColorScheme::kStatic)
        << "Requesting a static scheme doesn't make sense since there is no "
           "seed color";
    return {.scheme = scheme,
            .primary = SK_ColorRED,
            .secondary = SK_ColorGREEN,
            .tertiary = SK_ColorBLUE};
  }

  void NotifyObservers(const AccountId& account_id) {
    ColorPaletteSeed seed = GetColorPaletteSeed(account_id);
    for (auto& observer : observers_) {
      observer.OnColorPaletteChanging(seed);
    }
  }

  base::flat_map<ui::ColorProviderManager::ColorMode, SkColor> wallpaper_color_;

  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_observation_{this};

  base::raw_ptr<WallpaperControllerImpl> wallpaper_controller_;  // unowned

  base::raw_ptr<DarkLightModeController>
      dark_light_mode_controller_;  // unowned

  base::ObserverList<ColorPaletteController::Observer> observers_;
};

}  // namespace

// static
std::unique_ptr<ColorPaletteController> ColorPaletteController::Create() {
  Shell* shell = Shell::Get();
  return Create(shell->dark_light_mode_controller(),
                shell->wallpaper_controller());
}

// static
std::unique_ptr<ColorPaletteController> ColorPaletteController::Create(
    DarkLightModeController* dark_light_mode_controller,
    WallpaperControllerImpl* wallpaper_controller) {
  return std::make_unique<ColorPaletteControllerImpl>(
      dark_light_mode_controller, wallpaper_controller);
}

// static
void ColorPaletteController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDynamicColorColorScheme,
      static_cast<int>(ColorScheme::kTonalSpot),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterUint64Pref(
      prefs::kDynamicColorSeedColor, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

}  // namespace ash
