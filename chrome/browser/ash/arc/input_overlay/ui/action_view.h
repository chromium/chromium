// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_H_

#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_button.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_button.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class Action;
class DisplayOverlayController;
class ActionEditButton;

// Represents the default label index. Default -1 means all the index.
constexpr int kDefaultLabelIndex = -1;

// ActionView is the view for each action.
class ActionView : public views::View {
 public:
  ActionView(Action* action,
             DisplayOverlayController* display_overlay_controller);
  ActionView(const ActionView&) = delete;
  ActionView& operator=(const ActionView&) = delete;
  ~ActionView() override;

  // Each type of the actions sets view content differently.
  virtual void SetViewContent(BindingOption binding_option) = 0;
  // Each type of the actions acts differently on key binding change.
  virtual void OnKeyBindingChange(ActionLabel* action_label,
                                  ui::DomCode code) = 0;
  virtual void OnBindingToKeyboard() = 0;
  virtual void OnBindingToMouse(std::string mouse_action) = 0;
  // Each type of the actions shows different edit menu.
  virtual void OnMenuEntryPressed() = 0;
  virtual void AddTouchPoint() = 0;

  // TODO(cuicuiruan): Remove virtual for post MVP once edit menu is ready for
  // |ActionMove|.
  // If |editing_label| == nullptr, set display mode for all the |ActionLabel|
  // child views, otherwise, only set the display mode for |editing_label|.
  virtual void SetDisplayMode(const DisplayMode mode,
                              ActionLabel* editing_label = nullptr);

  // Set position from its center position.
  void SetPositionFromCenterPosition(const gfx::PointF& center_position);
  // Get edit menu position in parent's bounds.
  gfx::Point GetEditMenuPosition(gfx::Size menu_size);
  void RemoveEditMenu();
  // Show error message for action. If |ax_annouce| is true, ChromeVox
  // annouces the |message| directly. Otherwise, |message| is added as the
  // description of |editing_label|.
  void ShowErrorMsg(const base::StringPiece& message,
                    ActionLabel* editing_label,
                    bool ax_annouce);
  // Show info/edu message.
  void ShowInfoMsg(const base::StringPiece& message,
                   ActionLabel* editing_label);
  void ShowLabelFocusInfoMsg(const base::StringPiece& message,
                             ActionLabel* editing_label);
  void RemoveMessage();
  // Change binding for |action| binding to |input_element| and set
  // |kEditedSuccess| on |action_label| if |action_label| is not nullptr.
  // Otherwise, set |kEditedSuccess| to all |ActionLabel|.
  void ChangeInputBinding(Action* action,
                          ActionLabel* action_label,
                          std::unique_ptr<InputElement> input_element);
  // Reset binding to its previous binding before entering to the edit mode.
  void OnResetBinding();
  // Return true if it needs to show error message and also shows error message.
  // Otherwise, don't show any error message and return false.
  bool ShouldShowErrorMsg(ui::DomCode code,
                          ActionLabel* editing_label = nullptr);

  bool ApplyMousePressed(const ui::MouseEvent& event);
  bool ApplyMouseDragged(const ui::MouseEvent& event);
  void ApplyMouseReleased(const ui::MouseEvent& event);
  void ApplyGestureEvent(ui::GestureEvent* event);
  bool ApplyKeyPressed(const ui::KeyEvent& event);
  bool ApplyKeyReleased(const ui::KeyEvent& event);

  void OnFocus() override;
  void OnBlur() override;

  Action* action() { return action_; }
  const std::vector<ActionLabel*>& labels() const { return labels_; }
  TouchPoint* touch_point() { return touch_point_; }
  void set_editable(bool editable) { editable_ = editable; }
  DisplayOverlayController* display_overlay_controller() {
    return display_overlay_controller_;
  }
  void set_unbind_label_index(int label_index) {
    unbind_label_index_ = label_index;
  }
  int unbind_label_index() { return unbind_label_index_; }

 protected:
  void UpdateTrashButtonPosition();

  void AddTouchPoint(ActionType action_type);

  // Reference to the action of this UI.
  raw_ptr<Action> action_ = nullptr;
  // Reference to the owner class.
  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
  // Some types are not supported to edit.
  bool editable_ = false;
  // Three-dot button to show the |ActionEditMenu|.
  raw_ptr<ActionEditButton> menu_entry_ = nullptr;
  // Labels for mapping hints.
  std::vector<ActionLabel*> labels_;
  // Current display mode.
  DisplayMode current_display_mode_ = DisplayMode::kNone;
  // Center position of the touch point view.
  gfx::Point center_;
  // TODO(cuicuirunan): Enable or remove this after MVP.
  bool show_edit_button_ = false;

 private:
  void AddEditButton();
  void RemoveEditButton();

  void AddTrashButton();
  void RemoveTrashButton();
  void OnTrashButtonPressed();

  void RemoveTouchPoint();

  // Drag operations.
  void OnDragStart(const ui::LocatedEvent& event);
  bool OnDragUpdate(const ui::LocatedEvent& event);
  void OnDragEnd();

  void ChangePositionBinding(const gfx::Point& point);

  // By default, no label is unbound.
  int unbind_label_index_ = kDefaultLabelIndex;
  // The position when starting to drag.
  gfx::Point start_drag_event_pos_;
  // Touch point only shows up in the edit mode for users to align the position.
  // This view owns the touch point as one of its children and |touch_point_|
  // is for quick access.
  raw_ptr<TouchPoint> touch_point_ = nullptr;
  // TODO(b/250900717): Update when the final UX/UI is ready.
  raw_ptr<views::ImageButton> trash_button_ = nullptr;

  // TODO(b/260937747): Update or remove when removing flags
  // |kArcInputOverlayAlphaV2| or |kArcInputOverlayBeta|.
  bool allow_reposition_;
  // Corresponding to |kArcInputOverlayBeta| flag to turn on/off the editor
  // feature of adding or removing actions.
  bool beta_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_H_
