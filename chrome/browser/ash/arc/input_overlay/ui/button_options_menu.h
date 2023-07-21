// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector_observer.h"
#include "chrome/browser/ash/arc/input_overlay/ui/arrow_container.h"

namespace ash {
class FeatureTile;
class RoundedContainer;
}  // namespace ash

namespace arc::input_overlay {

class Action;
class ActionTypeButtonGroup;
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
class ButtonOptionsMenu : public ArrowContainer, public TouchInjectorObserver {
 public:
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
  void OnButtonLabelAssignmentPressed();

  // TouchInjectorObserver:
  void OnActionRemoved(const Action& action) override;
  void OnActionTypeChanged(Action* action, Action* new_action) override;
  void OnActionInputBindingUpdated(const Action& action) override;
  void OnActionNameUpdated(const Action& action) override;

  // DisplayOverlayController owns this class, no need to deallocate.
  const raw_ptr<DisplayOverlayController> controller_ = nullptr;
  raw_ptr<Action, DanglingUntriaged> action_ = nullptr;

  raw_ptr<ActionTypeButtonGroup> button_group_ = nullptr;
  raw_ptr<ash::RoundedContainer> action_edit_container_ = nullptr;
  raw_ptr<EditLabels, DisableDanglingPtrDetection> labels_view_ = nullptr;
  raw_ptr<NameTag> key_name_tag_ = nullptr;
  raw_ptr<ash::FeatureTile> action_name_tile_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_
