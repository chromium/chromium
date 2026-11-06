// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector_observer.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {
class IconButton;
}  // namespace ash

namespace gfx {
class Canvas;
class Size;
}  // namespace gfx

namespace views {
class Label;
}  // namespace views

namespace arc::input_overlay {

class Action;
class ActionEditView;
class ActionTypeButtonGroup;
class DisplayOverlayController;

// ButtonOptionsMenu displays action's type, input binding(s) and name and it
// can modify these information. It shows up upon clicking an action's touch
// point.
//
// View looks like this:
// +----------------------------------+
// ||"Button options"|          |icon||
// |----------------------------------|
// ||"Buttons let..."|                |
// |----------------------------------|
// |  |"Choose your button type:"   | |
// |  |feature_tile|  |feature_title| |
// |  |            |  |             | |
// |----------------------------------|
// ||"Selected key"       |key labels||
// ||"key"                            |
// |----------------------------------|
// -----------------------------------|
// ||           Done button          ||
// +----------------------------------+
class ButtonOptionsMenu : public views::View, public TouchInjectorObserver {
  METADATA_HEADER(ButtonOptionsMenu, views::View)

 public:
  ButtonOptionsMenu(DisplayOverlayController* controller, Action* action);
  ButtonOptionsMenu(const ButtonOptionsMenu&) = delete;
  ButtonOptionsMenu& operator=(const ButtonOptionsMenu&) = delete;
  ~ButtonOptionsMenu() override;

  // Set triangle wedge offset from center of the container on the height.
  //   - `offset` < 0, the triangle wedge is above the center.
  //   - `offset` > 0, the triangle wedge is below the center.
  //   - `offset` = 0, the triangle wedge is right on the center.
  void SetArrowVerticalOffset(int offset);
  // Set the triangle wedge on left or right side.
  void SetArrowOnLeft(bool arrow_on_left);

  void UpdateWidget();

  Action* action() const { return action_; }

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditLabelTest;
  friend class OverlayViewTestBase;

  class ShadowLayer;

  void Init();
  void UpdateBorder();

  // Add UI components.
  void AddHeader();
  void AddEditTitle();
  void AddActionSelection();
  void AddActionEdit();
  void AddDoneButton();

  // Functions related to buttons.
  void OnTrashButtonPressed();
  void OnDoneButtonPressed();
  void OnButtonLabelAssignmentPressed();

  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // TouchInjectorObserver:
  void OnActionRemoved(const Action& action) override;
  void OnActionTypeChanged(Action* action, Action* new_action) override;
  void OnActionInputBindingUpdated(const Action& action) override;
  void OnActionNewStateRemoved(const Action& action) override;

  std::unique_ptr<ShadowLayer> shadow_layer_;

  int arrow_vertical_offset_ = 0;
  bool arrow_on_left_ = false;

  // DisplayOverlayController owns this class, no need to deallocate.
  const raw_ptr<DisplayOverlayController> controller_ = nullptr;
  raw_ptr<Action, DanglingUntriaged> action_ = nullptr;

  raw_ptr<ash::IconButton> trash_button_ = nullptr;
  raw_ptr<ActionTypeButtonGroup> button_group_ = nullptr;
  raw_ptr<ActionEditView, DisableDanglingPtrDetection> action_edit_ = nullptr;
  raw_ptr<views::Label> action_name_label_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_BUTTON_OPTIONS_MENU_H_
