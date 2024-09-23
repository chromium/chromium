// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_VIDEO_RECORDING_WATCHER_H_
#define ASH_CAPTURE_MODE_VIDEO_RECORDING_WATCHER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/wm/window_dimmer.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "ui/aura/scoped_window_capture_request.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/wm/public/activation_change_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace wm {
class CursorManager;
}  // namespace wm

namespace ash {

class CaptureModeBehavior;
class CaptureModeController;
class CaptureModeDemoToolsController;
class RecordedWindowRootObserver;

// An instance of this class is created while video recording is in progress to
// watch for events that end video recording, such as a window being recorded
// gets closed or moved between displays, or a display being fullscreen-recorded
// gets disconnected.
// This also paints a dimming shield to distinguish the area being recorded, but
// only when recording a window or a partial region.
// Note that this object doesn't create a new layer, rather the controller makes
// it acquire and reuse the layer of the `CaptureModeSession` prior to the
// session ending.
// It also controls the overlay created on the video capturer to efficiently
// record the mouse cursor on top of the video frames.
class ASH_EXPORT VideoRecordingWatcher
    : public aura::WindowObserver,
      public ui::LayerOwner,
      public ui::LayerDelegate,
      public wm::ActivationChangeObserver,
      public display::DisplayObserver,
      public WindowDimmer::Delegate,
      public ui::EventHandler,
      public CursorWindowController::Observer,
      public ui::ColorProviderSourceObserver {
 public:
  VideoRecordingWatcher(
      CaptureModeController* controller,
      CaptureModeBehavior* active_behavior,
      aura::Window* window_being_recorded,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCaptureOverlay>
          cursor_capture_overlay,
      bool is_recording_audio);
  ~VideoRecordingWatcher() override;

  const CaptureModeBehavior* active_behavior() const {
    return active_behavior_;
  }
  aura::Window* window_being_recorded() const { return window_being_recorded_; }
  bool is_recording_audio() const { return is_recording_audio_; }
  bool should_paint_layer() const { return should_paint_layer_; }
  bool is_shutting_down() const { return is_shutting_down_; }
  CaptureModeSource recording_source() const { return recording_source_; }

  // Clean up prior to deletion.
  void ShutDown();

  // Returns the current parent window for the on-capture-surface widgets such
  // as `CaptureModeCameraController::camera_preview_widget_` and
  // `CaptureModeDemoToolsController::key_combo_widget_` when recording is in
  // progress.
  aura::Window* GetOnCaptureSurfaceWidgetParentWindow() const;

  // Returns the bounds within which the on-capture-surface widgets (such as
  // capture mode camera preview widget and key combo widget) will be confined
  // when recording is in progress.
  gfx::Rect GetCaptureSurfaceConfineBounds() const;

  // Returns the `partial_region_bounds_` clamped to the bounds of the
  // `current_root_`. It should only be called if `recording_source_` is
  // `kRegion`.
  gfx::Rect GetEffectivePartialRegionBounds() const;

  // Returns the `key_combo_widget_` if it is visible.
  const views::Widget* GetKeyComboWidgetIfVisible() const;

  // aura::WindowObserver:
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
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
  void OnDisplayTabletStateChanged(display::TabletState state) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // WindowDimmer::Delegate:
  void OnDimmedWindowDestroying(aura::Window* window) override;
  void OnDimmedWindowParentChanged(aura::Window* dimmed_window) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // CursorWindowController::Observer:
  void OnCursorCompositingStateChanged(bool enabled) override;

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override;

  bool IsWindowDimmedForTesting(aura::Window* window) const;

  void BindCursorOverlayForTesting(
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCaptureOverlay> overlay);

  void FlushCursorOverlayForTesting();

  void SendThrottledWindowSizeChangedNowForTesting();

  CaptureModeDemoToolsController* demo_tools_controller_for_testing() const {
    return demo_tools_controller_.get();
  }

 protected:
  // ui::LayerOwner:
  void SetLayer(std::unique_ptr<ui::Layer> layer) override;

 private:
  friend class CaptureModeTestApi;
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

  // Returns the current native cursor from |cursor_manager_|.
  gfx::NativeCursor GetCurrentCursor() const;

  // Updates the cursor overlay using the given |location| in the coordinates of
  // the |window_being_recorded_|. This is used for non-pressed/-released mouse
  // events which can be too frequent. Recording is performed at a rate of 30
  // FPS, so we don't need to send every mouse event to the capturer overlay on
  // Viz via mojo. Such events can be throttled using the
  // |cursor_events_throttle_timer_|.
  void UpdateOrThrottleCursorOverlay(const gfx::PointF& location);

  // As opposed to the above UpdateOrThrottleCursorOverlay(), this updates the
  // cursor capturer overlay immediately without throttling, if such an update
  // is needed (e.g. the cursor bitmap changed, or the location changed, or
  // both). This also cancels any pending throttled update, since this immediate
  // one is more recent.
  void UpdateCursorOverlayNow(const gfx::PointF& location);

  // Hides the cursor overlay in the video capturer. Note that this doesn't
  // necessarily mean that the video won't contain a cursor, since the software-
  // composited cursor might be enabled. See |force_cursor_overlay_hidden_|.
  void HideCursorOverlay();

  // Invoked when the |cursor_events_throttle_timer_| fires, in order to update
  // the cursor overlay with the pending most recently throttled mouse event
  // location in |throttled_cursor_location_| if any.
  void OnCursorThrottleTimerFiring();

  // Invoked when the |window_size_change_throttle_timer_| fires, in order to
  // push the current size of the window being recorded to the service.
  void OnWindowSizeChangeThrottleTimerFiring();

  const raw_ptr<CaptureModeController> controller_;

  // The currently active behavior which is passed from capture mode session.
  const raw_ptr<CaptureModeBehavior, DanglingUntriaged> active_behavior_;
  const raw_ptr<wm::CursorManager> cursor_manager_;
  const raw_ptr<aura::Window, DanglingUntriaged> window_being_recorded_;
  raw_ptr<aura::Window, DanglingUntriaged> current_root_;
  const CaptureModeSource recording_source_;

  // The end point of the overlay owned by the video capturer on Viz, which is
  // used to blit the mouse cursor onto the recorded video frames.
  mojo::Remote<viz::mojom::FrameSinkVideoCaptureOverlay>
      cursor_capture_overlay_remote_;

  // Observes the hierarchy changes of the root window of the recorded window.
  // Only constructed when performing a window recording (i.e.
  // |recording_source_| is |kWindow|).
  std::unique_ptr<RecordedWindowRootObserver> root_observer_;

  // The last cursor we used to update the cursor overlay. This is used to
  // determine whether we need to update the cursor bitmap.
  gfx::NativeCursor last_cursor_;

  // The last bounds we used to update the cursor overlay. This is used to skip
  // the update if the bounds didn't change.
  // Note that these bounds are relative within the bounds of the recorded frame
  // sink, i.e. in the range [0.f, 1.f) for both origin() and size().
  // See documentation of FrameSinkVideoCaptureOverlay for more details.
  gfx::RectF last_cursor_overlay_bounds_;

  // Since recording happens at a rate of 30 FPS, there's no need to send every
  // mouse move event (or equivalent events such as enter, exit, dragged, ...
  // etc.) to the cursor overlay. This timer is used to throttle such events
  // received while this timer is running, and their location will overwrite the
  // value of |throttled_cursor_location_|. Once the timer fires,
  // OnCursorThrottleTimerFiring() will be called to update the overlay with the
  // most recent received throttled event.
  base::OneShotTimer cursor_events_throttle_timer_;

  // Stores the location of the most recent throttled mouse event (i.e. received
  // while the |cursor_events_throttle_timer_| was running). The location is in
  // the |window_being_recorded_| coordinates.
  std::optional<gfx::PointF> throttled_cursor_location_;

  // Resizing a window can generate many intermediate steps, and it would be
  // inefficient to push all of them to the recording service, causing a
  // repeated reconfiguration of the video encoder. This timer is used to
  // throttle such events.
  base::OneShotTimer window_size_change_throttle_timer_;

  // True if this active recording session started with audio recording turned
  // on, and audio recording is being done by the recording service.
  const bool is_recording_audio_;

  // True if we force hiding the cursor overlay. This happens when we record a
  // fullscreen, or a partial screen region, and the software-composited cursor
  // gets enabled. The software-composited cursor is already part of the root
  // window's frame sink which we record, so we don't need to show the cursor
  // overlay. Otherwise, the video will end up with two overlapping cursors.
  bool force_cursor_overlay_hidden_ = false;

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

  std::unique_ptr<CaptureModeDemoToolsController> demo_tools_controller_;

  // True if the shutting down process has been triggered. We want to keep
  // `is_shutting_down_` as the last member variable in this class.
  bool is_shutting_down_ = false;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_VIDEO_RECORDING_WATCHER_H_
