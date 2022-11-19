// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_palette_controller.h"

#include <memory>

#include "base/logging.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"

namespace ash {

namespace {

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
                      base::OnceClosure on_complete) override {
    DVLOG(1) << "Setting color scheme to: " << (int)scheme;
    current_scheme_ = scheme;
    // TODO(b/258719005): Call this after the native theme change has been
    // applied. Also, actually change things.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(on_complete), base::Milliseconds(100));
  }

  void SetStaticColor(SkColor seed_color,
                      base::OnceClosure on_complete) override {
    DVLOG(1) << "Static color scheme: " << (int)seed_color;
    static_color_ = seed_color;
    current_scheme_ = ColorScheme::kStatic;
    // TODO(b/258719005): Call this after the native theme change has been
    // applied. Also, actually change things.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(on_complete), base::Milliseconds(100));
  }

  ColorPaletteSeed GetColorPaletteSeed() const override {
    // TODO(b/258719005):  Implement me!
    return {.seed_color = static_color_,
            .scheme = current_scheme_,
            .color_mode = ui::ColorProviderManager::ColorMode::kLight};
  }

  bool UsesWallpaperSeedColor() const override {
    // Scheme tracks if wallpaper color is used.
    return current_scheme_ != ColorScheme::kStatic;
  }

  ColorScheme color_scheme() const override { return current_scheme_; }

  absl::optional<SkColor> static_color() const override {
    if (current_scheme_ == ColorScheme::kStatic) {
      return static_color_;
    }

    return absl::nullopt;
  }

  void GenerateSampleScheme(ColorScheme scheme,
                            SampleSchemeCallback callback) const override {
    // TODO(b/258719005): Return correct and different schemes for each
    // `scheme`.
    DCHECK_NE(scheme, ColorScheme::kStatic)
        << "Requesting a static scheme doesn't make sense since there is no "
           "seed color";
    SampleScheme sample = {.primary = SK_ColorRED,
                           .secondary = SK_ColorGREEN,
                           .tertiary = SK_ColorBLUE};
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(callback), sample),
        base::Milliseconds(20));
  }

 private:
  SkColor static_color_ = SK_ColorBLUE;
  ColorScheme current_scheme_ = ColorScheme::kTonalSpot;
  base::ObserverList<ColorPaletteController::Observer> observers_;
};

}  // namespace

// static
std::unique_ptr<ColorPaletteController> ColorPaletteController::Create() {
  return std::make_unique<ColorPaletteControllerImpl>();
}

}  // namespace ash
