// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/accessibility/magnifier/magnifier_glass.h"
#include "ash/capture_mode/base_capture_mode_session.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_toast_controller.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/folder_selection_dialog_controller.h"
#include "ash/shell_observer.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/display/display_observer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace gfx {
class Canvas;
}  // namespace gfx

namespace views {
class BoxLayoutView;
}  // namespace views

namespace ash {

class CaptureModeBarView;
class CaptureModeController;
class CaptureModeSessionFocusCycler;
class CaptureModeSettingsView;
class CaptureWindowObserver;
class CursorSetter;
class RecordingTypeMenuView;
class UserNudgeController;
class WindowDimmer;

// TODO(http://b/366621847):  Create an API for creating action buttons that can
// be called asynchronously from `capture_mode_util`.

// Encapsulates an active capture mode session (i.e. an instance of this class
// lives as long as capture mode is active). It creates and owns the capture
// mode bar widget.
// The CaptureModeSession is a LayerOwner that owns a texture layer placed right
// beneath the layer of the bar widget. This layer is used to paint a dimming
// shield of the areas that won't be captured, and another bright region showing
// the one that will be.
class ASH_EXPORT CaptureModeSession
    : public BaseCaptureModeSession,
      public ui::LayerDelegate,
      public ui::EventHandler,
      public aura::WindowObserver,
      public display::DisplayObserver,
      public FolderSelectionDialogController::Delegate,
      public ui::ColorProviderSourceObserver {
 public:
  // Centralized place to control the events, observe windows and create the
  // capture mode needed widgets including `capture_mode_bar_widget_`,
  // `capture_label_widget_`, `recording_type_menu_widget_`, etc, on a
  // calculated root window. `active_behavior` will customize the widgets or
  // restrict certain operations.
  CaptureModeSession(CaptureModeController* controller,
                     CaptureModeBehavior* active_behavior);
  CaptureModeSession(const CaptureModeSession&) = delete;
  CaptureModeSession& operator=(const CaptureModeSession&) = delete;
  ~CaptureModeSession() override;

  // The vertical distance from the size label to the custom capture region.
  static constexpr int kSizeLabelYDistanceFromRegionDp = 8;

  // The vertical distance of the capture button from the capture region, if the
  // capture button does not fit inside the capture region.
  static constexpr int kCaptureButtonDistanceFromRegionDp = 24;

  views::Widget* capture_label_widget() { return capture_label_widget_.get(); }
  views::Widget* capture_mode_settings_widget() {
    return capture_mode_settings_widget_.get();
  }
  views::Widget* search_results_panel_widget() {
    return search_results_panel_widget_.get();
  }
  bool is_selecting_region() const { return is_selecting_region_; }
  CaptureModeToastController* capture_toast_controller() {
    return &capture_toast_controller_;
  }

  // Called when a user toggles the capture source or capture type to announce
  // an accessibility alert. If `trigger_now` is true, it will announce
  // immediately; otherwise, it will trigger another alert asynchronously with
  // the alert.
  void A11yAlertCaptureSource(bool trigger_now);

  // Called when the settings menu is toggled. If `by_key_event` is true, it
  // means that the settings menu is being opened or closed as a result of a key
  // event (e.g. pressing the space bar) on the settings button.
  void SetSettingsMenuShown(bool shown, bool by_key_event = false);

  // Opens the dialog that lets users pick the folder to which they want the
  // captured files to be saved.
  void OpenFolderSelectionDialog();

  // Returns true if we are currently in video recording countdown animation.
  bool IsInCountDownAnimation() const;

  // Returns true if `capture_window_observer_` exists and the capture bar is
  // anchored to a pre-selected window.
  bool IsBarAnchoredToWindow() const;

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
      std::optional<gfx::Point> cursor_screen_location = std::nullopt);

  // Sets the correct screen bounds on the `capture_mode_bar_widget_` based on
  // the `current_root_`, potentially moving the bar to a new display if
  // `current_root_` is different`.
  void RefreshBarWidgetBounds();

