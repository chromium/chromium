// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_TEST_API_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_TEST_API_H_

#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "base/memory/raw_ptr.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class BaseCaptureModeSession;
class CaptureLabelView;
class CaptureModeBarView;
class CaptureModeSettingsView;
class MagnifierGlass;
class PillButton;
class RecordingTypeMenuView;
class UserNudgeController;

// Wrapper for CaptureModeSession that exposes internal state to test functions.
class CaptureModeSessionTestApi {
 public:
  CaptureModeSessionTestApi();
  explicit CaptureModeSessionTestApi(BaseCaptureModeSession* session);

  CaptureModeSessionTestApi(CaptureModeSessionTestApi&) = delete;
  CaptureModeSessionTestApi& operator=(CaptureModeSessionTestApi&) = delete;
  ~CaptureModeSessionTestApi() = default;

  CaptureModeBarView* GetCaptureModeBarView();

  CaptureModeSettingsView* GetCaptureModeSettingsView();

  CaptureLabelView* GetCaptureLabelView();

  views::Label* GetCaptureLabelInternalView();

  RecordingTypeMenuView* GetRecordingTypeMenuView();

  views::Widget* GetCaptureModeSettingsWidget();

  views::Widget* GetCaptureLabelWidget();

  views::Widget* GetRecordingTypeMenuWidget();

  views::Widget* GetDimensionsLabelWidget();

  UserNudgeController* GetUserNudgeController();

  MagnifierGlass& GetMagnifierGlass();

  bool IsUsingCustomCursor(CaptureModeType type);

  CaptureModeSessionFocusCycler::FocusGroup GetCurrentFocusGroup();

  size_t GetCurrentFocusIndex();

  CaptureModeSessionFocusCycler::HighlightableWindow* GetHighlightableWindow(
      aura::Window* window);

  CaptureModeSessionFocusCycler::HighlightableView* GetCurrentFocusedView();

  // Returns false if `current_focus_group_` equals to `kNone` which means
  // there's no focus on any focus group for now. Otherwise, returns true;
  bool HasFocus();

  bool IsFolderSelectionDialogShown();

  // Returns true if all UIs (cursors, widgets, and paintings on the layer) of
  // the capture mode session are visible.
  bool AreAllUisVisible();

  gfx::Rect GetSelectedWindowTargetBounds();

  // A vector of the current action buttons for a Sunfish session. Will return
  // an empty vector if there are no buttons or there is no selected region.
  std::vector<PillButton*> GetActionButtons() const;

 private:
  const raw_ptr<CaptureModeSession, DanglingUntriaged> session_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_TEST_API_H_
