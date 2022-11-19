// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_COLOR_PALETTE_CONTROLLER_H_
#define ASH_STYLE_COLOR_PALETTE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/observer_list_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_manager.h"

namespace ash {

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
  // The color which the palette is genertated from.
  SkColor seed_color;
  // The type of palette which is being generated.
  ColorScheme scheme;
  // Dark or light palette.
  ui::ColorProviderManager::ColorMode color_mode;
};

// Manages data for the current color scheme which is used to generate a color
// palette. Colors are derived from the seed color, scheme type, and dark/light
// mode state. This class is intended for other controllers. Views should
// observe ColorProviderSource or NativeTheme instead. Events from this class
// will fire before either of those. Also, NativeTheme can change independently
// of this class.
class ASH_EXPORT ColorPaletteController {
 public:
  // Samples of color schemes for the tri-color scheme previews.
  struct SampleScheme {
    SkColor primary;
    SkColor secondary;
    SkColor tertiary;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when the color palette is about to change but before the
    // NativeThemeChanged event fires. `seed` is what the new palette will be
    // generated from.
    virtual void OnColorPaletteChanging(const ColorPaletteSeed& seed) = 0;
  };

  static ASH_EXPORT std::unique_ptr<ColorPaletteController> Create();

  ColorPaletteController() = default;

  ColorPaletteController(const ColorPaletteController&) = delete;
  ColorPaletteController& operator=(ColorPaletteController&) = delete;

  virtual ~ColorPaletteController() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Switches color scheme to `scheme` and generates a scheme based on the
  // sampled wallpaper color. Calls `on_complete` after the scheme has been
  // applied i.e. after NativeThemeObservers have executed. `on_complete` is
  // called after the change has been applied to the UI.
  virtual void SetColorScheme(ColorScheme scheme,
                              base::OnceClosure on_complete) = 0;

  // Overrides the wallpaper color with a scheme based on the provided
  // `seed_color`. This will override whatever might be sampled from the
  // wallpaper. `on_complete` is called after the change has been applied to the
  // UI.
  virtual void SetStaticColor(SkColor seed_color,
                              base::OnceClosure on_complete) = 0;

  // Returns the most recently used ColorPaletteSeed.
  virtual ColorPaletteSeed GetColorPaletteSeed() const = 0;

  // Returns true if using a color scheme based on the current wallpaper.
  virtual bool UsesWallpaperSeedColor() const = 0;

  virtual ColorScheme color_scheme() const = 0;

  // Iff a static color is the currently selected scheme, returns that color.
  virtual absl::optional<SkColor> static_color() const = 0;

  // Generates a tri-color SampleScheme based on the current configuration for
  // the provided `scheme`. i.e. uses the current seed_color and color_mode with
  // the chosen `scheme`. The generated scheme is provided through
  // `callback`.
  using SampleSchemeCallback = base::OnceCallback<void(SampleScheme)>;
  virtual void GenerateSampleScheme(ColorScheme scheme,
                                    SampleSchemeCallback callback) const = 0;
};

}  // namespace ash

#endif  // ASH_STYLE_COLOR_PALETTE_CONTROLLER_H_