  // BaseCaptureModeSession:
  views::Widget* GetCaptureModeBarWidget() override;
  aura::Window* GetSelectedWindow() const override;
  void SetPreSelectedWindow(aura::Window* pre_selected_window) override;
  void OnCaptureSourceChanged(CaptureModeSource new_source) override;
  void OnCaptureTypeChanged(CaptureModeType new_type) override;
  void OnRecordingTypeChanged() override;
  void OnAudioRecordingModeChanged() override;
  void OnDemoToolsSettingsChanged() override;
  void OnWaitingForDlpConfirmationStarted() override;
  void OnWaitingForDlpConfirmationEnded(bool reshow_uis) override;
  void ReportSessionHistograms() override;
  void StartCountDown(base::OnceClosure countdown_finished_callback) override;
  void OnCaptureFolderMayHaveChanged() override;
  void OnDefaultCaptureFolderSelectionChanged() override;
  bool CalculateCameraPreviewTargetVisibility() const override;
  void OnCameraPreviewDragStarted() override;
  void OnCameraPreviewDragEnded(const gfx::Point& screen_location,
                                bool is_touch) override;
  void OnCameraPreviewBoundsOrVisibilityChanged(
      bool capture_surface_became_too_small,
      bool did_bounds_or_visibility_change) override;
  void OnCameraPreviewDestroyed() override;
  void MaybeDismissUserNudgeForever() override;
  void MaybeChangeRoot(aura::Window* new_root,
                       bool root_window_will_shutdown) override;
  std::set<aura::Window*> GetWindowsToIgnoreFromWidgets() override;
  void ShowSearchResultsPanel(const gfx::ImageSkia& image) override;
  void AddActionButton(views::Button::PressedCallback callback,
                       std::u16string text,
                       const gfx::VectorIcon* icon) override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // FolderSelectionDialogController::Delegate:
  void OnFolderSelected(const base::FilePath& path) override;
  void OnSelectionWindowAdded() override;
  void OnSelectionWindowClosed() override;

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override;

 private:
  friend class CaptureModeSettingsTestApi;
  friend class CaptureModeSessionFocusCycler;
  friend class CaptureModeSessionTestApi;
  friend class CaptureModeTestApi;
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

  // Called when switching a capture type from another capture type.
  void A11yAlertCaptureType();

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

  // If possible, this recreates and shows the nudge that alerts the user about
  // the new folder selection settings. The nudge will be created on top of the
  // the settings button on the capture mode bar.
  void MaybeCreateUserNudge();

  // Called to accept and trigger a capture operation. This happens e.g. when
  // the user hits enter, selects a window/display to capture, or presses on the
  // record button in the capture label view.
  void DoPerformCapture();

