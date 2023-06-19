// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_

#include "ash/constants/ash_features.h"
#include "ash/style/rounded_container.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector_observer.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class Action;
class DisplayOverlayController;
class EditLabels;
class NameTag;

// ButtonOptionsMenu displays action's type, input binding(s) and name and it
// can modify these information. It shows up upon clicking an action's touch
// point.
//
// View looks like this:
// +----------------------------------+
// ||icon|  |"Button options"|  |icon||
// |----------------------------------|
// ||"Key assignment"|                |
// |----------------------------------|
// |  |feature_tile|  |feature_title| |
// |  |            |  |             | |
// |----------------------------------|
// ||"Selected key"       |key labels||
// ||"key"                            |
// |----------------------------------|
// ||"Button label"                 > |
// ||"Unassigned"                     |
// +----------------------------------+
class ButtonOptionsMenu : public views::View, public TouchInjectorObserver {
 public:
  static ButtonOptionsMenu* Show(DisplayOverlayController* controller,
                                 Action* action);

  ButtonOptionsMenu(DisplayOverlayController* controller, Action* action);
  ButtonOptionsMenu(const ButtonOptionsMenu&) = delete;
  ButtonOptionsMenu& operator=(const ButtonOptionsMenu&) = delete;
  ~ButtonOptionsMenu() override;

  Action* action() const { return action_; }

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditLabelTest;

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

  // View position calculation. Make it virtual for unit test.
  virtual void CalculatePosition();
  // Calculates triangle wedge offset.
  int CalculateActionOffset(int height);

  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize() const override;

  // TouchInjectorObserver:
  void OnActionRemoved(const Action& action) override;
  void OnActionTypeChanged(const Action& action,
                           const Action& new_action) override;
  void OnActionUpdated(const Action& action) override;

  // DisplayOverlayController owns this class, no need to deallocate.
  const raw_ptr<DisplayOverlayController> controller_ = nullptr;
  const raw_ptr<Action, DanglingUntriaged> action_ = nullptr;

  raw_ptr<EditLabels> labels_view_ = nullptr;
  raw_ptr<NameTag> labels_name_tag_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_
