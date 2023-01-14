// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_palette_controller.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/color/color_provider_manager.h"

namespace ash {

namespace {

PrefService* GetUserPrefService(const AccountId& account_id) {
  DCHECK(account_id.is_valid());
  return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
      account_id);
}

// TODO(b/258719005): Finish implementation with code that works/uses libmonet.
class ColorPaletteControllerImpl : public ColorPaletteController {
 public:
  ColorPaletteControllerImpl() = default;

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
    pref_service->SetUint64(prefs::kDynamicColorSeedColor, seed_color);
    pref_service->SetInteger(prefs::kDynamicColorColorScheme,
                             static_cast<int>(ColorScheme::kStatic));
    // TODO(b/258719005): Call this after the native theme change has been
    // applied.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(on_complete), base::Milliseconds(100));
  }

  ColorPaletteSeed GetColorPaletteSeed(
      const AccountId& account_id) const override {
    PrefService* pref_service = GetUserPrefService(account_id);
    if (!pref_service) {
      DVLOG(1) << "No user pref service available. Returning default color "
                  "palette seed.";
      return {.seed_color = SK_ColorBLUE,
              .scheme = ColorScheme::kTonalSpot,
              .color_mode = ui::ColorProviderManager::ColorMode::kLight};
    }
    SkColor color = static_cast<SkColor>(
        pref_service->GetUint64(prefs::kDynamicColorSeedColor));
    return {.seed_color = color,
            .scheme = GetColorScheme(account_id),
            .color_mode = ui::ColorProviderManager::ColorMode::kLight};
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
      return pref_service->GetUint64(prefs::kDynamicColorSeedColor);
    }

    return absl::nullopt;
  }

  void GenerateSampleColorSchemes(
      const std::vector<ColorScheme>& color_scheme_buttons,
      SampleColorSchemeCallback callback) const override {
    std::vector<SampleColorScheme> samples;
    for (auto scheme : color_scheme_buttons) {
      samples.push_back(GenerateSampleColorScheme(scheme));
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(callback), samples),
        base::Milliseconds(20));
  }

 private:
  base::ObserverList<ColorPaletteController::Observer> observers_;

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
};

}  // namespace

// static
std::unique_ptr<ColorPaletteController> ColorPaletteController::Create() {
  return std::make_unique<ColorPaletteControllerImpl>();
}

// static
void ColorPaletteController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kDynamicColorColorScheme,
                                static_cast<int>(ColorScheme::kTonalSpot));
  registry->RegisterUint64Pref(prefs::kDynamicColorSeedColor, 0);
}

}  // namespace ash