  // Called when the drop-down button in the `capture_label_widget_` is pressed
  // which toggles the recording type menu on and off.
  void OnRecordingTypeDropDownButtonPressed(const ui::Event& event);

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
      std::optional<gfx::Point> cursor_screen_location = std::nullopt);

  // Schedules a paint on the region and enough inset around it so that the
  // shadow, affordance circles, etc. are all repainted.
  void RepaintRegion();

  // Selects a default region that is centered and whose size is a ratio of the
  // root window bounds. Called when the space key is pressed.
  void SelectDefaultRegion();

  // Updates the region either horizontally or vertically. Called when the arrow
  // keys are pressed. `event_flags` are the flags from the event that triggers
  // these calls. Different modifiers will move the region more or less.
  void UpdateRegionForArrowKeys(ui::KeyboardCode key_code, int event_flags);

  // Called when the parent container of camera preview may need to be updated.
  void MaybeReparentCameraPreviewWidget();

  // Called at the beginning or end of the drag of capture region to update the
  // camera preview's bounds and visibility.
  void MaybeUpdateCameraPreviewBounds();

  // Creates or distroys the recording type menu widget based on the given
  // `shown` value. If `by_key_event` is true, it means that the recording type
  // menu is being opened or closed as a result of a key event (e.g. pressing
  // the space bar) on the recording type drop down button.
  void SetRecordingTypeMenuShown(bool shown, bool by_key_event = false);

  // Returns true if the given `screen_location` is on the drop down button in
  // the `capture_label_widget_` which when clicked opens the recording type
  // menu.
  bool IsPointOnRecordingTypeDropDownButton(
      const gfx::Point& screen_location) const;

  // Updates the availability or bounds of the recording type menu widget
  // according to the current state.
  void MaybeUpdateRecordingTypeMenu();

  // Returns true if there is a selected window and it is the topmost
  // capturable window at `screen_point`. Returns false otherwise.
  bool IsPointOverSelectedWindow(const gfx::Point& screen_point) const;

  // Creates the the action container widget if it wasn't previously created,
  // and updates the widget's bounds.
  void UpdateActionContainerWidget();

  // Updates the action container widget's bounds.
  void UpdateActionContainerWidgetBounds();

  // Calculates the targeted action container widget bounds in screen
  // coordinates.
  gfx::Rect CalculateActionContainerWidgetBounds() const;

  // Removes any existing action buttons from `action_container_view_` if the
  // `action_container_widget_` exists,
  void RemoveAllActionButtons();

  // BaseCaptureModeSession:
  void InitInternal() override;
  void ShutdownInternal() override;

  views::UniqueWidgetPtr capture_mode_bar_widget_ =
      std::make_unique<views::Widget>();

  // The content view of the above widget and owned by its views hierarchy.
  raw_ptr<CaptureModeBarView, DanglingUntriaged> capture_mode_bar_view_ =
      nullptr;

  views::UniqueWidgetPtr capture_mode_settings_widget_;

  // The content view of the above widget and owned by its views hierarchy.
  raw_ptr<CaptureModeSettingsView, DanglingUntriaged>
      capture_mode_settings_view_ = nullptr;

  // Widget which displays capture region size during a region capture session.
  views::UniqueWidgetPtr dimensions_label_widget_;

  // Widget that shows an optional icon and a message in the middle of the
  // screen or in the middle of the capture region and prompts the user what to
  // do next. The icon and message can be different in different capture type
  // and source and can be empty in some cases. And in video capture mode, when
  // starting capturing, the widget will transform into a 3-second countdown
  // timer.
  views::UniqueWidgetPtr capture_label_widget_;
  raw_ptr<CaptureLabelView, DanglingUntriaged> capture_label_view_ = nullptr;

  // TODO(hewer): Check if we can migrate these widgets to `SunfishBehavior`.
  views::UniqueWidgetPtr action_container_widget_;
  raw_ptr<views::BoxLayoutView> action_container_view_ = nullptr;

  // Widget that hosts the recording type menu, from which the user can pick the
  // desired recording format type.
  views::UniqueWidgetPtr recording_type_menu_widget_;
  raw_ptr<RecordingTypeMenuView, DanglingUntriaged> recording_type_menu_view_ =
      nullptr;

  // Contains `SearchResultsPanel` as its contents view.
  // TODO(b/362772923): Determine whether we need to move
  // `search_results_panel_widget_` to `CaptureModeController`.
  std::unique_ptr<views::Widget> search_results_panel_widget_;

  // Magnifier glass used during a region capture session.
  MagnifierGlass magnifier_glass_;

  // True if all UIs (cursors, widgets, and paintings on the layer) of the
  // capture mode session is visible.
  bool is_all_uis_visible_ = true;

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
  std::optional<bool> old_mouse_warp_status_;

  // Observer to observe the current selected to-be-captured window.
  std::unique_ptr<CaptureWindowObserver> capture_window_observer_;

  // Observer to observe the parent container `kShellWindowId_MenuContainer`.
  std::unique_ptr<ParentContainerObserver> parent_container_observer_;

  // Contains the window dimmers which dim all the root windows except
  // |current_root_|.
  base::flat_set<std::unique_ptr<WindowDimmer>> root_window_dimmers_;

  // The object to specify the cursor type.
  std::unique_ptr<CursorSetter> cursor_setter_;

  // Counter used to track the number of times a user adjusts a capture region.
  // This should be reset when a user creates a new capture region, changes
  // capture sources or when a user performs a capture.
  int num_capture_region_adjusted_ = 0;

  // True if at any point during the lifetime of |this|, the capture source
  // changed. Used for metrics collection.
  bool capture_source_changed_ = false;

  // The window which had input capture prior to entering the session. It may be
  // null if no such window existed.
  raw_ptr<aura::Window, DanglingUntriaged> input_capture_window_ = nullptr;

  // The display observer between init/shutdown.
  std::optional<display::ScopedDisplayObserver> display_observer_;

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
