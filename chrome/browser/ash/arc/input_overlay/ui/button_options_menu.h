// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_

#include "ash/constants/ash_features.h"
#include "ash/style/rounded_container.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/views/view.h"

namespace arc::input_overlay {
class Action;
class DisplayOverlayController;
class ButtonOptionsMenu : public views::View {
 public:
  static ButtonOptionsMenu* Show(
      DisplayOverlayController* display_overlay_controller,
      Action* action);

  ButtonOptionsMenu(DisplayOverlayController* display_overlay_controller,
                    Action* action);
  ButtonOptionsMenu(const ButtonOptionsMenu&) = delete;
  ButtonOptionsMenu& operator=(const ButtonOptionsMenu&) = delete;
  ~ButtonOptionsMenu() override;

 private:
  void Init();

  // Add UI components.
  void AddHeader();
  void AddEditTitle();
  void AddActionEdit();
  void AddActionSelection();
  void AddActionNameLabel();

  // Functions related to buttons.
  void OnTrashButtonPressed();
  void OnDoneButtonPressed();
  void OnTapButtonPressed();
  void OnMoveButtonPressed();
  void OnButtonLabelAssignmentPressed();

  // View position calculation.
  void CalculatePosition();

  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize() const override;

  // DisplayOverlayController owns this class, no need to deallocate.
  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
  const raw_ptr<Action> action_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_
