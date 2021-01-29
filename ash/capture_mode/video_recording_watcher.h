// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_VIDEO_RECORDING_WATCHER_H_
#define ASH_CAPTURE_MODE_VIDEO_RECORDING_WATCHER_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/wm/window_dimmer.h"
#include "ui/aura/scoped_window_capture_request.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/display/display_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class CaptureModeController;
class RecordedWindowRootObserver;

// An instance of this class is created while video recording is in progress to
// watch for events that end video recording, such as a window being recorded
// gets closed or moved between displays, or a display being fullscreen-recorded
// gets disconnected.
// This also paints a dimming shield to distinguish the area being recorded, but
// only when recording a window or a partial region.
// Note that this object doesn't create a new layer, rather the controller makes
// it acquire and reuse the layer of the |CaptureModeSession| prior to the
// session ending.
class ASH_EXPORT VideoRecordingWatcher : public aura::WindowObserver,
                                         public ui::LayerOwner,
                                         public ui::LayerDelegate,
                                         public wm::ActivationChangeObserver,
                                         public display::DisplayObserver,
                                         public WindowDimmer::Delegate {
 public:
  VideoRecordingWatcher(CaptureModeController* controller,
                        aura::Window* window_being_recorded);
  ~VideoRecordingWatcher() override;

  aura::Window* window_being_recorded() const { return window_being_recorded_; }
  bool should_paint_layer() const { return should_paint_layer_; }

  // aura::WindowObserver:
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowOpacitySet(aura::Window* window,
                          ui::PropertyChangeReason reason) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // WindowDimmer::Delegate:
  void OnDimmedWindowDestroying(aura::Window* window) override;
  void OnDimmedWindowParentChanged(aura::Window* dimmed_window) override;

  bool IsWindowDimmedForTesting(aura::Window* window) const;

 protected:
  // ui::LayerOwner:
  void SetLayer(std::unique_ptr<ui::Layer> layer) override;

 private:
  friend class RecordedWindowRootObserver;

  // Called by |RecordedWindowRootObserver| to notify us with a hierarchy change
  // event received by the |current_root_| window. The |target| window is the
  // window that was added to or removed from the hierarchy.
  void OnRootHierarchyChanged(aura::Window* target);

  bool CalculateShouldPaintLayer() const;

  // Uses CalculateShouldPaintLayer() to update whether we should paint the
  // recording shield, and stores the value in |should_paint_layer_|. If the
  // value of |should_paint_layer_| is changed, it schedules painting on our
  // layer.
  void UpdateShouldPaintLayer();

  // Updates our layer's parent and stacking order within its parent. It also
  // determines whether some windows need to be dimmed individually because they
  // are above the shield layer in z-order.
  void UpdateLayerStackingAndDimmers();

  CaptureModeController* const controller_;
  aura::Window* const window_being_recorded_;
  aura::Window* current_root_;
  const CaptureModeSource recording_source_;

  // Observes the hierarchy changes of the root window of the recorded window.
  // Only constructed when performing a window recording (i.e.
  // |recording_source_| is |kWindow|).
  std::unique_ptr<RecordedWindowRootObserver> root_observer_;

  // Whether or not to paint the layer content in OnPaintLayer(). The value of
  // this field is calculated and updated in UpdateShouldPaintLayer().
  bool should_paint_layer_ = false;

  // The user-selected region for the current ongoing recording. This is only
  // valid and non empty when the |recording_source_| is |kRegion|. Note that
  // this differs from |CaptureModeController::user_capture_region_| in that the
  // latter may change during the recording if the user opens capture mode again
  // to take a partial screenshot.
  gfx::Rect partial_region_bounds_;

  // Maintains window dimmers where each is mapped by the window it dims. These
  // are created for the windows that are above the |window_being_recorded_| in
  // z-order on the same display so as to clearly show they're not being
  // recorded. The ones that are below |window_being_recorded_| in z-order are
  // dimmed by the shield layer owned by |this|.
  base::flat_map<aura::Window*, std::unique_ptr<WindowDimmer>> dimmers_;

  // If |window_being_recorded_| is not a root window, we must make a request to
  // make it capturable by the |FrameSinkVideoCapturer|.
  aura::ScopedWindowCaptureRequest non_root_window_capture_request_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_VIDEO_RECORDING_WATCHER_H_
