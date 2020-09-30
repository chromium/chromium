// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/button/button.h"
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
                                      public ui::EventHandler,
                                      public views::ButtonListener {
 public:
  // Creates the bar widget on the given |root| window.
  CaptureModeSession(CaptureModeController* controller, aura::Window* root);
  CaptureModeSession(const CaptureModeSession&) = delete;
  CaptureModeSession& operator=(const CaptureModeSession&) = delete;
  ~CaptureModeSession() override;

  // The vertical distance from the size label to the custom capture region.
  static constexpr int kSizeLabelYDistanceFromRegionDp = 8;

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

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::Widget* dimensions_label_widget() {
    return dimensions_label_widget_.get();
  }

 private:
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

  // Handles updating the select region UI.
  void OnLocatedEventPressed(const gfx::Point& location_in_root, bool is_touch);
  void OnLocatedEventDragged(const gfx::Point& location_in_root);
  void OnLocatedEventReleased(const gfx::Point& location_in_root);

  // Updates the capture region and the capture region widgets.
  void UpdateCaptureRegion(const gfx::Rect& new_capture_region);

  // Updates the widgets that are used to display text/icons while selecting a
  // capture region. They are not visible during fullscreen or window capture,
  // and some are only visible during certain phases of region capture. This
  // will create or destroy the widgets as needed.
  void UpdateCaptureRegionWidgets();

  // Creates |dimensions_label_widget_| if it does not exist and then set its
  // content view to the size label view.
  void MaybeCreateAndUpdateDimensionsLabelWidget();

  // Updates the bounds of |dimensions_label_widget_| relative to the current
  // capture region. Both |dimensions_label_widget_| and its content view must
  // exist.
  void UpdateDimensionsLabelBounds();

  // Creates |capture_button_widget_| if it does not exist and then set its
  // content view to the capture button view.
  void CreateCaptureButtonWidget();

  // Populates |capture_button_widget_| with its content view which displays the
  // capture button. |capture_button_widget_| must exist.
  void UpdateCaptureButtonContents();

  // Updates the bounds of |capture_button_widget_| relative to the current
  // capture region. Does nothing if |capture_button_widget_| does not exist.
  void UpdateCaptureButtonBounds();

  // Retrieves the anchor points on the current selected region associated with
  // |position|. The anchor points are described as the points that do not
  // change when resizing the capture region while dragging one of the drag
  // affordances. There is one anchor point if |position| is a vertex, and two
  // anchor points if |position| is an edge.
  std::vector<gfx::Point> GetAnchorPointsForPosition(FineTunePosition position);

  CaptureModeController* const controller_;

  // The current root window on which the capture session is active, which may
  // change if the user warps the cursor to another display in some situations.
  aura::Window* current_root_;

  views::Widget capture_mode_bar_widget_;

  // The content view of the above widget and owned by its views hierarchy.
  CaptureModeBarView* capture_mode_bar_view_;

  // Widgets which display text and icons during a region capture session.
  std::unique_ptr<views::Widget> dimensions_label_widget_;
  std::unique_ptr<views::Widget> capture_button_widget_;

  // Stores the data needed to select a region during a region capture session.
  // There are two phases for a region capture session. The select phase, where
  // the user can quickly select a region and the fine tune phase, where the
  // user can reposition and resize the region with a lot of accuracy.
  bool is_select_phase_ = true;
  // The location of the last press and drag events.
  gfx::Point initial_location_in_root_;
  gfx::Point previous_location_in_root_;
  // The position of the last press event during the fine tune phase drag.
  FineTunePosition fine_tune_position_;
  // The points that do not change during a fine tune resize. This is empty
  // when |fine_tune_position_| is kNone or kCenter, or if there is no drag
  // underway.
  std::vector<gfx::Point> anchor_points_;

  // Caches the old status of mouse warping before the session started to be
  // restored at the end.
  bool old_mouse_warp_status_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_
