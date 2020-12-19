// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/magnifier/magnifier_glass.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/display/display_observer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ash {

class CaptureModeBarView;
class CaptureModeController;
class CaptureWindowObserver;
class WindowDimmer;

// Encapsulates an active capture mode session (i.e. an instance of this class
// lives as long as capture mode is active). It creates and owns the capture
// mode bar widget.
// The CaptureModeSession is a LayerOwner that owns a texture layer placed right
// beneath the layer of the bar widget. This layer is used to paint a dimming
// shield of the areas that won't be captured, and another bright region showing
// the one that will be.
class ASH_EXPORT CaptureModeSession : public ui::LayerOwner,
                                      public ui::LayerDelegate,
                                      public ui::EventHandler,
                                      public TabletModeObserver,
                                      public aura::WindowObserver,
                                      public display::DisplayObserver {
 public:
  // Creates the bar widget on a calculated root window.
  explicit CaptureModeSession(CaptureModeController* controller);
  CaptureModeSession(const CaptureModeSession&) = delete;
  CaptureModeSession& operator=(const CaptureModeSession&) = delete;
  ~CaptureModeSession() override;

  // The vertical distance from the size label to the custom capture region.
  static constexpr int kSizeLabelYDistanceFromRegionDp = 8;

  // The vertical distance of the capture button from the capture region, if the
  // capture button does not fit inside the capture region.
  static constexpr int kCaptureButtonDistanceFromRegionDp = 24;

  aura::Window* current_root() const { return current_root_; }
  bool is_selecting_region() const { return is_selecting_region_; }
  bool is_drag_in_progress() const { return is_drag_in_progress_; }

  // Gets the current window selected for |kWindow| capture source. Returns
  // nullptr if no window is available for selection.
  aura::Window* GetSelectedWindow() const;

  // Called when either the capture source or type changes.
  void OnCaptureSourceChanged(CaptureModeSource new_source);
  void OnCaptureTypeChanged(CaptureModeType new_type);

  // Called when the user performs a capture. Records histograms related to this
  // session.
  void ReportSessionHistograms();

  // Called when starting 3-seconds count down before recording video.
  void StartCountDown(base::OnceClosure countdown_finished_callback);

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

 private:
  friend class CaptureModeSessionTestApi;
  class CursorSetter;

  // Gets the bounds of current window selected for |kWindow| capture source.
  gfx::Rect GetSelectedWindowBounds() const;

  // Ensures that the bar widget is on top of everything, and the overlay (which
  // is the |layer()| of this class that paints the capture region) is stacked
  // right below the bar.
  void RefreshStackingOrder(aura::Window* parent_container);

  // Paints the current capture region depending on the current capture source.
  void PaintCaptureRegion(gfx::Canvas* canvas);

  // Helper to unify mouse/touch events. Forwards events to the three below
  // functions and they are located on |capture_button_widget_|. Blocks events
  // from reaching other handlers, unless the event is located on
  // |capture_mode_bar_widget_|. |is_touch| signifies this is a touch event, and
  // we will use larger hit targets for the drag affordances.
  void OnLocatedEvent(ui::LocatedEvent* event, bool is_touch);

  // Returns the fine tune position that corresponds to the given
  // |location_in_root|.
  FineTunePosition GetFineTunePosition(const gfx::Point& location_in_root,
                                       bool is_touch) const;

  // Handles updating the select region UI.
  void OnLocatedEventPressed(const gfx::Point& location_in_root,
                             bool is_touch,
                             bool is_event_on_capture_bar);
  void OnLocatedEventDragged(const gfx::Point& location_in_root);
  void OnLocatedEventReleased(bool is_event_on_capture_bar,
                              bool region_intersects_capture_bar);

  // Updates the capture region and the capture region widgets depending on the
  // value of |is_resizing|. |by_user| is true if the capture region is changed
  // by user.
  void UpdateCaptureRegion(const gfx::Rect& new_capture_region,
                           bool is_resizing,
                           bool by_user);

  // Updates the dimensions label widget shown during a region capture session.
  // If not |is_resizing|, not a region capture session or the capture region is
  // empty, clear existing |dimensions_label_widget_|. Otherwise, create and
  // update the dimensions label.
  void UpdateDimensionsLabelWidget(bool is_resizing);

  // Updates the bounds of |dimensions_label_widget_| relative to the current
  // capture region. Both |dimensions_label_widget_| and its content view must
  // exist.
  void UpdateDimensionsLabelBounds();

  // If |fine_tune_position_| is not a corner, do nothing. Otherwise show
  // |magnifier_glass_| at |location_in_root| in the current root window and
  // hide the cursor.
  void MaybeShowMagnifierGlassAtPoint(const gfx::Point& location_in_root);

  // Closes |magnifier_glass_|.
  void CloseMagnifierGlass();

  // Retrieves the anchor points on the current selected region associated with
  // |position|. The anchor points are described as the points that do not
  // change when resizing the capture region while dragging one of the drag
  // affordances. There is one anchor point if |position| is a vertex, and two
  // anchor points if |position| is an edge.
  std::vector<gfx::Point> GetAnchorPointsForPosition(FineTunePosition position);

  // Updates the capture label widget's icon/text and bounds.
  void UpdateCaptureLabelWidget();
  // Updates the capture label widget's bounds. If |animate| is true, do bounds
  // animation.
  void UpdateCaptureLabelWidgetBounds(bool animate);
  // Calculates the targeted capture label widget bounds in screen coordinates.
  gfx::Rect CalculateCaptureLabelWidgetBounds();

  // Returns true if the capture label should handle the event. |event_target|
  // is the window which is receiving the event. The capture label should handle
  // the event if its associated window is |event_target| and its capture button
  // child is visible.
  bool ShouldCaptureLabelHandleEvent(aura::Window* event_target);

  // Handles changing |root_window_| when the mouse cursor changes to another
  // display, or if a display was removed. Moves the capture mode widgets to
  // |new_root| depending on the capture mode source an whether it was a display
  // removal.
  void MaybeChangeRoot(aura::Window* new_root);

  // Updates |root_window_dimmers_| to dim the correct root windows.
  void UpdateRootWindowDimmers();

  // Returns true if we are currently in video recording countdown animation.
  bool IsInCountDownAnimation() const;

  // Updates the current cursor depending on current |location_in_screen| and
  // current capture type and source. |is_touch| is used when calculating fine
  // tune position in region capture mode. We'll have a larger hit test region
  // for the touch events than the mouse events.
  void UpdateCursor(const gfx::Point& location_in_screen, bool is_touch);

  // Returns true if we're using custom image capture icon when |type| is
  // kImage or using custom video capture icon when |type| is kVideo.
  bool IsUsingCustomCursor(CaptureModeType type) const;

  // Updates the capture bar widget with a given opacity. There is a different
  // animation duration and tween type for mouse/touch release.
  void UpdateCaptureBarWidgetOpacity(float opacity, bool on_release);

  // Ensure the user region in |controller_| is within the bounds of the root
  // window. This is called when creating |this| or when the display bounds have
  // changed.
  void ClampCaptureRegionToRootWindowSize();

  // Ends a region selection. Cleans up internal state and updates the cursor,
  // capture bar opacity and magnifier glass.
  void EndSelection(bool is_event_on_capture_bar,
                    bool region_intersects_capture_bar);

  // Schedules a paint on the region and enough inset around it so that the
  // shadow, affordance circles, etc. are all repainted.
  void RepaintRegion();

  // Selects a default region that is centered and whose size is a ratio of the
  // root window bounds. Called when the space key is pressed.
  void SelectDefaultRegion();

  // Updates the region either horizontally or vertically. Called when the arrow
  // keys are pressed.
  void UpdateRegionHorizontally(bool left, bool is_shift_down);
  void UpdateRegionVertically(bool up, bool is_shift_down);

  CaptureModeController* const controller_;

  // The current root window on which the capture session is active, which may
  // change if the user warps the cursor to another display in some situations.
  aura::Window* current_root_;

  views::UniqueWidgetPtr capture_mode_bar_widget_ =
      std::make_unique<views::Widget>();

  // The content view of the above widget and owned by its views hierarchy.
  CaptureModeBarView* capture_mode_bar_view_ = nullptr;

  // Widget which displays capture region size during a region capture session.
  views::UniqueWidgetPtr dimensions_label_widget_;

  // Widget that shows an optional icon and a message in the middle of the
  // screen or in the middle of the capture region and prompts the user what to
  // do next. The icon and message can be different in different capture type
  // and source and can be empty in some cases. And in video capture mode, when
  // starting capturing, the widget will transform into a 3-second countdown
  // timer.
  views::UniqueWidgetPtr capture_label_widget_;

  // Magnifier glass used during a region capture session.
  MagnifierGlass magnifier_glass_;

  // Stores the data needed to select a region during a region capture session.
  // This variable indicates if the user is currently selecting a region to
  // capture, it will be true when the first mouse/touch presses down and will
  // remain true until the mouse/touch releases up. After that, if the capture
  // region is non empty, the capture session will enter the fine tune phase,
  // where the user can reposition and resize the region with a lot of accuracy.
  bool is_selecting_region_ = false;

  // The location of the last press and drag events.
  gfx::Point initial_location_in_root_;
  gfx::Point previous_location_in_root_;
  // The position of the last press event during the fine tune phase drag.
  FineTunePosition fine_tune_position_ = FineTunePosition::kNone;
  // The points that do not change during a fine tune resize. This is empty
  // when |fine_tune_position_| is kNone or kCenter, or if there is no drag
  // underway.
  std::vector<gfx::Point> anchor_points_;

  // Caches the old status of mouse warping while dragging or resizing a
  // captured region.
  base::Optional<bool> old_mouse_warp_status_;

  // Observer to observe the current selected to-be-captured window.
  std::unique_ptr<CaptureWindowObserver> capture_window_observer_;

  // Contains the window dimmers which dim all the root windows except
  // |current_root_|.
  base::flat_set<std::unique_ptr<WindowDimmer>> root_window_dimmers_;

  // The object to specify the cursor type.
  std::unique_ptr<CursorSetter> cursor_setter_;

  // True when dragging is in progress.
  bool is_drag_in_progress_ = false;

  // Counter used to track the number of times a user adjusts a capture region.
  // This should be reset when a user creates a new capture region, changes
  // capture sources or when a user performs a capture.
  int num_capture_region_adjusted_ = 0;

  // True if at any point during the lifetime of |this|, the capture source
  // changed. Used for metrics collection.
  bool capture_source_changed_ = false;

  // The current focused fine tune position. This changes as user tabs while a
  // in capture region mode.
  FineTunePosition focused_fine_tune_position_ = FineTunePosition::kNone;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_
