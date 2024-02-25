// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CURTAIN_SECURITY_CURTAIN_WIDGET_CONTROLLER_H_
#define ASH_CURTAIN_SECURITY_CURTAIN_WIDGET_CONTROLLER_H_

#include <cstdint>
#include <memory>

#include "ash/ash_export.h"
#include "ash/curtain/security_curtain_controller.h"
#include "ui/aura/window_occlusion_tracker.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
class Widget;
}  // namespace views

namespace ui {
class Layer;
}  // namespace ui

namespace ash::curtain {

// Displays a curtain widget over a single display, which will cover all other
// content, preventing local users and passerby's from observing the display.
// Owns the widget.
class ASH_EXPORT SecurityCurtainWidgetController {
 public:
  SecurityCurtainWidgetController(SecurityCurtainWidgetController&&);
  SecurityCurtainWidgetController& operator=(SecurityCurtainWidgetController&&);
  ~SecurityCurtainWidgetController();

  // Creates a new curtain overlay.
  static SecurityCurtainWidgetController CreateForRootWindow(
      aura::Window* curtain_container,
      std::unique_ptr<views::View> curtain_view);

  const views::Widget& GetWidget() const;
  views::Widget& GetWidget();

 private:
  class WidgetMaximizer;

  using Layers = std::vector<std::unique_ptr<ui::Layer>>;
  SecurityCurtainWidgetController(std::unique_ptr<views::Widget> widget,
                                  Layers layers);

  Layers widget_layers_;
  std::unique_ptr<views::Widget> widget_;
  // The curtain widget should not occlude any other windows, otherwise they
  // might not be rendered (which will be a problem when streaming the
  // uncurtained desktop for example through Chrome Remote Desktop).
  std::unique_ptr<aura::WindowOcclusionTracker::ScopedExclude>
      occlusion_tracker_exclude_;

  // Ensures the widget is always maximized, even when the display is resized.
  std::unique_ptr<WidgetMaximizer> widget_maximizer_;
};

}  // namespace ash::curtain

#endif  // ASH_CURTAIN_SECURITY_CURTAIN_WIDGET_CONTROLLER_H_
