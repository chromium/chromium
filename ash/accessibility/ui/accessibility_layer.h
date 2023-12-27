// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_ACCESSIBILITY_LAYER_H_
#define ASH_ACCESSIBILITY_UI_ACCESSIBILITY_LAYER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

// A delegate interface implemented by the object that owns an
// AccessibilityLayer.
class AccessibilityLayerDelegate {
 public:
  virtual void OnDeviceScaleFactorChanged() = 0;

 protected:
  virtual ~AccessibilityLayerDelegate() {}
};

// AccessibilityLayer manages a global always-on-top layer used to
// highlight or annotate UI elements for accessibility.
class AccessibilityLayer : public ui::LayerDelegate {
 public:
  explicit AccessibilityLayer(AccessibilityLayerDelegate* delegate);

  AccessibilityLayer(const AccessibilityLayer&) = delete;
  AccessibilityLayer& operator=(const AccessibilityLayer&) = delete;

  ~AccessibilityLayer() override;

  // Move the accessibility layer to the given bounds in the coordinates of
  // the given root window.
  void Set(aura::Window* root_window,
           const gfx::Rect& bounds,
           bool stack_at_top);

  // Set the layer's opacity.
  void SetOpacity(float opacity);

  // Set the layer's offset from parent layer.
  void SetSubpixelPositionOffset(const gfx::Vector2dF& offset);

  // Gets the inset for this layer in DIPs. This is used to increase
  // the bounding box to provide space for any margins or padding.
  virtual int GetInset() const = 0;

  ui::Layer* layer() { return layer_.get(); }
  aura::Window* root_window() { return root_window_; }

 protected:
  // Updates |root_window_| and creates |layer_| if it doesn't exist,
  // or if the root window has changed. Moves the layer to the top if
  // |stack_at_top| is true, otherwise moves layer to the bottom.
  void CreateOrUpdateLayer(aura::Window* root_window,
                           const char* layer_name,
                           const gfx::Rect& bounds,
                           bool stack_at_top);

  // The current root window containing the focused object.
  raw_ptr<aura::Window, DanglingUntriaged> root_window_ = nullptr;

  // The current layer.
  std::unique_ptr<ui::Layer> layer_;

  // The bounding rectangle of the focused object, in |root_window_|
  // coordinates.
  gfx::Rect layer_rect_;

 private:
  // ui::LayerDelegate overrides:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // The object that owns this layer.
  raw_ptr<AccessibilityLayerDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_ACCESSIBILITY_LAYER_H_
