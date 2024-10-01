// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_BASE_CAPTURE_MODE_SESSION_H_
#define ASH_CAPTURE_MODE_BASE_CAPTURE_MODE_SESSION_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/shell_observer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/views/controls/button/button.h"

namespace ash {

// An interface for different kinds of capture mode sessions. This class is a
// LayerOwner and will transfer ownership of its texture layer to a recording
// if/when it starts.
//
// There are two patterns for the capture mode session:
//  - Regular capture mode, also known as `CaptureModeSession`. It creates and
//    owns UI widgets such as the capture mode bar widget, the settings menu
//    widget, etc.
//  - Null object capture mode, also known as `NullCaptureModeSession`. It
//    serves as a way to instantly start a capture by bypassing user input and
//    the countdown.
class ASH_EXPORT BaseCaptureModeSession : public ui::LayerOwner,
                                          public ShellObserver {
 public:
  BaseCaptureModeSession(CaptureModeController* controller,
                         CaptureModeBehavior* active_behavior,
                         SessionType type);
  BaseCaptureModeSession(const BaseCaptureModeSession&) = delete;
  BaseCaptureModeSession& operator=(const BaseCaptureModeSession&) = delete;
  ~BaseCaptureModeSession() override;

  SessionType session_type() const { return session_type_; }
  aura::Window* current_root() const { return current_root_; }
  bool is_drag_in_progress() const { return is_drag_in_progress_; }
  CaptureModeBehavior* active_behavior() { return active_behavior_; }
  bool is_shutting_down() const { return is_shutting_down_; }
  void set_a11y_alert_on_session_exit(bool value) {
    a11y_alert_on_session_exit_ = value;
  }
  void set_is_stopping_to_start_video_recording(bool value) {
    is_stopping_to_start_video_recording_ = value;
  }
  void set_can_exit_on_escape(bool value) { can_exit_on_escape_ = value; }

  // Initializes the capture mode session. This should be called right after the
  // object is created.
  void Initialize();

  // Shuts down the capture mode session. This should be called right before the
  // object is destroyed. Subclass specific shutdown is handled via
  // `ShutdownInternal()`.
  void Shutdown();

  // Returns the current parent window for the on-capture-surface widgets such
  // as `CaptureModeCameraController::camera_preview_widget_` and
  // `CaptureModeDemoToolsController::key_combo_widget_` when capture mode
  // session is active.
  aura::Window* GetOnCaptureSurfaceWidgetParentWindow() const;

  // Returns the confine bounds for the on-capture-surface widgets (such as the
  // camera preview widget and key combo widget) when capture session is active.
  gfx::Rect GetCaptureSurfaceConfineBounds() const;

  // Gets the CaptureModeBarWidget. Should not be called for a null session, as
  // it does not have a bar widget.
  virtual views::Widget* GetCaptureModeBarWidget() = 0;

  // Gets the current window selected for `kWindow` capture source. Returns
  // nullptr if no window is available for selection.
  virtual aura::Window* GetSelectedWindow() const = 0;

  // Sets the pre-selected window for the capture session. Once set, the window
  // can't be altered throughout the entire capture session.
  virtual void SetPreSelectedWindow(aura::Window* pre_selected_window) = 0;

  // Called when either the capture source, type, recording type, audio
  // recording mode or demo tools changes.
  virtual void OnCaptureSourceChanged(CaptureModeSource source) = 0;
  virtual void OnCaptureTypeChanged(CaptureModeType type) = 0;
  virtual void OnRecordingTypeChanged() = 0;
  virtual void OnAudioRecordingModeChanged() = 0;
  virtual void OnDemoToolsSettingsChanged() = 0;

  // When performing capture, or at the end of the 3-second count down, the DLP
  // manager is checked for any restricted content. The DLP manager may choose
  // to show a system-modal dialog to warn the user about some content they're
  // about to capture. This function is called to prepare for this case.
  virtual void OnWaitingForDlpConfirmationStarted() = 0;

  // This function is called when the DLP manager replies back.
  virtual void OnWaitingForDlpConfirmationEnded(bool reshow_uis) = 0;

  // Called when the user performs a capture. Records histograms related to this
  // session.
  virtual void ReportSessionHistograms() = 0;

  // Called when starting 3-seconds count down before recording video.
  virtual void StartCountDown(
      base::OnceClosure countdown_finished_callback) = 0;

  // Called when the capture folder may have changed as the settings menu may
  // need updating.
  virtual void OnCaptureFolderMayHaveChanged() = 0;

  // Called when we change the setting to to force-use the default downloads
  // folder as the save folder.
  virtual void OnDefaultCaptureFolderSelectionChanged() = 0;

  // Returns the in-session target value that should be used for the visibility
  // of the camera preview (if any). During the session, things like dragging
  // the user region may affect the camera preview's visibility, and hence this
  // function should be consulted.
  virtual bool CalculateCameraPreviewTargetVisibility() const = 0;

  virtual void OnCameraPreviewDragStarted() = 0;
  virtual void OnCameraPreviewDragEnded(const gfx::Point& screen_location,
                                        bool is_touch) = 0;

  // Called every time when camera preview is updated.
  // `capture_surface_became_too_small` indicates whether the camera preview
  // becomes invisible is due to the capture surface becoming too small.
  // `did_bounds_or_visibility_change` determines whether the capture UIs'
  // opacity should be updated.
  virtual void OnCameraPreviewBoundsOrVisibilityChanged(
      bool capture_surface_became_too_small,
      bool did_bounds_or_visibility_change) = 0;

  virtual void OnCameraPreviewDestroyed() = 0;

  // If there's a user nudge currently showing, it will be dismissed forever,
  // and will no longer be shown to the user.
  virtual void MaybeDismissUserNudgeForever() = 0;

  // Handles changing `root_window_`. For example, moving the mouse cursor to
  // another display, a display was removed or the game window of the
  // `kGameDashboard` session was moved to another display. Moves the capture
  // mode widgets to `new_root` depending on the capture mode source and whether
  // it was a display removal (`root_window_will_shutdown` will be true in this
  // case).
  virtual void MaybeChangeRoot(aura::Window* new_root,
                               bool root_window_will_shutdown) = 0;

  // Helper function for `GetTopMostCapturableWindowAtPoint()`. Returns the
  // native windows of the widgets associated with the session (bar widget,
  // label widget, etc.) that should be ignored as the topmost window.
  virtual std::set<aura::Window*> GetWindowsToIgnoreFromWidgets() = 0;

  // Shows (if the underlying session type supports it) the results panel with
  // the captured region as `image`.
  virtual void ShowSearchResultsPanel(const gfx::ImageSkia& image) = 0;

  // Adds an action button below the selected region during an active session.
  virtual void AddActionButton(views::Button::PressedCallback callback,
                               std::u16string text,
                               const gfx::VectorIcon* icon) = 0;

  // ShellObserver:
  void OnRootWindowWillShutdown(aura::Window* root_window) override;

 protected:
  // Gets the menu container inside |root|.
  static aura::Window* GetParentContainer(aura::Window* root);

  // Triggers a selfie camera visibility update during capture mode session on
  // capture mode type changed.
  void MaybeUpdateSelfieCamInSessionVisibility();

  // Gets the bounds of current window selected for `kWindow` capture source. It
  // can be the actual bounds for the selected window or the transformed bounds
  // if the window is in overview session.
  gfx::Rect GetSelectedWindowTargetBounds() const;

  const raw_ptr<CaptureModeController> controller_;

  // The currently active capture mode behavior for this session which will be
  // used to configure capture mode session differently with different modes.
  const raw_ptr<CaptureModeBehavior> active_behavior_;

  // Indicates whether this is a regular (real) session or a null session.
  const SessionType session_type_;

  // The current root window on which the capture session is active, which may
  // change if the user warps the cursor to another display in some situations.
  raw_ptr<aura::Window> current_root_;

  // Whether pressing the escape key can exit the session. This is used when we
  // find capturable content at the end of the 3-second count down, but we need
  // to do some extra asynchronous operations before we start the actual
  // recording. At this point we don't want the user to be able to bail out.
  bool can_exit_on_escape_ = true;

  // True when dragging is in progress.
  bool is_drag_in_progress_ = false;

  // False only when we end the session to start recording.
  bool a11y_alert_on_session_exit_ = false;

  // True once Shutdown() is called.
  bool is_shutting_down_ = false;

  // True when the session is being stopped to start video recording, at which
  // point, it's guaranteed that recording will start and will not be blocked by
  // any errors, DLP restrictions, or any user cancellation.
  bool is_stopping_to_start_video_recording_ = false;

 private:
  // Handles subclass specific setup and cleanup.
  virtual void InitInternal() = 0;
  virtual void ShutdownInternal() = 0;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_BASE_CAPTURE_MODE_SESSION_H_
