// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_

#include <memory>
#include <vector>

#include "ash/accessibility/magnifier/magnifier_glass.h"
#include "ash/ash_export.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_toast_controller.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/folder_selection_dialog_controller.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell_observer.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/display/display_observer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ash {

class CaptureModeBarView;
class CaptureModeController;
class CaptureModeSessionFocusCycler;
class CaptureModeSettingsView;
class CaptureWindowObserver;
class UserNudgeController;
class WindowDimmer;

// Encapsulates an active capture mode session (i.e. an instance of this class
// lives as long as capture mode is active). It creates and owns the capture
// mode bar widget.
// The CaptureModeSession is a LayerOwner that owns a texture layer placed right
// beneath the layer of the bar widget. This layer is used to paint a dimming
// shield of the areas that won't be captured, and another bright region showing
// the one that will be.
class ASH_EXPORT CaptureModeSession
    : public ui::LayerOwner,
      public ui::LayerDelegate,
      public ui::EventHandler,
      public TabletModeObserver,
      public aura::WindowObserver,
      public display::DisplayObserver,
      public FolderSelectionDialogController::Delegate,
      public ShellObserver {
 public:
  // Creates the bar widget on a calculated root window. |projector_mode|
  // specifies whether this session was started for the projector workflow.
  CaptureModeSession(CaptureModeController* controller, bool projector_mode);
  CaptureModeSession(const CaptureModeSession&) = delete;
  CaptureModeSession& operator=(const CaptureModeSession&) = delete;
  ~CaptureModeSession() override;

  // The vertical distance from the size label to the custom capture region.
  static constexpr int kSizeLabelYDistanceFromRegionDp = 8;

  // The vertical distance of the capture button from the capture region, if the
  // capture button does not fit inside the capture region.
  static constexpr int kCaptureButtonDistanceFromRegionDp = 24;

  aura::Window* current_root() const { return current_root_; }
  views::Widget* capture_mode_bar_widget() {
    return capture_mode_bar_widget_.get();
  }
  views::Widget* capture_label_widget() { return capture_label_widget_.get(); }
  views::Widget* capture_mode_settings_widget() {
    return capture_mode_settings_widget_.get();
  }
  bool is_in_projector_mode() const { return is_in_projector_mode_; }
  void set_can_exit_on_escape(bool value) { can_exit_on_escape_ = value; }
  bool is_selecting_region() const { return is_selecting_region_; }
  bool is_drag_in_progress() const { return is_drag_in_progress_; }
  void set_a11y_alert_on_session_exit(bool value) {
    a11y_alert_on_session_exit_ = value;
  }
  bool is_shutting_down() const { return is_shutting_down_; }
  void set_is_stopping_to_start_video_recording(bool value) {
    is_stopping_to_start_video_recording_ = value;
  }
  CaptureModeToastController* capture_toast_controller() {
    return &capture_toast_controller_;
  }

  // Initializes the capture mode session. This should be called right after the
  // object is created.
  void Initialize();

  // Shuts down the capture mode session. This should be called right before the
  // object is destroyed.
  void Shutdown();

  // Gets the current window selected for |kWindow| capture source. Returns
  // nullptr if no window is available for selection.
  aura::Window* GetSelectedWindow() const;

  // Called when a user toggles the capture source or capture type to announce
  // an accessibility alert. If `trigger_now` is true, it will announce
  // immediately; otherwise, it will trigger another alert asynchronously with
  // the alert.
  void A11yAlertCaptureSource(bool trigger_now);

  // Called when switching a capture type from another capture type.
  void A11yAlertCaptureType();

  // Called when either the capture source, type, or recording type changes.
  void OnCaptureSourceChanged(CaptureModeSource new_source);
  void OnCaptureTypeChanged(CaptureModeType new_type);
  void OnRecordingTypeChanged();

  // When performing capture, or at the end of the 3-second count down, the DLP
  // manager is checked for any restricted content. The DLP manager may choose
  // to show a system-modal dialog to warn the user about some content they're
  // about to capture. This function is called to prepare for this case by
  // hiding all the capture mode UIs and stopping the consumption of input
  // events, so users can interact with the dialog.
  void OnWaitingForDlpConfirmationStarted();

  // This function is called when the DLP manager replies back. If `reshow_uis`
  // is true, then we'll undo the hiding of all the capture mode UIs done in
  // OnWaitingForDlpConfirmationStarted().
  void OnWaitingForDlpConfirmationEnded(bool reshow_uis);

  // Called when the settings menu is toggled.
  void SetSettingsMenuShown(bool shown);

  // Called when the user performs a capture. Records histograms related to this
  // session.
  void ReportSessionHistograms();

  // Called when starting 3-seconds count down before recording video.
  void StartCountDown(base::OnceClosure countdown_finished_callback);

  // Opens the dialog that lets users pick the folder to which they want the
  // captured files to be saved.
  void OpenFolderSelectionDialog();

  // Returns true if we are currently in video recording countdown animation.
  bool IsInCountDownAnimation() const;

  // Called when the capture folder may have changed to update the set of menu
  // options in the settings menu and resize it so that it fits its potentially
  // new contents.
  void OnCaptureFolderMayHaveChanged();

  // Called when we change the setting to force-use the default downloads folder
  // as the save folder.
  void OnDefaultCaptureFolderSelectionChanged();

  // Returns the current parent window for the on-capture-surface widgets such
  // as `CaptureModeCameraController::camera_preview_widget_` and
  // `CaptureModeDemoToolsController::demo_tools_widget_` when capture mode
  // session is active.
  aura::Window* GetOnCaptureSurfaceWidgetParentWindow() const;

  // Returns the confine bounds for the on-capture-surface widgets (such as the
  // camera preview and demo tools widgets) when capture session is active.
  gfx::Rect GetCaptureSurfaceConfineBounds() const;

  // Returns the in-session target value that should be used for the visibility
  // of the camera preview (if any). During the session, things like dragging
  // the user region may affect the camera preview's visibility, and hence this
  // function should be consulted.
  bool CalculateCameraPreviewTargetVisibility() const;

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

  // ShellObserver:
  void OnRootWindowWillShutdown(aura::Window* root_window) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // FolderSelectionDialogController::Delegate:
  void OnFolderSelected(const base::FilePath& path) override;
  void OnSelectionWindowAdded() override;
  void OnSelectionWindowClosed() override;

  // Updates the current cursor depending on current |location_in_screen| and
  // current capture type and source. |is_touch| is used when calculating fine
  // tune position in region capture mode. We'll have a larger hit test region
  // for the touch events than the mouse events.
  void UpdateCursor(const gfx::Point& location_in_screen, bool is_touch);

  // Highlights the give |window| for keyboard navigation
  // events (tabbing through windows in capture window mode).
  void HighlightWindowForTab(aura::Window* window);

  // Called when the settings view has been updated, its bounds may need to be
  // updated correspondingly.
  void MaybeUpdateSettingsBounds();

  // Called when opacity of capture UIs (capture bar, capture label) may need to
  // be updated. For example, when camera preview is created, destroyed,
  // reparented, display metrics change or located events enter / exit / move
  // on capture UI.
  void MaybeUpdateCaptureUisOpacity(
      absl::optional<gfx::Point> cursor_screen_location = absl::nullopt);

  void OnCameraPreviewDragStarted();
  void OnCameraPreviewDragEnded(const gfx::Point& screen_location,
                                bool is_touch);

  // Called every time when camera preview is updated.
  // `capture_surface_became_too_small` indicates whether the camera preview
  // becomes invisible is due to the capture surface becoming too small.
  // `did_bounds_or_visibility_change` determines whether the capture UIs'
  // opacity should be updated.
  void OnCameraPreviewBoundsOrVisibilityChanged(
      bool capture_surface_became_too_small,
      bool did_bounds_or_visibility_change);

  void OnCameraPreviewDestroyed();

 private:
  friend class CaptureModeSettingsTestApi;
  friend class CaptureModeSessionFocusCycler;
  friend class CaptureModeSessionTestApi;
  friend class CaptureModeTestApi;
  class CursorSetter;
  class ParentContainerObserver;

  enum class CaptureLabelAnimation {
    // No animation on the capture label.
    kNone,
    // The animation on the capture label when the user has finished selecting a
    // region and is moving to the fine tune phase.
    kRegionPhaseChange,
    // The animation on the capture label when the user has clicked record and
    // the capture label animates into a countdown label.
    kCountdownStart,
  };

  // Returns a list of all the currently available widgets that are owned by
  // this session.
  std::vector<views::Widget*> GetAvailableWidgets();

  // All UI elements, cursors, widgets and paintings on the session's layer can
  // be either shown or hidden with the below functions.
  void HideAllUis();
  void ShowAllUis();

  // Called by `ShowAllUis` for each widget. Returns true if the given `widget`
  // could be shown, otherwise, returns false.
  bool CanShowWidget(views::Widget* widget) const;

  // Sets the correct screen bounds on the `capture_mode_bar_widget_` based on
  // the `current_root_`, potentially moving the bar to a new display if
  // `current_root_` is different`.
  void RefreshBarWidgetBounds();

  // If possible, this recreates and shows the nudge that alerts the user about
  // the new folder selection settings. The nudge will be created on top of the
  // the settings button on the capture mode bar.
  void MaybeCreateUserNudge();

  // If there's a user nudge currently showing, it will be dismissed forever,
  // and will no longer be shown to the user.
  void MaybeDismissUserNudgeForever();

  // Called to accept and trigger a capture operation. This happens e.g. when
  // the user hits enter, selects a window/display to capture, or presses on the
  // record button in the capture label view.
  void DoPerformCapture();

  // Called when the drop-down button in the `capture_label_widget_` is pressed
  // which toggles the recording type menu on and off.
  void OnRecordingTypeDropDownButtonPressed();

  // Gets the bounds of current window selected for |kWindow| capture source.
  gfx::Rect GetSelectedWindowBounds() const;

  // Ensures that the bar widget is on top of everything, and the overlay (which
  // is the |layer()| of this class that paints the capture region) is stacked
  // below the bar.
  void RefreshStackingOrder();

  // Paints the current capture region depending on the current capture source.
  void PaintCaptureRegion(gfx::Canvas* canvas);

  // Helper to unify mouse/touch events. Forwards events to the three below
  // functions and they are located on |capture_button_widget_|. Blocks events
  // from reaching other handlers, unless the event is located on
  // |capture_mode_bar_widget_|. |is_touch| signifies this is a touch event, and
  // we will use larger hit targets for the drag affordances.
  void OnLocatedEvent(ui::LocatedEvent* event, bool is_touch);

  // Returns the fine tune position that corresponds to the given
  // `location_in_screen`.
  FineTunePosition GetFineTunePosition(const gfx::Point& location_in_screen,
                                       bool is_touch) const;

  // Handles updating the select region UI.
  void OnLocatedEventPressed(const gfx::Point& location_in_root, bool is_touch);
  void OnLocatedEventDragged(const gfx::Point& location_in_root);
  void OnLocatedEventReleased(const gfx::Point& location_in_root);

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

  // Updates the capture label widget's icon/text and bounds. The capture label
  // widget may be animated depending on |animation_type|.
  void UpdateCaptureLabelWidget(CaptureLabelAnimation animation_type);

  // Updates the capture label widget's bounds. The capture label
  // widget may be animated depending on |animation_type|.
  void UpdateCaptureLabelWidgetBounds(CaptureLabelAnimation animation_type);

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

  // Returns true if we're using custom image capture icon when |type| is
  // kImage or using custom video capture icon when |type| is kVideo.
  bool IsUsingCustomCursor(CaptureModeType type) const;

  // Ensure the user region in |controller_| is within the bounds of the root
  // window. This is called when creating |this| or when the display bounds have
  // changed.
  void ClampCaptureRegionToRootWindowSize();

  // Ends a region selection. Cleans up internal state and updates the cursor,
  // capture UIs' opacity and magnifier glass. The `cursor_screen_location`
  // could not be provided in some use cases, for example the capture region is
  // updated because of the display metrics are changed. When
  // `cursor_screen_location` is not provived, we will try to get the screen
  // location of the mouse.
  void EndSelection(
      absl::optional<gfx::Point> cursor_screen_location = absl::nullopt);

  // Schedules a paint on the region and enough inset around it so that the
  // shadow, affordance circles, etc. are all repainted.
  void RepaintRegion();

  // Selects a default region that is centered and whose size is a ratio of the
  // root window bounds. Called when the space key is pressed.
  void SelectDefaultRegion();

  // Updates the region either horizontally or vertically. Called when the arrow
  // keys are pressed. |event_flags| are the flags from the event that triggers
  // these calls. Different modifiers will move the region more or less.
  void UpdateRegionHorizontally(bool left, int event_flags);
  void UpdateRegionVertically(bool up, int event_flags);

  // Called when the parent container of camera preview may need to be updated.
  void MaybeReparentCameraPreviewWidget();

  // Called at the beginning or end of the drag of capture region to update the
  // camera preview's bounds and visibility.
  void MaybeUpdateCameraPreviewBounds();

  // Creates or distroys the recording type menu widget based on the given
  // `shown` value.
  void SetRecordingTypeMenuShown(bool shown);

  // Returns true if the given `screen_location` is on the drop down button in
  // the `capture_label_widget_` which when clicked opens the recording type
  // menu.
  bool IsPointOnRecordingTypeDropDownButton(
      const gfx::Point& screen_location) const;

  // Updates the availability or bounds of the recording type menu widget
  // according to the current state.
  void MaybeUpdateRecordingTypeMenu();

  CaptureModeController* const controller_;

  // The current root window on which the capture session is active, which may
  // change if the user warps the cursor to another display in some situations.
  aura::Window* current_root_;

  views::UniqueWidgetPtr capture_mode_bar_widget_ =
      std::make_unique<views::Widget>();

  // The content view of the above widget and owned by its views hierarchy.
  CaptureModeBarView* capture_mode_bar_view_ = nullptr;

  views::UniqueWidgetPtr capture_mode_settings_widget_;

  // The content view of the above widget and owned by its views hierarchy.
  CaptureModeSettingsView* capture_mode_settings_view_ = nullptr;

  // Widget which displays capture region size during a region capture session.
  views::UniqueWidgetPtr dimensions_label_widget_;

  // Widget that shows an optional icon and a message in the middle of the
  // screen or in the middle of the capture region and prompts the user what to
  // do next. The icon and message can be different in different capture type
  // and source and can be empty in some cases. And in video capture mode, when
  // starting capturing, the widget will transform into a 3-second countdown
  // timer.
  views::UniqueWidgetPtr capture_label_widget_;
  CaptureLabelView* capture_label_view_ = nullptr;

  // Widget that hosts the recording type menu, from which the user can pick the
  // desired recording format type.
  views::UniqueWidgetPtr recording_type_menu_widget_;

  // Magnifier glass used during a region capture session.
  MagnifierGlass magnifier_glass_;

  // True if all UIs (cursors, widgets, and paintings on the layer) of the
  // capture mode session is visible.
  bool is_all_uis_visible_ = true;

  // Whether this session was started from a projector workflow.
  const bool is_in_projector_mode_ = false;

  // Whether pressing the escape key can exit the session. This is used when we
  // find capturable content at the end of the 3-second count down, but we need
  // to do some extra asynchronous operations before we start the actual
  // recording. At this point we don't want the user to be able to bail out.
  bool can_exit_on_escape_ = true;

  // Stores the data needed to select a region during a region capture session.
  // This variable indicates if the user is currently selecting a region to
  // capture, it will be true when the first mouse/touch presses down and will
  // remain true until the mouse/touch releases up. After that, if the capture
  // region is non empty, the capture session will enter the fine tune phase,
  // where the user can reposition and resize the region with a lot of accuracy.
  bool is_selecting_region_ = false;

  // True when a located pressed event is received outside the bounds of a
  // present settings menu widget. This event will be used to dismiss the
  // settings menu and all future located events up to and including the
  // released event will be ignored (i.e. will not be used to update the capture
  // region, perform capture ... etc.).
  bool ignore_located_events_ = false;

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
  absl::optional<bool> old_mouse_warp_status_;

  // Observer to observe the current selected to-be-captured window.
  std::unique_ptr<CaptureWindowObserver> capture_window_observer_;

  // Observer to observe the parent container `kShellWindowId_MenuContainer`.
  std::unique_ptr<ParentContainerObserver> parent_container_observer_;

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

  // The window which had input capture prior to entering the session. It may be
  // null if no such window existed.
  aura::Window* input_capture_window_ = nullptr;

  // False only when we end the session to start recording.
  bool a11y_alert_on_session_exit_ = true;

  // The display observer between init/shutdown.
  absl::optional<display::ScopedDisplayObserver> display_observer_;

  // True once Shutdown() is called.
  bool is_shutting_down_ = false;

  // True when the session is being stopped to start video recording, at which
  // point, it's guaranteed that recording will start and will not be blocked by
  // any errors, DLP restrictions, or any user cancellation.
  bool is_stopping_to_start_video_recording_ = false;

  // True when we ask the DLP manager to check the screen content before we
  // perform the capture.
  bool is_waiting_for_dlp_confirmation_ = false;

  // The object which handles tab focus while in a capture session.
  std::unique_ptr<CaptureModeSessionFocusCycler> focus_cycler_;

  // True if a located event should be passed to camera preview to be handled.
  bool should_pass_located_event_to_camera_preview_ = false;

  // Controls the folder selection dialog. Not null only while the dialog is
  // shown.
  std::unique_ptr<FolderSelectionDialogController>
      folder_selection_dialog_controller_;

  // Controls the user nudge animations.
  std::unique_ptr<UserNudgeController> user_nudge_controller_;

  // Controls creating, destroying or updating the visibility of the capture
  // toast.
  CaptureModeToastController capture_toast_controller_;

  base::WeakPtrFactory<CaptureModeSession> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_
