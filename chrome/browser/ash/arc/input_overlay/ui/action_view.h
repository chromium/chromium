// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class Action;
class ActionLabel;
class ArrowContainer;
class DisplayOverlayController;
class InputElement;
class RepositionController;
class TouchPoint;

// Represents the default label index. Default -1 means all the index.
constexpr int kDefaultLabelIndex = -1;

// ActionView is the view for each action.
class ActionView : public views::View {
  METADATA_HEADER(ActionView, views::View)

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
  virtual void AddTouchPoint() = 0;

  // Called when associated action is updated.
  void OnActionInputBindingUpdated();
  // Called when window/content bounds are changed.
  void OnContentBoundsSizeChanged();

  // TODO(cuicuiruan): Remove virtual for post MVP once edit menu is ready for
  // `ActionMove`.
  // If `editing_label` == nullptr, set display mode for all the `ActionLabel`
  // child views, otherwise, only set the display mode for `editing_label`.
  virtual void SetDisplayMode(const DisplayMode mode,
                              ActionLabel* editing_label = nullptr);

  // Set position from its center position.
  void SetPositionFromCenterPosition(const gfx::PointF& center_position);
  // Show error message for action. If `ax_annouce` is true, ChromeVox
  // announces the `message` directly. Otherwise, `message` is added as the
  // description of `editing_label`.
  void ShowErrorMsg(std::string_view message,
                    ActionLabel* editing_label,
                    bool ax_annouce);
  // Show info/edu message.
  void ShowInfoMsg(std::string_view message, ActionLabel* editing_label);
  void ShowFocusInfoMsg(std::string_view message, views::View* view);
  void RemoveMessage();
  // Change binding for `action` binding to `input_element` and set
  // `kEditedSuccess` on `action_label` if `action_label` is not nullptr.
  // Otherwise, set `kEditedSuccess` to all `ActionLabel`.
  void ChangeInputBinding(Action* action,
                          ActionLabel* action_label,
                          std::unique_ptr<InputElement> input_element);
  // Reset binding to its previous binding before entering to the edit mode.
  void OnResetBinding();
  // Return true if it needs to show error message and also shows error message.
  // Otherwise, don't show any error message and return false.
  bool ShouldShowErrorMsg(ui::DomCode code,
                          ActionLabel* editing_label = nullptr);
  // Reacts to child label focus change.
  void OnChildLabelUpdateFocus(ActionLabel* child, bool focus);

  // Set the action view to be not new, for the action label.
  void RemoveNewState();

  void ApplyMousePressed(const ui::MouseEvent& event);
  void ApplyMouseDragged(const ui::MouseEvent& event);
  void ApplyMouseReleased(const ui::MouseEvent& event);
  void ApplyGestureEvent(ui::GestureEvent* event);
  bool ApplyKeyPressed(const ui::KeyEvent& event);
  bool ApplyKeyReleased(const ui::KeyEvent& event);

  void ShowButtonOptionsMenu();

  // Callbacks related to reposition operations.
  void OnDraggingCallback();
  void OnMouseDragEndCallback();
  void OnGestureDragEndCallback();
  void OnKeyPressedCallback();
  void OnKeyReleasedCallback();

  void SetTouchPointCenter(const gfx::Point& touch_point_center);
  gfx::Point GetTouchCenterInWindow() const;

  // Returns the `attached_view` position and update the attached_view.
  gfx::Point CalculateAttachViewPositionInRootWindow(
      const gfx::Rect& available_bounds,
      const gfx::Point& window_content_origin,
      ArrowContainer* attached_view) const;

  // views::View:
  void AddedToWidget() override;

  Action* action() { return action_; }
  const std::vector<raw_ptr<ActionLabel, VectorExperimental>>& labels() const {
    return labels_;
  }
  TouchPoint* touch_point() { return touch_point_; }
  DisplayOverlayController* display_overlay_controller() {
    return display_overlay_controller_;
  }
  void set_unbind_label_index(int label_index) {
    unbind_label_index_ = label_index;
  }
  int unbind_label_index() { return unbind_label_index_; }

  std::optional<gfx::Point> touch_point_center() const {
    return touch_point_center_;
  }

 protected:
  virtual void MayUpdateLabelPosition(bool moving = true) = 0;

  void AddTouchPoint(ActionType action_type);

  // Reference to the action of this UI.
  raw_ptr<Action, DanglingUntriaged> action_ = nullptr;
  // Reference to the owner class.
  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
  // Labels for mapping hints.
  std::vector<raw_ptr<ActionLabel, VectorExperimental>> labels_;
  // Current display mode.
  DisplayMode current_display_mode_ = DisplayMode::kNone;
  // Local center position of the touch point view.
  std::optional<gfx::Point> touch_point_center_;

  // Touch point only shows up in the edit mode for users to align the position.
  // This view owns the touch point as one of its children and `touch_point_`
  // is for quick access.
  raw_ptr<TouchPoint, DanglingUntriaged> touch_point_ = nullptr;
  DisplayMode display_mode_ = DisplayMode::kView;

 private:
  friend class ActionViewTest;
  friend class OverlayViewTestBase;
  friend class ViewTestBase;

  void RemoveTouchPoint();

  void SetRepositionController();

  std::unique_ptr<RepositionController> reposition_controller_;

  // By default, no label is unbound.
  int unbind_label_index_ = kDefaultLabelIndex;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_H_
