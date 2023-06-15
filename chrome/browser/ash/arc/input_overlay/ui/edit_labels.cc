// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"
#include "ui/views/layout/table_layout.h"

namespace arc::input_overlay {

std::unique_ptr<EditLabels> EditLabels::CreateEditLabels(
    DisplayOverlayController* controller,
    Action* action) {
  auto labels = std::make_unique<EditLabels>(controller, action);
  labels->Init();
  return labels;
}

EditLabels::EditLabels(DisplayOverlayController* controller, Action* action)
    : controller_(controller), action_(action) {}

EditLabels::~EditLabels() = default;

void EditLabels::Init() {
  switch (action_->GetType()) {
    case ActionType::TAP:
      InitForActionTapKeyboard();
      break;
    case ActionType::MOVE:
      InitForActionMoveKeyboard();
      break;
    default:
      NOTREACHED();
  }
}

void EditLabels::OnActionUpdated() {
  for (auto* label : labels_) {
    label->OnActionUpdated();
  }
}

std::u16string EditLabels::GetTextForNameTag() {
  std::u16string key_string = u"";
  bool unassigned = true;
  for (auto* label : labels_) {
    key_string.append(label->GetText());
    key_string.append(u", ");
    if (!label->IsInputUnbound()) {
      unassigned = false;
    }
  }
  key_string.erase(key_string.end() - 2, key_string.end());
  // TODO(b/274690042): Replace placeholder text with localized strings.
  if (unassigned) {
    key_string = u"unassigned";
  }
  return labels_.size() == 1 ? u"Key " + key_string : u"Keys " + key_string;
}

void EditLabels::InitForActionTapKeyboard() {
  SetUseDefaultFillLayout(true);
  labels_.emplace_back(
      AddChildView(std::make_unique<EditLabel>(controller_, action_)));
}

void EditLabels::InitForActionMoveKeyboard() {
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                  /*v_align=*/views::LayoutAlignment::kCenter,
                  /*horizontal_resize=*/1.0f,
                  /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/4)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/4)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize)
      .AddPaddingRow(/*vertical_resize=*/views::TableLayout::kFixedSize,
                     /*height=*/4)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize);

  for (int i = 0; i < 6; i++) {
    // Column 1 row 1 and Column 3 row 1 are empty.
    if (i == 0 || i == 2) {
      AddChildView(std::make_unique<views::View>());
    } else {
      labels_.emplace_back(AddChildView(
          std::make_unique<EditLabel>(controller_, action_, labels_.size())));
    }
  }
}

}  // namespace arc::input_overlay
