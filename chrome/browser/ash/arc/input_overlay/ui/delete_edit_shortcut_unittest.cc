// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/delete_edit_shortcut.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "ui/views/view_utils.h"

namespace arc::input_overlay {

class DeleteEditShortcutTest : public OverlayViewTestBase {
 public:
  DeleteEditShortcutTest() = default;
  ~DeleteEditShortcutTest() override = default;

  void PressEditButton() {
    if (auto* delete_edit_view = GetDeleteEditShortcut()) {
      delete_edit_view->OnEditButtonPressed();
    }
  }

  void PressDeleteButton() {
    if (auto* delete_edit_view = GetDeleteEditShortcut()) {
      delete_edit_view->OnDeleteButtonPressed();
    }
  }

  Action* GetDeleteEditShortcutAnchorAction() const {
    if (const auto* delete_edit_view = GetDeleteEditShortcut()) {
      if (const auto* list_item = views::AsViewClass<ActionViewListItem>(
              delete_edit_view->GetAnchorView())) {
        return list_item->action();
      }
    }
    return nullptr;
  }

  bool IsDeleteEditShortcutVisible() const {
    if (auto* delete_edit_view = GetDeleteEditShortcut()) {
      return delete_edit_view->GetWidget()->IsVisible();
    }
    return false;
  }
};

TEST_F(DeleteEditShortcutTest, TestVisibility) {
  EXPECT_FALSE(IsDeleteEditShortcutVisible());
  HoverAtActionViewListItem(/*index=*/0u);
  EXPECT_TRUE(IsDeleteEditShortcutVisible());
  EXPECT_EQ(GetEditingListItemAction(/*index=*/0u),
            GetDeleteEditShortcutAnchorAction());

  HoverAtActionViewListItem(/*index=*/1u);
  EXPECT_TRUE(IsDeleteEditShortcutVisible());
  EXPECT_EQ(GetEditingListItemAction(/*index=*/1u),
            GetDeleteEditShortcutAnchorAction());

  // Click and touch on the center of the delete-edit view.
  auto* delete_edit_view = GetDeleteEditShortcut();
  DCHECK(delete_edit_view);
  LeftClickOn(delete_edit_view);
  EXPECT_TRUE(IsDeleteEditShortcutVisible());
  GestureTapOn(delete_edit_view);
  EXPECT_TRUE(IsDeleteEditShortcutVisible());

  // Click outside of the delete-edit view to close it.
  auto origin = delete_edit_view->GetBoundsInScreen().origin();
  origin.Offset(-2, -2);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(origin);
  event_generator->ClickLeftButton();
  EXPECT_FALSE(IsDeleteEditShortcutVisible());

  // Mouse hover on the delete-edit view and then hover out to close it.
  HoverAtActionViewListItem(/*index=*/1u);
  EXPECT_TRUE(IsDeleteEditShortcutVisible());
  const auto view_bounds = GetDeleteEditShortcut()->GetBoundsInScreen();
  event_generator->MoveMouseTo(view_bounds.CenterPoint());
  EXPECT_TRUE(IsDeleteEditShortcutVisible());
  auto point = view_bounds.bottom_right();
  point.Offset(2, 2);
  event_generator->MoveMouseTo(point);
  EXPECT_FALSE(IsDeleteEditShortcutVisible());
}

TEST_F(DeleteEditShortcutTest, TestFunctions) {
  // Test edit button.
  EXPECT_FALSE(IsDeleteEditShortcutVisible());
  HoverAtActionViewListItem(/*index=*/0u);
  EXPECT_TRUE(IsDeleteEditShortcutVisible());
  PressEditButton();
  EXPECT_FALSE(IsDeleteEditShortcutVisible());
  EXPECT_TRUE(GetButtonOptionsMenu());
  EXPECT_EQ(GetEditingListItemAction(/*index=*/0u),
            GetButtonOptionsMenuAction());
  PressDoneButtonOnButtonOptionsMenu();

  // Test delete button.
  const size_t original_size = GetActionListItemsSize();
  HoverAtActionViewListItem(/*index=*/1u);
  EXPECT_TRUE(IsDeleteEditShortcutVisible());
  PressDeleteButton();
  EXPECT_FALSE(IsDeleteEditShortcutVisible());
  EXPECT_EQ(original_size - 1, GetActionListItemsSize());
  EXPECT_EQ(original_size - 1, GetActionViewSize());
}

}  // namespace arc::input_overlay
