// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_palette_controller.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/color_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/dynamic_color/palette.h"
#include "ui/color/dynamic_color/palette_factory.h"
#include "ui/gfx/color_palette.h"

namespace ash {

namespace {

class ColorPaletteControllerImpl;

using ColorMode = ui::ColorProviderManager::ColorMode;

const SkColor kDefaultWallpaperColor = gfx::kGoogleBlue400;

// Returns the wallpaper colors for pre-Jelly.  Called for both dark and light.
SkColor GetWallpaperColor(bool is_dark_mode_enabled) {
  const SkColor default_color =
      is_dark_mode_enabled ? gfx::kGoogleGrey900 : SK_ColorWHITE;
  return ColorUtil::GetBackgroundThemedColor(default_color,
                                             is_dark_mode_enabled);
}

PrefService* GetUserPrefService(const AccountId& account_id) {
  if (!account_id.is_valid()) {
    CHECK_IS_TEST();
    return nullptr;
  }
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

class ColorPaletteControllerImpl : public ColorPaletteController,
                                   public WallpaperControllerObserver,
                                   public ColorModeObserver {
 public:
  ColorPaletteControllerImpl(
      DarkLightModeController* dark_light_mode_controller,
      WallpaperControllerImpl* wallpaper_controller,
      PrefService* local_state)
      : wallpaper_controller_(wallpaper_controller),
        dark_light_mode_controller_(dark_light_mode_controller),
        local_state_(local_state) {
    dark_light_observation_.Observe(dark_light_mode_controller);
    wallpaper_observation_.Observe(wallpaper_controller);
    Shell::Get()->login_screen_controller()->data_dispatcher()->AddObserver(
        this);
    if (!local_state) {
      // The local state should only be null in tests.
      CHECK_IS_TEST();
    }
  }

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
    absl::optional<SkColor> seed_color =
        UsesWallpaperSeedColor(account_id)
            ? GetWallpaperColorForUser(account_id)
            : GetStaticSeedColor(account_id);
    if (!seed_color) {
      return {};
    }

    seed.color_mode = dark_light_mode_controller_->IsDarkModeEnabled()
                          ? ui::ColorProviderManager::ColorMode::kDark
                          : ui::ColorProviderManager::ColorMode::kLight;
    seed.seed_color = *seed_color;
    seed.scheme = GetColorScheme(account_id);

    return seed;
  }

  absl::optional<SkColor> GetWallpaperColorForUser(
      const AccountId& account_id) const {
    if (GetActiveUserSession()) {
      return CurrentWallpaperColor(
          dark_light_mode_controller_->IsDarkModeEnabled());
    }
    const auto seed_color =
        wallpaper_controller_->GetCachedWallpaperColorForUser(account_id);
    if (seed_color.has_value()) {
      return seed_color.value();
    }
    DVLOG(1)
        << "No wallpaper color for user. Returning default wallpaper color.";
    return kDefaultWallpaperColor;
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
    if (pref_service) {
      const PrefService::Preference* pref =
          pref_service->FindPreference(prefs::kDynamicColorColorScheme);
      if (!pref->IsDefaultValue()) {
        return static_cast<ColorScheme>(pref->GetValue()->GetInt());
      }
    } else {
      CHECK(local_state_);
      const auto scheme =
          user_manager::KnownUser(local_state_)
              .FindIntPath(account_id, prefs::kDynamicColorColorScheme);
      if (scheme.has_value()) {
        return static_cast<ColorScheme>(scheme.value());
      }
    }

    DVLOG(1) << "No user pref service or local pref service available. "
                "Returning default color scheme.";
    // The preferred default color scheme for the time of day wallpaper instead
    // of tonal spot.
    return features::IsTimeOfDayWallpaperEnabled() ? ColorScheme::kNeutral
                                                   : ColorScheme::kTonalSpot;
  }

  absl::optional<SkColor> GetStaticColor(
      const AccountId& account_id) const override {
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
      seed_color.emplace(kDefaultWallpaperColor);
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

  // LoginDataDispatcher::Observer overrides:
  void OnOobeDialogStateChanged(OobeDialogState state) override {
    oobe_state_ = state;
  }

  // WallpaperControllerObserver overrides:
  void OnWallpaperColorsChanged() override {
    NotifyObservers(BestEffortSeed(GetActiveUserSession()));
  }

  void SelectLocalAccount(const AccountId& account_id) override {
    if (!chromeos::features::IsJellyEnabled()) {
      return;
    }
    NotifyObservers(GetColorPaletteSeed(account_id));
  }

  // ColorModeObserver overrides:
  void OnColorModeChanged(bool) override {
    NotifyObservers(BestEffortSeed(GetActiveUserSession()));
  }

  // SessionObserver overrides:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override {
    if (!chromeos::features::IsJellyEnabled()) {
      return;
    }
    NotifyObservers(BestEffortSeed(GetActiveUserSession()));

    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(prefs);
    UpdateLocalColorSchemePref();
    UpdateLocalSeedColorPref();

    pref_change_registrar_->Add(
        prefs::kDynamicColorColorScheme,
        base::BindRepeating(
            &ColorPaletteControllerImpl::UpdateLocalColorSchemePref,
            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kDynamicColorSeedColor,
        base::BindRepeating(
            &ColorPaletteControllerImpl::UpdateLocalSeedColorPref,
            base::Unretained(this)));
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
    if (pref_service) {
      return static_cast<SkColor>(
          pref_service->GetUint64(prefs::kDynamicColorSeedColor));
    }
    CHECK(local_state_);
    const base::Value* value =
        user_manager::KnownUser(local_state_)
            .FindPath(account_id, prefs::kDynamicColorSeedColor);
    if (value) {
      const auto seed_color = base::ValueToInt64(value);
      if (seed_color.has_value()) {
        return static_cast<SkColor>(seed_color.value());
      }
    }
    DVLOG(1) << "No user pref service or local pref service available. "
                "Returning default color palette seed.";
    return kDefaultWallpaperColor;
  }

  // Returns the seed for `session` if it's present.  Otherwise, returns a seed
  // for backward compatibility with just dark/light and seed color filled.
  absl::optional<ColorPaletteSeed> BestEffortSeed(const UserSession* session) {
    if (session) {
      return GetColorPaletteSeed(AccountFromSession(session));
    }
    session_manager::SessionState session_state =
        Shell::Get()->session_controller()->GetSessionState();
    const bool is_oobe =
        session_state == session_manager::SessionState::OOBE ||
        (session_state == session_manager::SessionState::LOGIN_PRIMARY &&
         oobe_state_ != OobeDialogState::HIDDEN);
    if (!chromeos::features::IsJellyEnabled() || is_oobe) {
      // Generate a seed where we assume TonalSpot and ignore static colors.
      ColorPaletteSeed seed;
      bool dark = dark_light_mode_controller_->IsDarkModeEnabled();
      absl::optional<SkColor> seed_color = CurrentWallpaperColor(dark);
      if (!seed_color) {
        // If `seed_color` is not available, we expect to have it shortly
        // the color computation is done and this will be called again.
        return {};
      }
      seed.color_mode = dark ? ui::ColorProviderManager::ColorMode::kDark
                             : ui::ColorProviderManager::ColorMode::kLight;
      seed.seed_color = *seed_color;
      seed.scheme = ColorScheme::kTonalSpot;

      return seed;
    }
    return {};
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

  void UpdateLocalColorSchemePref() {
    CHECK(local_state_);
    auto account_id = AccountFromSession(GetActiveUserSession());
    auto color_scheme = GetColorScheme(account_id);
    user_manager::KnownUser(local_state_)
        .SetIntegerPref(account_id, prefs::kDynamicColorColorScheme,
                        static_cast<int>(color_scheme));
  }

  void UpdateLocalSeedColorPref() {
    CHECK(local_state_);
    auto account_id = AccountFromSession(GetActiveUserSession());
    auto seed_color = GetStaticSeedColor(account_id);
    user_manager::KnownUser(local_state_)
        .SetPath(account_id, prefs::kDynamicColorSeedColor,
                 base::Int64ToValue(seed_color));
  }

  base::ScopedObservation<DarkLightModeController, ColorModeObserver>
      dark_light_observation_{this};

  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_observation_{this};

  ScopedSessionObserver scoped_session_observer_{this};

  raw_ptr<WallpaperControllerImpl> wallpaper_controller_;  // unowned

  raw_ptr<DarkLightModeController> dark_light_mode_controller_;  // unowned

  // May be null in tests.
  const raw_ptr<PrefService> local_state_;

  base::ObserverList<ColorPaletteController::Observer> observers_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  OobeDialogState oobe_state_ = OobeDialogState::HIDDEN;
};
}  // namespace

// static
std::unique_ptr<ColorPaletteController> ColorPaletteController::Create(
    DarkLightModeController* dark_light_mode_controller,
    WallpaperControllerImpl* wallpaper_controller,
    PrefService* local_state) {
  return std::make_unique<ColorPaletteControllerImpl>(
      dark_light_mode_controller, wallpaper_controller, local_state);
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

// static
void ColorPaletteController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kDynamicColorColorScheme,
                                static_cast<int>(ColorScheme::kTonalSpot));
  registry->RegisterUint64Pref(prefs::kDynamicColorSeedColor, 0);
}

}  // namespace ash
