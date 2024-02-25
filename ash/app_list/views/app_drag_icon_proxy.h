// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_DRAG_ICON_PROXY_H_
#define ASH_APP_LIST_VIEWS_APP_DRAG_ICON_PROXY_H_

#include <memory>
#include "ash/style/system_shadow.h"
#include "base/functional/callback.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class ImageSkia;
class Point;
class Rect;
}  // namespace gfx

namespace ui {
class Layer;
class LayerOwner;
}  // namespace ui

namespace ash {

// Manages the drag image shown while an app is being dragged in app list or
// shelf. It creates a DragImageView widget in a window container used for
// drag images, so the app icon can escape the views container that owns the
// dragged app view. The widget is destroyed when this goes out of scope.
class AppDragIconProxy {
 public:
  // `root_window` - The root window to which the proxy should be added.
  // `icon` - The icon to be used for the app icon.
  // `badge_icon` - If non-empty, the badge icon to be overlaied over the app
  //     icon.
  // `pointer_location_in_screen` - The initial pointer location.
  // `pointer_offset_from_center` - The pointer offset from the center of the
  //     drag image. The drag icon position will be offset from the pointer
  //     location to maintain pointer offset from the drag image center.
  // `scale_factor` - The scale factor by which the `icon` should be scaled when
  //     shown as a drag image.
  // `is_folder_icon` - whether the icon dragged is a folder.
  // `shadow_size` - specify the size of the shadow, which will be drawn at the
  // center of the icon proxy.
  AppDragIconProxy(aura::Window* root_window,
                   const gfx::ImageSkia& icon,
                   const gfx::ImageSkia& badge_icon,
                   const gfx::Point& pointer_location_in_screen,
                   const gfx::Vector2d& pointer_offset_from_center,
                   float scale_factor,
                   bool is_folder_icon,
                   const gfx::Size& shadow_size);
  AppDragIconProxy(const AppDragIconProxy&) = delete;
  AppDragIconProxy& operator=(const AppDragIconProxy&) = delete;
  ~AppDragIconProxy();

  // Updates drag icon position to match the new pointer location.
  void UpdatePosition(const gfx::Point& pointer_location_in_screen);

  // Animates the drag image to the provided bounds, and closes the widget once
  // the animation completes. Expected to be called at most once.
  // `animation_completion_callback` - Called when the animation completes, or
  // when the `AppDragIconProxy` gets deleted.
  void AnimateToBoundsAndCloseWidget(
      const gfx::Rect& bounds_in_screen,
      base::OnceClosure animation_completion_callback);

  // Sets the drag image opacity.
  void SetOpacity(float opacity);

  // Returns the current drag image bounds in screen.
  gfx::Rect GetBoundsInScreen() const;

  // Returns the drag image view's layer.
  ui::Layer* GetImageLayerForTesting();

  // Returns the drag image widget.
  views::Widget* GetWidgetForTesting();

  gfx::Rect shadow_bounds_for_testing() const {
    return shadow_->GetContentBounds();
  }

  // Returns the layer that is used to blur the background.
  ui::Layer* GetBlurredLayerForTesting();

 private:
  void OnProxyAnimationCompleted();

  // Whether close animation (see `AnimateToBoundsAndCloseWidget()`) is in
  // progress.
  bool closing_widget_ = false;

  std::unique_ptr<SystemShadow> shadow_;

  // A layer that is used to blur the background of the dragged icon. Only used
  // for refreshed folder icons.
  std::unique_ptr<ui::LayerOwner> blurred_background_layer_;

  // The widget used to display the drag image.
  views::UniqueWidgetPtr drag_image_widget_;

  // The cursor offset to the center of the dragged item.
  gfx::Vector2d drag_image_offset_;

  base::OnceClosure animation_completion_callback_;

  base::WeakPtrFactory<AppDragIconProxy> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_DRAG_ICON_PROXY_H_
