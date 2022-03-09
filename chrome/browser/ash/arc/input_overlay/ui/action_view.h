// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_H_

#include "ash/wm/desks/persistent_desks_bar_button.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_circle.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_button.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/view.h"

namespace arc {
namespace input_overlay {

class Action;
class DisplayOverlayController;
class ActionEditButton;

// ActionView is the view for each action.
class ActionView : public views::View {
 public:
  explicit ActionView(Action* action,
                      DisplayOverlayController* display_overlay_controller);
  ActionView(const ActionView&) = delete;
  ActionView& operator=(const ActionView&) = delete;
  ~ActionView() override;

  Action* action() { return action_; }

  void set_editable(bool editable) { editable_ = editable; }

  // Set position from its center position.
  void SetPositionFromCenterPosition(gfx::PointF& center_position);
  void SetDisplayMode(const DisplayMode mode);
  void OnMenuEntryPressed();
  // Get edit menu position in parent's bounds.
  gfx::Point GetEditMenuPosition(gfx::Size menu_size);
  void RemoveEditMenu();

 protected:
  // Reference to the action of this UI.
  Action* action_ = nullptr;
  // Reference to the owner class.
  DisplayOverlayController* const display_overlay_controller_ = nullptr;
  // Some types are not supported to edit.
  bool editable_ = false;
  // Three-dot button to show the |ActionEditMenu|.
  ActionEditButton* menu_entry_ = nullptr;
  // The circle view shows up for editing the action.
  ActionCircle* circle_ = nullptr;
  // Labels for mapping hints.
  std::vector<ActionLabel*> labels_;
  // Current display mode.
  DisplayMode current_display_mode_ = DisplayMode::kNone;
  // Center position of the circle view.
  gfx::Point center_;

 private:
  void AddEditButton();
  void RemoveEditButton();
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_H_
