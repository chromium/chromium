// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector_observer.h"
#include "ui/events/event.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class DisplayOverlayController;

// EditingList contains the list of controls.
//    _________________________________
//   |icon        "Editing"        icon|
//   |   ___________________________   |
//   |  |                           |  |
//   |  |    zero-state or          |  |
//   |  |    scrollable list        |  |
//   |  |___________________________|  |
//   |_________________________________|
//
class EditingList : public views::View, public TouchInjectorObserver {
 public:
  explicit EditingList(DisplayOverlayController* display_overlay_controller);
  EditingList(const EditingList&) = delete;
  EditingList& operator=(const EditingList&) = delete;
  ~EditingList() override;

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

  // Add UI components to `container` as children.
  void AddHeader(views::View* container);
  // Add the zero state view when there are no actions / controls.
  void AddZeroStateContent();
  // Add the list view for the actions / controls.
  void AddControlListContent();

  // Functions related to buttons.
  void OnAddButtonPressed();
  void OnDoneButtonPressed();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // TouchInjectorObserver:
  void OnActionAdded(Action& action) override;
  void OnActionRemoved(const Action& action) override;
  void OnActionTypeChanged(Action* action, Action* new_action) override;
  void OnActionInputBindingUpdated(const Action& action) override;
  void OnActionNameUpdated(const Action& action) override;

  // Drag operations.
  void OnDragStart(const ui::LocatedEvent& event);
  void OnDragUpdate(const ui::LocatedEvent& event);
  void OnDragEnd(const ui::LocatedEvent& event);

  // Clamp position.
  void ClampPosition(gfx::Point& position);

  raw_ptr<DisplayOverlayController> controller_;
  // It wraps ActionViewListItem.
  raw_ptr<views::View> scroll_content_;

  // For test. Used to tell if the zero state view shows up.
  bool is_zero_state_ = false;

  // LocatedEvent's position when drag starts.
  gfx::Point start_drag_event_pos_;
  // Initial position when drag starts.
  gfx::Point start_drag_pos_;
  // Window bounds, relative to the initial position of the editing list.
  gfx::Rect window_bounds_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_
