// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_BLURRED_BACKGROUND_SHIELD_H_
#define ASH_STYLE_BLURRED_BACKGROUND_SHIELD_H_

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/view_observer.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// `BlurredBackgroundShield` holds a rounded rect solid color layer with
// background blur. It can be used as the blurred background of a view without
// clipping the contents of the view and its children. The background layer is
// always below the view's layer and has the same size of the view. When a
// dynamic color ID is set, the background color will be updated on theme
// change.
class ASH_EXPORT BlurredBackgroundShield : public views::ViewObserver {
 public:
  // When `add_layer_to_region` is true, the background layer will be added to
  // the view by `View::AddLayerToRegion` with region below. However, in rare
  // cases, the host view's `AddLayerToRegion` is overridden such that the layer
  // may not be below the view's layer, e.g., `views::LabelButton`. In these
  // cases, the `add_layer_to_region` should be set to false and the background
  // layer will manually be located under the view's layer.
  BlurredBackgroundShield(views::View* host,
                          absl::variant<SkColor, ui::ColorId> color,
                          float blur_sigma,
                          const gfx::RoundedCornersF& rounded_corners,
                          bool add_layer_to_region = true);
  BlurredBackgroundShield(const BlurredBackgroundShield&) = delete;
  BlurredBackgroundShield& operator=(const BlurredBackgroundShield&) = delete;
  ~BlurredBackgroundShield() override;

  void SetColor(SkColor color);
  void SetColorId(ui::ColorId color_id);

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override;
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;
  void OnViewLayerBoundsSet(views::View* observed_view) override;
  void OnViewThemeChanged(views::View* observed_view) override;

 private:
  void StackLayerBelowHost();
  void UpdateBackgroundColor();

  ui::Layer background_layer_ = ui::Layer(ui::LAYER_SOLID_COLOR);
  const raw_ptr<views::View> host_;
  absl::variant<SkColor, ui::ColorId> color_;
  const float blur_sigma_;
  // If the background layer should be added to the view's region below.
  const bool add_layer_to_region_;
  base::ScopedObservation<views::View, views::ViewObserver> host_observation_{
      this};
};

}  // namespace ash

#endif  // ASH_STYLE_BLURRED_BACKGROUND_SHIELD_H_
