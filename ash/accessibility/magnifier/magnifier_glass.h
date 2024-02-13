// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MAGNIFIER_MAGNIFIER_GLASS_H_
#define ASH_ACCESSIBILITY_MAGNIFIER_MAGNIFIER_GLASS_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Point;
}

namespace ui {
class Layer;
}

namespace ash {

// Shows a magnifier glass at a specified location.
class ASH_EXPORT MagnifierGlass : public aura::WindowObserver,
                                  public views::WidgetObserver {
 public:
  struct Params {
    // Ratio of magnifier scale.
    float scale = 2.f;
    // Radius of the magnifying glass in DIP.
    int radius = 64;
    // Size of the border around the magnifying glass in DIP.
    int border_size = 10;
    // Thickness of the outline around magnifying glass border in DIP.
    int border_outline_thickness = 1;
    // The color of the border and its outlines. The border has an outline on
    // both sides, producing a black/white/black ring.
    SkColor border_color = SkColorSetARGB(204, 255, 255, 255);
    SkColor border_outline_color = SkColorSetARGB(51, 0, 0, 0);
    // The shadow values for the border.
    gfx::ShadowValue bottom_shadow =
        gfx::ShadowValue(gfx::Vector2d(0, 24), 24, SkColorSetARGB(61, 0, 0, 0));
    gfx::ShadowValue top_shadow =
        gfx::ShadowValue(gfx::Vector2d(0, 0), 24, SkColorSetARGB(26, 0, 0, 0));
  };

  explicit MagnifierGlass(Params params);
  MagnifierGlass(const MagnifierGlass& other) = delete;
  MagnifierGlass& operator=(const MagnifierGlass& other) = delete;
  ~MagnifierGlass() override;

  // Shows the magnifier glass centered at |location_in_root| for |root_window|.
  void ShowFor(aura::Window* root_window, const gfx::Point& location_in_root);

  // Closes the magnifier glass widget.
  void Close();

  views::Widget* host_widget_for_testing() const { return host_widget_; }

 private:
  friend class PartialMagnifierControllerTestApi;
  class BorderRenderer;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Create or close the magnifier window.
  void CreateMagnifierWindow(aura::Window* root_window,
                             const gfx::Point& point_in_root);
  void CloseMagnifierWindow();

  // Removes this as an observer of the zoom widget and the root window.
  void RemoveZoomWidgetObservers();

  const Params params_;

  // The host widget is the root parent for all of the layers. The widget's
  // location follows the mouse, which causes the layers to also move.
  raw_ptr<views::Widget> host_widget_ = nullptr;

  // Draws a multicolored black/white/black border on top of |border_layer_|.
  // Also draws a shadow around the border. This must be ordered before
  // |border_layer_| so that it gets destroyed after |border_layer_|, otherwise
  // |border_layer_| will have a pointer to a deleted delegate.
  std::unique_ptr<BorderRenderer> border_renderer_;

  // Draws the background with a zoom filter applied.
  std::unique_ptr<ui::Layer> zoom_layer_;
  // Draws an outline that is overlaid on top of |zoom_layer_|.
  std::unique_ptr<ui::Layer> border_layer_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MAGNIFIER_MAGNIFIER_GLASS_H_
