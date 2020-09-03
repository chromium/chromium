// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ash {

class CaptureModeBarView;
class CaptureModeController;

// Encapsulates an active capture mode session (i.e. an instance of this class
// lives as long as capture mode is active). It creates and owns the capture
// mode bar widget.
// The CaptureModeSession is a LayerOwner that owns a texture layer placed right
// beneath the layer of the bar widget. This layer is used to paint a dimming
// shield of the areas that won't be captured, and another bright region showing
// the one that will be.
class ASH_EXPORT CaptureModeSession : public ui::LayerOwner,
                                      public ui::LayerDelegate,
                                      public ui::EventHandler {
 public:
  // Creates the bar widget on the given |root| window.
  CaptureModeSession(CaptureModeController* controller, aura::Window* root);
  CaptureModeSession(const CaptureModeSession&) = delete;
  CaptureModeSession& operator=(const CaptureModeSession&) = delete;
  ~CaptureModeSession() override;

  aura::Window* current_root() const { return current_root_; }
  CaptureModeBarView* capture_mode_bar_view() const {
    return capture_mode_bar_view_;
  }

  // Gets the current window selected for |kWindow| capture source. Returns
  // nullptr if no window is available for selection.
  aura::Window* GetSelectedWindow() const;

  // Called when either the capture source or type changes.
  void OnCaptureSourceChanged(CaptureModeSource new_source);
  void OnCaptureTypeChanged(CaptureModeType new_type);

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

 private:
  // Gets the bounds of current window selected for |kWindow| capture source.
  gfx::Rect GetSelectedWindowBounds() const;

  // Ensures that the bar widget is on top of everything, and the overlay (which
  // is the |layer()| of this class that paints the capture region) is stacked
  // right below the bar.
  void RefreshStackingOrder(aura::Window* parent_container);

  // Paints the current capture region depending on the current capture source.
  void PaintCaptureRegion(gfx::Canvas* canvas);

  CaptureModeController* const controller_;

  // The current root window on which the capture session is active, which may
  // change if the user warps the cursor to another display in some situations.
  aura::Window* current_root_;

  views::Widget capture_mode_bar_widget_;

  // The content view of the above widget and owned by its views hierarchy.
  CaptureModeBarView* capture_mode_bar_view_;

  // Caches the old status of mouse warping before the session started to be
  // restored at the end.
  bool old_mouse_warp_status_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_
