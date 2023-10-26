// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/view.h"

namespace views {
class Label;
class LabelButton;
class ScrollView;
class TableLayoutView;
}  // namespace views

namespace arc::input_overlay {

class DisplayOverlayController;

// EditingList contains the list of controls.
//   +---------------------------------+
//   ||"Controls"|       |icon||"Done"||
//   ||"Create button"|             |+||
//   |  +---------------------------+  |
//   |  |                           |  |
//   |  |    empty or               |  |
//   |  |    scrollable list        |  |
//   |  |                           |  |
//   |  +---------------------------+  |
//   +---------------------------------+
class EditingList : public views::View, public TouchInjectorObserver {
 public:
  METADATA_HEADER(EditingList);
  explicit EditingList(DisplayOverlayController* display_overlay_controller);
  EditingList(const EditingList&) = delete;
  EditingList& operator=(const EditingList&) = delete;
  ~EditingList() override;

  void UpdateWidget();

  void ShowEduNudgeForEditingTip();

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditingListTest;
  friend class EditLabelTest;
  friend class OverlayViewTestBase;

  void Init();
  bool HasControls() const;

  // Add top buttons and title.
  void AddHeader();
  // Adds the UI row for creating new actions.
  void AddActionAddRow();
  // Add the list view for the actions / controls.
  void AddControlListContent();

  // Updates changes depending on whether `is_zero_state` is true.
  void UpdateOnZeroState(bool is_zero_state);

  // Functions related to buttons.
  void OnAddButtonPressed();
  void OnDoneButtonPressed();
  void OnHelpButtonPressed();
  void UpdateAddButtonState();

  // Drag operations.
  void OnDragStart(const ui::LocatedEvent& event);
  void OnDragUpdate(const ui::LocatedEvent& event);
  void OnDragEnd(const ui::LocatedEvent& event);

  // The attached widget should be magnetic to the left or right and inside or
  // outside of the attached sibling game window inside or outside.
  gfx::Point GetWidgetMagneticPositionLocal();

  // Clips the height of `scroll_view_` based on it is located inside or outside
  // of the game window.
  void ClipScrollViewHeight(bool is_outside);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void OnThemeChanged() override;

  // TouchInjectorObserver:
  void OnActionAdded(Action& action) override;
  void OnActionRemoved(const Action& action) override;
  void OnActionTypeChanged(Action* action, Action* new_action) override;
  void OnActionInputBindingUpdated(const Action& action) override;
  void OnActionNameUpdated(const Action& action) override;
  void OnActionNewStateRemoved(const Action& action) override;

  raw_ptr<DisplayOverlayController> controller_;

  // It wraps ActionViewListItem.
  raw_ptr<views::View> scroll_content_;
  // It wraps `scroll_content_` and adds scrolling feature.
  raw_ptr<views::ScrollView> scroll_view_;

  // Label for list header.
  raw_ptr<views::Label> editing_header_label_;

  // UIs in the row of creating new action.
  raw_ptr<views::TableLayoutView> add_container_;
  raw_ptr<views::Label> add_title_;
  raw_ptr<views::LabelButton> add_button_;

  // Used to tell if the zero state view shows up.
  bool is_zero_state_ = false;

  // LocatedEvent's position when drag starts.
  gfx::Point start_drag_event_pos_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_
