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
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/dynamic_color/palette.h"
#include "ui/color/dynamic_color/palette_factory.h"
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

using SchemeVariant = ui::ColorProviderManager::SchemeVariant;

SchemeVariant ToVariant(ColorScheme scheme) {
  switch (scheme) {
    case ColorScheme::kStatic:
    case ColorScheme::kNeutral:
      return SchemeVariant::kNeutral;
    case ColorScheme::kTonalSpot:
      return SchemeVariant::kTonalSpot;
    case ColorScheme::kExpressive:
      return SchemeVariant::kExpressive;
    case ColorScheme::kVibrant:
      return SchemeVariant::kVibrant;
  }
}

SampleColorScheme GenerateSampleColorScheme(bool dark,
                                            SkColor seed_color,
                                            ColorScheme scheme) {
  DCHECK_NE(scheme, ColorScheme::kStatic)
      << "Requesting a static scheme doesn't make sense since there is no "
         "seed color";

  std::unique_ptr<ui::Palette> palette =
      ui::GeneratePalette(seed_color, ToVariant(scheme));
  SampleColorScheme sample;
  sample.scheme = scheme;
  // Tertiary is cros.ref.teratiary-70 for all color schemes.
  sample.tertiary = palette->tertiary().get(70.f);  // tertiary 70

  if (scheme == ColorScheme::kVibrant) {
    // Vibrant uses cros.ref.primary-70 and cros.ref.primary-50.
    sample.primary = palette->primary().get(70.f);    // primary 70
    sample.secondary = palette->primary().get(50.f);  // primary 50
  } else {
    // All other schemes use cros.ref.primary-80 and cros.ref.primary-60.
    sample.primary = palette->primary().get(80.f);    // primary 80
    sample.secondary = palette->primary().get(60.f);  // primary 60
  }

  return sample;
}

std::vector<SampleColorScheme> GenerateSamples(
    bool dark,
    SkColor sample_color,
    const std::vector<const ColorScheme>& schemes) {
  std::vector<SampleColorScheme> samples;
  for (auto scheme : schemes) {
    samples.push_back(GenerateSampleColorScheme(dark, sample_color, scheme));
  }

  return samples;
}

