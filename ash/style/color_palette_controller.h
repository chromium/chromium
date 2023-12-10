// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_COLOR_PALETTE_CONTROLLER_H_
#define ASH_STYLE_COLOR_PALETTE_CONTROLLER_H_

#include <optional>
#include <tuple>

#include "ash/ash_export.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/style/mojom/color_scheme.mojom-shared.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/gfx/color_palette.h"

class PrefRegistrySimple;

namespace ash {

class DarkLightModeController;
class WallpaperControllerImpl;

// An encapsulation of the data which Ash provides for the generation of a color
// palette.
struct ASH_EXPORT ColorPaletteSeed {
  // The color which the palette is generated from.
  SkColor seed_color = gfx::kGoogleBlue400;
  // The type of palette which is being generated.
  style::mojom::ColorScheme scheme = style::mojom::ColorScheme::kStatic;
  // Dark or light palette.
  ui::ColorProviderKey::ColorMode color_mode =
      ui::ColorProviderKey::ColorMode::kLight;

  bool operator==(const ColorPaletteSeed& other) const {
    return std::tie(seed_color, scheme, color_mode) ==
           std::tie(other.seed_color, other.scheme, other.color_mode);
  }
};

// Samples of color schemes for the tri-color scheme previews.
struct ASH_EXPORT SampleColorScheme {
  style::mojom::ColorScheme scheme;
  SkColor primary;
  SkColor secondary;
  SkColor tertiary;

  bool operator==(const SampleColorScheme& other) const {
    return std::tie(primary, secondary, tertiary, scheme) ==
           std::tie(other.primary, other.secondary, other.tertiary,
                    other.scheme);
  }
};

// Manages data for the current color scheme which is used to generate a color
// palette. Colors are derived from the seed color, scheme type, and dark/light
// mode state. This class is intended for other controllers. Views should
// observe ColorProviderSource or NativeTheme instead. Events from this class
// will fire before either of those. Also, NativeTheme can change independently
// of this class.
class ASH_EXPORT ColorPaletteController : public SessionObserver,
                                          public LoginDataDispatcher::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the color palette is about to change but before the
    // NativeThemeChanged event fires. `seed` is what the new palette will be
    // generated from.
    virtual void OnColorPaletteChanging(const ColorPaletteSeed& seed) = 0;
  };

  static std::unique_ptr<ColorPaletteController> Create(
      DarkLightModeController* dark_light_mode_controller,
      WallpaperControllerImpl* wallpaper_controller,
      PrefService* local_state);

  ColorPaletteController() = default;

  ColorPaletteController(const ColorPaletteController&) = delete;
  ColorPaletteController& operator=(ColorPaletteController&) = delete;

  ~ColorPaletteController() override = default;

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Switches color scheme to `scheme` and generates a scheme based on the
  // sampled wallpaper color. Calls `on_complete` after the scheme has been
  // applied i.e. after NativeThemeObservers have executed. `on_complete` is
  // called after the change has been applied to the UI.
  virtual void SetColorScheme(style::mojom::ColorScheme scheme,
                              const AccountId& account_id,
                              base::OnceClosure on_complete) = 0;

  virtual SkColor GetUserWallpaperColorOrDefault(
      SkColor default_color) const = 0;

  // Overrides the wallpaper color with a scheme based on the provided
  // `seed_color`. This will override whatever might be sampled from the
  // wallpaper. `on_complete` is called after the change has been applied to the
  // UI.
  virtual void SetStaticColor(SkColor seed_color,
                              const AccountId& account_id,
                              base::OnceClosure on_complete) = 0;

  // Returns the most recently used ColorPaletteSeed.
  virtual std::optional<ColorPaletteSeed> GetColorPaletteSeed(
      const AccountId& account_id) const = 0;

  // Returns the current seed for the current user.
  virtual std::optional<ColorPaletteSeed> GetCurrentSeed() const = 0;

  // Returns true if using a color scheme based on the current wallpaper.
  virtual bool UsesWallpaperSeedColor(const AccountId& account_id) const = 0;

  virtual style::mojom::ColorScheme GetColorScheme(
      const AccountId& account_id) const = 0;

  // Iff a static color is the currently selected scheme, returns that color.
  virtual std::optional<SkColor> GetStaticColor(
      const AccountId& account_id) const = 0;

  virtual bool GetUseKMeansPref(const AccountId& account_id) const = 0;

  // Updates the system colors with the given account's color prefs. Used for
  // the login screen.
  virtual void SelectLocalAccount(const AccountId& account_id) = 0;

  // Generates a tri-color SampleColorScheme based on the current configuration
  // for the provided `scheme`. i.e. uses the current seed_color and color_mode
  // with the chosen `scheme`. The generated scheme is provided through
  // `callback`.
  using SampleColorSchemeCallback =
      base::OnceCallback<void(const std::vector<ash::SampleColorScheme>&)>;
  virtual void GenerateSampleColorSchemes(
      base::span<const style::mojom::ColorScheme> color_scheme_buttons,
      SampleColorSchemeCallback callback) const = 0;
};

}  // namespace ash

#endif  // ASH_STYLE_COLOR_PALETTE_CONTROLLER_H_
