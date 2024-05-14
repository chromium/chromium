// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"

#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
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
    bool for_editing_list) {
  auto labels = std::make_unique<EditLabels>(controller, action, name_tag,
                                             for_editing_list);
  labels->Init();
  return labels;
}

EditLabels::EditLabels(DisplayOverlayController* controller,
                       Action* action,
                       NameTag* name_tag,
                       bool for_editing_list)
    : controller_(controller),
      action_(action),
      name_tag_(name_tag),
      for_editing_list_(for_editing_list) {}

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
      NOTREACHED_IN_MIGRATION();
  }

  UpdateNameTag();
}

void EditLabels::OnActionInputBindingUpdated() {
  for (arc::input_overlay::EditLabel* label : labels_) {
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
    if (auto* label = labels_[i].get(); label->HasFocus()) {
      labels_[(i + 1) % labels_.size()]->RequestFocus();
      return;
    }
  }
  labels_[0]->RequestFocus();
}

std::u16string EditLabels::CalculateActionName() {
  std::u16string key_string = u"";
  // Check if all labels are unassigned. The prefix for the sub-title is
  // different if all labels are unassigned.
  bool all_unassigned = true;
  // If at least one label is unassigned, it needs to show error state.
  missing_assign_ = false;
  DCHECK_GE(labels_.size(), 1u);
  for (arc::input_overlay::EditLabel* label : labels_) {
    if (label->IsInputUnbound()) {
      missing_assign_ = true;
    } else {
      key_string.append(label->GetText());
      all_unassigned = false;
    }
  }

  int control_type_id = 0;
  switch (action_->GetType()) {
    case ActionType::TAP:
      control_type_id = IDS_INPUT_OVERLAY_BUTTON_TYPE_SINGLE_BUTTON_LABEL;
      break;
    case ActionType::MOVE:
      control_type_id = IDS_INPUT_OVERLAY_BUTTON_TYPE_JOYSTICK_BUTTON_LABEL;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  if (all_unassigned) {
    return l10n_util::GetStringFUTF16(
        IDS_INPUT_OVERLAY_CONTROL_NAME_LABEL_UNASSIGNED_TEMPLATE,
        l10n_util::GetStringUTF16(control_type_id));
  }

  return l10n_util::GetStringFUTF16(
      IDS_INPUT_OVERLAY_CONTROL_NAME_LABEL_TEMPLATE,
      l10n_util::GetStringUTF16(control_type_id), key_string);
}

std::u16string EditLabels::CalculateKeyListForA11yLabel() const {
  std::vector<std::u16string> keys;
  for (EditLabel* label : labels_) {
    keys.push_back(GetDisplayTextAccessibleName(label->GetText()));
  }

  return base::JoinString(keys, u", ");
}

bool EditLabels::IsFirstLabelUnassigned() const {
  DCHECK_GE(labels_.size(), 1u);
  return labels_[0]->IsInputUnbound();
}

void EditLabels::PerformPulseAnimationOnFirstLabel() {
  DCHECK_GE(labels_.size(), 1u);
  labels_[0]->PerformPulseAnimation(/*pulse_count=*/0);
}

void EditLabels::InitForActionTapKeyboard() {
  SetUseDefaultFillLayout(true);
  labels_.emplace_back(AddChildView(
      std::make_unique<EditLabel>(controller_, action_, for_editing_list_)));
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
      DCHECK_LT(labels_.size(), size_t(Direction::kMaxValue) + 1);
      labels_.emplace_back(AddChildView(std::make_unique<EditLabel>(
          controller_, action_, for_editing_list_, labels_.size())));
    }
  }
}

void EditLabels::UpdateNameTag() {
  // If at least one label is unassigned, it needs to show error state.
  missing_assign_ = false;
  DCHECK_GE(labels_.size(), 1u);
  for (arc::input_overlay::EditLabel* label : labels_) {
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

  if (for_editing_list_) {
    name_tag_->SetTitle(CalculateActionName());
  }
}

void EditLabels::RemoveNewState() {
  for (arc::input_overlay::EditLabel* label : labels_) {
    label->RemoveNewState();
  }

  UpdateNameTag();
}

BEGIN_METADATA(EditLabels)
END_METADATA

}  // namespace arc::input_overlay
