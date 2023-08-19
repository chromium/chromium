// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/table_layout.h"

namespace arc::input_overlay {

// static
std::unique_ptr<EditLabels> EditLabels::CreateEditLabels(
    DisplayOverlayController* controller,
    Action* action,
    NameTag* name_tag,
    bool set_title) {
  auto labels =
      std::make_unique<EditLabels>(controller, action, name_tag, set_title);
  labels->Init();
  return labels;
}

EditLabels::EditLabels(DisplayOverlayController* controller,
                       Action* action,
                       NameTag* name_tag,
                       bool set_title)
    : controller_(controller),
      action_(action),
      name_tag_(name_tag),
      set_title_(set_title) {}

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

  UpdateNameTag();
  if (set_title_) {
    UpdateNameTagTitle();
  }
}

void EditLabels::OnActionInputBindingUpdated() {
  for (auto* label : labels_) {
    label->OnActionInputBindingUpdated();
  }

  UpdateNameTag();
}

void EditLabels::UpdateNameTagTitle() {
  name_tag_->SetTitle(GetActionNameAtIndex(controller_->action_name_list(),
                                           action_->name_label_index()));
}

void EditLabels::SetNameTagState(bool is_error,
                                 const std::u16string& error_tooltip) {
  // If an individual label doesn't need to show error, but other sibling label
  // still has label unassigned, `name_tag_` still needs to show error.
  if (!is_error && missing_assign_) {
    name_tag_->SetState(
        /*is_error=*/true,
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDIT_MISSING_BINDING));
  } else {
    name_tag_->SetState(is_error, error_tooltip);
  }
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

void EditLabels::UpdateNameTag() {
  std::u16string key_string = u"";
  // Check if all labels are unassigned. The prefix for the sub-title is
  // different if all labels are unassigned.
  bool all_unassigned = true;
  // If at least one label is unassigned, it needs to show error state.
  missing_assign_ = false;
  DCHECK_GE(labels_.size(), 1u);
  for (auto* label : labels_) {
    key_string.append(label->GetText());
    key_string.append(u", ");
    if (label->IsInputUnbound()) {
      missing_assign_ = true;
    } else {
      all_unassigned = false;
    }
  }
  key_string.erase(key_string.end() - 2, key_string.end());
  // TODO(b/274690042): Replace placeholder text with localized strings.
  if (all_unassigned) {
    key_string = u"unassigned";
  }

  name_tag_->SetSubtitle(labels_.size() == 1 ? u"Key " + key_string
                                             : u"Keys " + key_string);
  name_tag_->SetState(
      /*is_error=*/missing_assign_,
      missing_assign_
          ? l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDIT_MISSING_BINDING)
          : u"");
}

}  // namespace arc::input_overlay
