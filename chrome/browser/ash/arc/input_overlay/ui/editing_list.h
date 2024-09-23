// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_

#include <memory>

#include "ash/style/pill_button.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {
class AnchoredNudge;
class PillButton;
class SystemShadow;
}  // namespace ash

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace views {
class Button;
class Label;
class LabelButton;
class ScrollView;
}  // namespace views

namespace arc::input_overlay {

class ActionViewListItem;
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
  METADATA_HEADER(EditingList, views::View)

 public:
  explicit EditingList(DisplayOverlayController* display_overlay_controller);
  EditingList(const EditingList&) = delete;
  EditingList& operator=(const EditingList&) = delete;
  ~EditingList() override;

  void UpdateWidget();

  ActionViewListItem* GetListItemForAction(Action* action);

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditingListTest;
  friend class EditLabelTest;
  friend class OverlayViewTestBase;

  class AddContainerButton;

  void Init();
  bool HasControls() const;

  // Add top buttons and title.
  void AddHeader();
  // Add the list view for the actions / controls.
  void AddControlListContent();

  // These are called after adding the first new action.
  void MaybeApplyEduDecoration();
  void ShowKeyEditNudge();
  void PerformPulseAnimation();

  // Updates changes depending on whether `is_zero_state` is true.
  void UpdateOnZeroState(bool is_zero_state);

  // Updates the `scroll_view_` when the `scroll_content_` changes. If
  // `scroll_to_bottom` is true, scroll `scroll_view_` to the bottom.
  void UpdateScrollView(bool scroll_to_bottom);
  // Called when `scroll_view_` is scrolled.
  void OnScrollViewScrolled();
  // Returns true if `scroll_view_` is scrolled with an offset.
  bool HasScrollOffset();

  // Functions related to buttons.
  void OnAddButtonPressed();
  void OnDoneButtonPressed();
  void OnHelpButtonPressed();

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
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // TouchInjectorObserver:
  void OnActionAdded(Action& action) override;
  void OnActionRemoved(const Action& action) override;
  void OnActionTypeChanged(Action* action, Action* new_action) override;
  void OnActionInputBindingUpdated(const Action& action) override;
  void OnActionNewStateRemoved(const Action& action) override;

  // For test.
  bool IsKeyEditNudgeShownForTesting() const;
  ash::AnchoredNudge* GetKeyEditNudgeForTesting() const;
  views::LabelButton* GetAddButtonForTesting() const;
  views::Button* GetAddContainerButtonForTesting() const;

  const raw_ptr<DisplayOverlayController> controller_;

  // It wraps ActionViewListItem.
  raw_ptr<views::View> scroll_content_;
  // It wraps `scroll_content_` and adds scrolling feature.
  raw_ptr<views::ScrollView> scroll_view_;

  // Label for list header.
  raw_ptr<views::Label> editing_header_label_;

  raw_ptr<AddContainerButton> add_container_;
  raw_ptr<ash::PillButton> done_button_;

  // Owned by this view.
  std::unique_ptr<ash::SystemShadow> shadow_;

  // Used to tell if the zero state view shows up.
  bool is_zero_state_ = false;
  // Show education decoration once after adding the first action.
  bool show_edu_ = false;

  // LocatedEvent's position when drag starts.
  gfx::Point start_drag_event_pos_;

  base::CallbackListSubscription on_scroll_view_scrolled_subscription_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDITING_LIST_H_