// Refresh colors of the system on the current color mode. Not only the SysUI,
// but also all the other components like WebUI. This will trigger
// View::OnThemeChanged to live update the colors. The colors live update can
// happen when color mode changes or wallpaper changes. It is needed when
// wallpaper changes as the background color is calculated from current
// wallpaper.
void RefreshNativeTheme(const ColorPaletteSeed& seed) {
  const SkColor themed_color = seed.seed_color;
  bool is_dark_mode_enabled = seed.color_mode == ColorMode::kDark;
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  native_theme->set_use_dark_colors(is_dark_mode_enabled);
  native_theme->set_user_color(themed_color);
  native_theme->set_scheme_variant(ToVariant(seed.scheme));
  native_theme->NotifyOnNativeThemeUpdated();

  auto* native_theme_web = ui::NativeTheme::GetInstanceForWeb();
  if (!native_theme_web->IsForcedDarkMode()) {
    native_theme_web->set_use_dark_colors(is_dark_mode_enabled);
    native_theme_web->set_preferred_color_scheme(
        is_dark_mode_enabled ? ui::NativeTheme::PreferredColorScheme::kDark
                             : ui::NativeTheme::PreferredColorScheme::kLight);
  }
  native_theme_web->set_scheme_variant(ToVariant(seed.scheme));
  native_theme_web->set_user_color(themed_color);
  native_theme_web->NotifyOnNativeThemeUpdated();
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
    dark_light_observation_.Observe(dark_light_mode_controller);
    wallpaper_observation_.Observe(wallpaper_controller);
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
    NotifyObservers(GetColorPaletteSeed(account_id));
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
    NotifyObservers(GetColorPaletteSeed(account_id));
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(on_complete), base::Milliseconds(100));
  }

  absl::optional<ColorPaletteSeed> GetColorPaletteSeed(
      const AccountId& account_id) const override {
    ColorPaletteSeed seed;
    bool dark = dark_light_mode_controller_->IsDarkModeEnabled();
    absl::optional<SkColor> seed_color = UsesWallpaperSeedColor(account_id)
                                             ? CurrentWallpaperColor(dark)
                                             : GetStaticSeedColor(account_id);
    if (!seed_color) {
      return {};
    }

    seed.color_mode = dark ? ui::ColorProviderManager::ColorMode::kDark
                           : ui::ColorProviderManager::ColorMode::kLight;
    seed.seed_color = *seed_color;
    seed.scheme = GetColorScheme(account_id);

    return seed;
  }

  absl::optional<ColorPaletteSeed> GetCurrentSeed() const override {
    const auto* session = GetActiveUserSession();
    if (!session) {
      return {};
    }

    return GetColorPaletteSeed(AccountFromSession(session));
  }

  bool UsesWallpaperSeedColor(const AccountId& account_id) const override {
    // Scheme tracks if wallpaper color is used.
    return GetColorScheme(account_id) != ColorScheme::kStatic;
  }

  ColorScheme GetColorScheme(const AccountId& account_id) const override {
    if (!chromeos::features::IsJellyEnabled()) {
      // Pre-Jelly, this is always Tonal Spot.
      return ColorScheme::kTonalSpot;
    }
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
    bool dark = dark_light_mode_controller_->IsDarkModeEnabled();
    absl::optional<SkColor> seed_color = CurrentWallpaperColor(dark);
    if (!seed_color) {
      LOG(WARNING) << "Using default color due to missing wallpaper sample";
      seed_color.emplace(gfx::kGoogleBlue400);
    }
    // Schemes need to be copied as the underlying memory for the span could go
    // out of scope.
    std::vector<const ColorScheme> schemes_copy(color_scheme_buttons.begin(),
                                                color_scheme_buttons.end());
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&GenerateSamples, dark, *seed_color, schemes_copy),
        base::BindOnce(std::move(callback)));
  }

  // WallpaperControllerObserver overrides:
  void OnWallpaperColorsChanged() override {
    NotifyObservers(BestEffortSeed(GetActiveUserSession()));
  }

  // ColorModeObserver overrides:
  void OnColorModeChanged(bool) override {
    NotifyObservers(BestEffortSeed(GetActiveUserSession()));
  }

 private:
  absl::optional<SkColor> CurrentWallpaperColor(bool dark) const {
    if (!chromeos::features::IsJellyEnabled()) {
      return GetWallpaperColor(dark);
    }

    const absl::optional<WallpaperCalculatedColors>& calculated_colors =
        wallpaper_controller_->calculated_colors();
    if (!calculated_colors) {
      return {};
    }

    return calculated_colors->celebi_color;
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

  // Returns the seed for `session` if it's present.  Otherwise, returns a seed
  // for backward compatibility with just dark/light and seed color filled.
  absl::optional<ColorPaletteSeed> BestEffortSeed(const UserSession* session) {
    if (session) {
      return GetColorPaletteSeed(AccountFromSession(session));
    }

    // Generate a seed where we assume TonalSpot and ignore static colors.
    // TODO(b/276475812): When static color and color scheme are in local state,
    // only run this if Jelly is not enabled.
    ColorPaletteSeed seed;
    bool dark = dark_light_mode_controller_->IsDarkModeEnabled();
    absl::optional<SkColor> seed_color = CurrentWallpaperColor(dark);
    if (!seed_color) {
      // If `seed_color` is not available, we expect to have it shortly when the
      // color computation is done and this will be called again.
      return {};
    }
    seed.color_mode = dark ? ui::ColorProviderManager::ColorMode::kDark
                           : ui::ColorProviderManager::ColorMode::kLight;
    seed.seed_color = *seed_color;
    seed.scheme = ColorScheme::kTonalSpot;

    return seed;
  }

  void NotifyObservers(const absl::optional<ColorPaletteSeed>& seed) {
    if (!seed) {
      // If the seed wasn't valid, skip notifications.
      return;
    }

    for (auto& observer : observers_) {
      observer.OnColorPaletteChanging(*seed);
    }

    RefreshNativeTheme(*seed);
  }

  base::ScopedObservation<DarkLightModeController, ColorModeObserver>
      dark_light_observation_{this};

  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_observation_{this};

  raw_ptr<WallpaperControllerImpl> wallpaper_controller_;  // unowned

  raw_ptr<DarkLightModeController> dark_light_mode_controller_;  // unowned

  base::ObserverList<ColorPaletteController::Observer> observers_;
};

}  // namespace

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
