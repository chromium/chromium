// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_LABEL_LIST_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_LABEL_LIST_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/ui/arrow_container.h"

namespace ash {
class RadioButtonGroup;
}  // namespace ash

namespace arc::input_overlay {

class Action;
class DisplayOverlayController;

// ButtonLabelList displays a list of action names that can be assigned to the
// current action.
//
// View looks like this:
// +----------------------------------+
// ||icon|  |"Action List"|           |
// |----------------------------------|
// ||<Action string>|                 |
// |----------------------------------|
// ||<Action string>|                 |
// |----------------------------------|
// | ...                              |
// |----------------------------------|
// ||<Action string>|                 |
// +----------------------------------+
class ButtonLabelList : public ArrowContainer {
 public:
  ButtonLabelList(DisplayOverlayController* display_overlay_controller,
                  Action* action);
  ButtonLabelList(const ButtonLabelList&) = delete;
  ButtonLabelList& operator=(const ButtonLabelList&) = delete;
  ~ButtonLabelList() override;

 private:
  // Build related views.
  void Init();
  void AddHeader();
  void AddActionLabels();

  // Handle button functions.
  void OnActionLabelPressed();
  void OnBackButtonPressed();

  // DisplayOverlayController owns this class, no need to deallocate.
  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
  raw_ptr<Action> action_ = nullptr;
  raw_ptr<ash::RadioButtonGroup> button_group_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_LABEL_LIST_H_
