// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"

#include "base/check_op.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_utils.h"

namespace arc::input_overlay {

// static
std::unique_ptr<EditLabels> EditLabels::CreateEditLabels(
    DisplayOverlayController* controller,
    Action* action,
    NameTag* name_tag,
    bool should_update_title) {
  auto labels = std::make_unique<EditLabels>(controller, action, name_tag,
                                             should_update_title);
  labels->Init();
  return labels;
}

EditLabels::EditLabels(DisplayOverlayController* controller,
                       Action* action,
                       NameTag* name_tag,
                       bool should_update_title)
    : controller_(controller),
      action_(action),
      name_tag_(name_tag),
      should_update_title_(should_update_title) {}

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
}

void EditLabels::OnActionInputBindingUpdated() {
  for (auto* label : labels_) {
    label->OnActionInputBindingUpdated();
  }

  UpdateNameTag();
}

void EditLabels::SetNameTagState(bool is_error,
                                 const std::u16string& error_tooltip) {
  // If an individual label doesn't need to show error, but other sibling label
  // still has label unassigned, `name_tag_` still needs to show error.
  if (!is_error && missing_assign_) {
    name_tag_->SetState(
        /*is_error=*/!action_->is_new(),
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDIT_MISSING_BINDING));
  } else {
    name_tag_->SetState(is_error, error_tooltip);
  }
}

void EditLabels::FocusLabel() {
  // Clicking the edit labels with an already focused edit label causes the next
  // label to gain focus.
  for (size_t i = 0; i < labels_.size(); i++) {
    auto* label = labels_[i];
    if (label->HasFocus()) {
      labels_[(i + 1) % labels_.size()]->RequestFocus();
      return;
    }
  }
  labels_[0]->RequestFocus();
}

void EditLabels::ShowEduNudgeForEditingTip() {
  size_t size = labels_.size();
  DCHECK_GE(size, 1u);
  // TODO(b/274690042): Replace it with localized strings.
  controller_->AddNudgeWidget(labels_[size - 1],
                              u"You can easily click and swap this key. To "
                              u"edit the details, tap the row.");
}

std::u16string EditLabels::CalculateActionName() {
  std::u16string key_string = u"";
  // Check if all labels are unassigned. The prefix for the sub-title is
  // different if all labels are unassigned.
  bool all_unassigned = true;
  // If at least one label is unassigned, it needs to show error state.
  missing_assign_ = false;
  DCHECK_GE(labels_.size(), 1u);
  for (auto* label : labels_) {
    if (label->IsInputUnbound()) {
      missing_assign_ = true;
    } else {
      key_string.append(label->GetText());
      all_unassigned = false;
    }
  }
  // TODO(b/274690042): Replace placeholder text with localized strings.
  if (all_unassigned) {
    switch (action_->GetType()) {
      case ActionType::TAP:
        return u"Unassigned button";
      case ActionType::MOVE:
        return u"Unassigned joystick";
      default:
        NOTREACHED();
    }
  }

  auto* prefix_string = u"";
  switch (action_->GetType()) {
    case ActionType::TAP:
      prefix_string = u"Game button ";
      break;
    case ActionType::MOVE:
      prefix_string = u"Joystick ";
      break;
    default:
      NOTREACHED();
  }
  return prefix_string + key_string;
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
  // If at least one label is unassigned, it needs to show error state.
  missing_assign_ = false;
  DCHECK_GE(labels_.size(), 1u);
  for (auto* label : labels_) {
    if (label->IsInputUnbound()) {
      missing_assign_ = true;
      break;
    }
  }

  name_tag_->SetState(
      // The name tag is not set to be in an error state if it was newly
      // created.
      /*is_error=*/missing_assign_ && !action_->is_new(),
      missing_assign_
          ? l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDIT_MISSING_BINDING)
          : u"");

  if (should_update_title_) {
    name_tag_->SetTitle(CalculateActionName());
  }
}

void EditLabels::RemoveNewState() {
  for (auto* label : labels_) {
    label->RemoveNewState();
  }

  UpdateNameTag();
}

BEGIN_METADATA(EditLabels)
END_METADATA

}  // namespace arc::input_overlay
