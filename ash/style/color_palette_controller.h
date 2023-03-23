// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_COLOR_PALETTE_CONTROLLER_H_
#define ASH_STYLE_COLOR_PALETTE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/containers/span.h"
#include "base/observer_list_types.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_manager.h"

class PrefRegistrySimple;

namespace ash {

class DarkLightModeController;
class WallpaperControllerImpl;

// Types of ColorSchemes. For a given seed color, each ColorScheme will generate
// a different color palette/set of ref colors.
enum class ASH_EXPORT ColorScheme {
  kStatic,  // TonalSpot but with a static color.
  kTonalSpot,
  kNeutral,
  kExpressive,
  kVibrant
};

// An encapsulation of the data which Ash provides for the generation of a color
// palette.
struct ASH_EXPORT ColorPaletteSeed {
  // The color which the palette is generated from.
  SkColor seed_color;
  // The type of palette which is being generated.
  ColorScheme scheme;
  // Dark or light palette.
  ui::ColorProviderManager::ColorMode color_mode;
};

// Samples of color schemes for the tri-color scheme previews.
struct ASH_EXPORT SampleColorScheme {
  ColorScheme scheme;
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
class ASH_EXPORT ColorPaletteController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the color palette is about to change but before the
    // NativeThemeChanged event fires. `seed` is what the new palette will be
    // generated from.
    virtual void OnColorPaletteChanging(const ColorPaletteSeed& seed) = 0;
  };

  // Temporary factory for migration.  DO NOT USE.
  static std::unique_ptr<ColorPaletteController> Create();

  static std::unique_ptr<ColorPaletteController> Create(
      DarkLightModeController* dark_light_mode_controller,
      WallpaperControllerImpl* wallpaper_controller);

  ColorPaletteController() = default;

  ColorPaletteController(const ColorPaletteController&) = delete;
  ColorPaletteController& operator=(ColorPaletteController&) = delete;

  virtual ~ColorPaletteController() = default;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Switches color scheme to `scheme` and generates a scheme based on the
  // sampled wallpaper color. Calls `on_complete` after the scheme has been
  // applied i.e. after NativeThemeObservers have executed. `on_complete` is
  // called after the change has been applied to the UI.
  virtual void SetColorScheme(ColorScheme scheme,
                              const AccountId& account_id,
                              base::OnceClosure on_complete) = 0;

  // Overrides the wallpaper color with a scheme based on the provided
  // `seed_color`. This will override whatever might be sampled from the
  // wallpaper. `on_complete` is called after the change has been applied to the
  // UI.
  virtual void SetStaticColor(SkColor seed_color,
                              const AccountId& account_id,
                              base::OnceClosure on_complete) = 0;

  // Returns the most recently used ColorPaletteSeed.
  virtual ColorPaletteSeed GetColorPaletteSeed(
      const AccountId& account_id) const = 0;

  // Returns true if using a color scheme based on the current wallpaper.
  virtual bool UsesWallpaperSeedColor(const AccountId& account_id) const = 0;

  virtual ColorScheme GetColorScheme(const AccountId& account_id) const = 0;

  // Iff a static color is the currently selected scheme, returns that color.
  virtual absl::optional<SkColor> GetStaticColor(
      const AccountId& account_id) const = 0;

  // Generates a tri-color SampleColorScheme based on the current configuration
  // for the provided `scheme`. i.e. uses the current seed_color and color_mode
  // with the chosen `scheme`. The generated scheme is provided through
  // `callback`.
  using SampleColorSchemeCallback =
      base::OnceCallback<void(const std::vector<ash::SampleColorScheme>&)>;
  virtual void GenerateSampleColorSchemes(
      base::span<const ColorScheme> color_scheme_buttons,
      SampleColorSchemeCallback callback) const = 0;
};

}  // namespace ash

#endif  // ASH_STYLE_COLOR_PALETTE_CONTROLLER_H_
