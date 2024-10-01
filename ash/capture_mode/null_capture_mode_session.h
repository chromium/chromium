// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_NULL_CAPTURE_MODE_SESSION_H_
#define ASH_CAPTURE_MODE_NULL_CAPTURE_MODE_SESSION_H_

#include "ash/capture_mode/base_capture_mode_session.h"
#include "ui/aura/window_tracker.h"

namespace ash {

// A null object version of CaptureModeSession that is used to skip the
// countdown and user input. It does not create a capture bar widget, and does
// nothing for capture bar-related operations.
class ASH_EXPORT NullCaptureModeSession : public BaseCaptureModeSession {
 public:
  NullCaptureModeSession(CaptureModeController* controller,
                         CaptureModeBehavior* active_behavior);
  NullCaptureModeSession(const NullCaptureModeSession&) = delete;
  NullCaptureModeSession& operator=(const NullCaptureModeSession&) = delete;
  ~NullCaptureModeSession() override = default;

  // NullCaptureModeSession:
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

 private:
  // CaptureModeSession:
  void InitInternal() override;
  void ShutdownInternal() override;

  aura::WindowTracker selected_window_tracker_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_NULL_CAPTURE_MODE_SESSION_H_
