// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_menu_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/switch_access_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_node_data.mojom-shared.h"
#include "ui/events/event.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

namespace {
constexpr int kMaxColumns = 3;

struct ButtonInfo {
  const gfx::VectorIcon* icon;
  int label_id;
};

// These strings must match the values of
// accessibility_private::SwitchAccessMenuAction.
const base::flat_map<std::string, ButtonInfo>& GetMenuButtonDetails() {
  static const base::NoDestructor<base::flat_map<std::string, ButtonInfo>>
      kMenuButtonDetails({
          {"copy", {&kSwitchAccessCopyIcon, IDS_ASH_SWITCH_ACCESS_COPY}},
          {"cut", {&kSwitchAccessCutIcon, IDS_ASH_SWITCH_ACCESS_CUT}},
          {"decrement",
           {&kSwitchAccessDecrementIcon, IDS_ASH_SWITCH_ACCESS_DECREMENT}},
          {"dictation",
           {&kDictationOnNewuiIcon, IDS_ASH_SWITCH_ACCESS_DICTATION}},
          {"endTextSelection",
           {&kSwitchAccessEndTextSelectionIcon,
            IDS_ASH_SWITCH_ACCESS_END_TEXT_SELECTION}},
          {"increment",
           {&kSwitchAccessIncrementIcon, IDS_ASH_SWITCH_ACCESS_INCREMENT}},
          {"jumpToBeginningOfText",
           {&kSwitchAccessJumpToBeginningOfTextIcon,
            IDS_ASH_SWITCH_ACCESS_JUMP_TO_BEGINNING_OF_TEXT}},
          {"jumpToEndOfText",
           {&kSwitchAccessJumpToEndOfTextIcon,
            IDS_ASH_SWITCH_ACCESS_JUMP_TO_END_OF_TEXT}},
          {"keyboard",
           {&kSwitchAccessKeyboardIcon, IDS_ASH_SWITCH_ACCESS_KEYBOARD}},
          {"moveBackwardOneCharOfText",
           {&kSwitchAccessMoveBackwardOneCharOfTextIcon,
            IDS_ASH_SWITCH_ACCESS_MOVE_BACKWARD_ONE_CHAR_OF_TEXT}},
          {"moveBackwardOneWordOfText",
           {&kSwitchAccessMoveBackwardOneWordOfTextIcon,
            IDS_ASH_SWITCH_ACCESS_MOVE_BACKWARD_ONE_WORD_OF_TEXT}},
          {"moveCursor",
           {&kSwitchAccessMoveCursorIcon, IDS_ASH_SWITCH_ACCESS_MOVE_CURSOR}},
          {"moveDownOneLineOfText",
           {&kSwitchAccessMoveDownOneLineOfTextIcon,
            IDS_ASH_SWITCH_ACCESS_MOVE_DOWN_ONE_LINE_OF_TEXT}},
          {"moveForwardOneCharOfText",
           {&kSwitchAccessMoveForwardOneCharOfTextIcon,
            IDS_ASH_SWITCH_ACCESS_MOVE_FORWARD_ONE_CHAR_OF_TEXT}},
          {"moveForwardOneWordOfText",
           {&kSwitchAccessMoveForwardOneWordOfTextIcon,
            IDS_ASH_SWITCH_ACCESS_MOVE_FORWARD_ONE_WORD_OF_TEXT}},
          {"moveUpOneLineOfText",
           {&kSwitchAccessMoveUpOneLineOfTextIcon,
            IDS_ASH_SWITCH_ACCESS_MOVE_UP_ONE_LINE_OF_TEXT}},
          {"paste", {&kSwitchAccessPasteIcon, IDS_ASH_SWITCH_ACCESS_PASTE}},
          {"scrollDown",
           {&kSwitchAccessScrollDownIcon, IDS_ASH_SWITCH_ACCESS_SCROLL_DOWN}},
          {"scrollLeft",
           {&kSwitchAccessScrollLeftIcon, IDS_ASH_SWITCH_ACCESS_SCROLL_LEFT}},
          {"scrollRight",
           {&kSwitchAccessScrollRightIcon, IDS_ASH_SWITCH_ACCESS_SCROLL_RIGHT}},
          {"scrollUp",
           {&kSwitchAccessScrollUpIcon, IDS_ASH_SWITCH_ACCESS_SCROLL_UP}},
          {"select", {&kSwitchAccessSelectIcon, IDS_ASH_SWITCH_ACCESS_SELECT}},
          {"settings",
           {&kSwitchAccessSettingsIcon, IDS_ASH_SWITCH_ACCESS_SETTINGS}},
          {"startTextSelection",
           {&kSwitchAccessStartTextSelectionIcon,
            IDS_ASH_SWITCH_ACCESS_START_TEXT_SELECTION}},
      });
  return *kMenuButtonDetails;
}

}  // namespace

SwitchAccessMenuView::SwitchAccessMenuView() = default;
SwitchAccessMenuView::~SwitchAccessMenuView() = default;

void SwitchAccessMenuView::SetActions(std::vector<std::string> actions) {
  RemoveAllChildViews(/*delete_children=*/true);

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddPaddingColumn(0 /* resize_percent */, kUnifiedMenuPadding);
  for (int i = 0; i < kMaxColumns; i++) {
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                       0, /* resize_percent */
                       views::GridLayout::ColumnSize::kFixed,
                       SwitchAccessMenuButton::kWidthDip, 0);
    columns->AddPaddingColumn(0 /* resize_percent */, kUnifiedMenuPadding);
  }

  int button_count = 0;
  for (std::string action : actions) {
    auto it = GetMenuButtonDetails().find(action);
    if (it == GetMenuButtonDetails().end())
      continue;
    ButtonInfo info = it->second;
    // If this is the first button of a new row, tell the layout to start a
    // new row.
    if (button_count % kMaxColumns == 0)
      layout->StartRowWithPadding(0, 0, 0, kUnifiedMenuPadding);
    layout->AddView(std::make_unique<SwitchAccessMenuButton>(action, *info.icon,
                                                             info.label_id));
    button_count++;
  }
  layout->AddPaddingRow(0, kUnifiedMenuPadding);
  InvalidateLayout();
}

int SwitchAccessMenuView::GetBubbleWidthDip() const {
  // In the future this will vary with the number of menu items displayed.
  return (kMaxColumns * SwitchAccessMenuButton::kWidthDip) +
         ((kMaxColumns - 1) * kUnifiedMenuPadding) +
         kUnifiedMenuItemPadding.left() + kUnifiedMenuItemPadding.right();
}

void SwitchAccessMenuView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kMenu;
}

const char* SwitchAccessMenuView::GetClassName() const {
  return "SwitchAccessMenuView";
}

}  // namespace ash
