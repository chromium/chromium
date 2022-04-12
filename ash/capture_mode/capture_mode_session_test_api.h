// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_TEST_API_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_TEST_API_H_

#include "capture_mode_session_focus_cycler.h"

namespace ash {

class CaptureModeSession;
class CaptureModeBarView;
class CaptureModeSettingsView;
class UserNudgeController;
class MagnifierGlass;

// Wrapper for CaptureModeSession that exposes internal state to test functions.
class CaptureModeSessionTestApi {
 public:
  explicit CaptureModeSessionTestApi(CaptureModeSession* session);

  CaptureModeSessionTestApi(CaptureModeSessionTestApi&) = delete;
  CaptureModeSessionTestApi& operator=(CaptureModeSessionTestApi&) = delete;
  ~CaptureModeSessionTestApi() = default;

  CaptureModeBarView* GetCaptureModeBarView();

  CaptureModeSettingsView* GetCaptureModeSettingsView();

  views::Widget* GetCaptureModeSettingsWidget();

  views::Widget* GetCaptureLabelWidget();

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
  // the capture mode session is visible.
  bool IsAllUisVisible();

 private:
  CaptureModeSession* const session_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_TEST_API_H_
