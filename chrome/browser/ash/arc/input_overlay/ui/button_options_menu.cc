// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button_group.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"

namespace arc::input_overlay {

// ButtonOptionsActionEdit shows in ButtonOptions and is associated with each
// of Action.
// ----------------------------
// | |Name tag|        |keys| |
// ----------------------------
class ButtonOptionsActionEdit : public ActionEditView {
 public:
  ButtonOptionsActionEdit(DisplayOverlayController* controller, Action* action)
      : ActionEditView(controller,
                       action,
                       ash::RoundedContainer::Behavior::kBottomRounded) {
    // TODO(b/274690042): Replace the hardcoded string with a localized string.
    auto* title_string =
        (action_->is_new() ? u"Select a key" : u"Selected key");
    name_tag_->SetTitle(title_string);
    labels_view_->set_should_update_title(false);
  }
  ButtonOptionsActionEdit(const ButtonOptionsActionEdit&) = delete;
  ButtonOptionsActionEdit& operator=(const ButtonOptionsActionEdit&) = delete;
  ~ButtonOptionsActionEdit() override = default;

  // ActionEditView:
  void OnActionInputBindingUpdated() override {
    ActionEditView::OnActionInputBindingUpdated();
    // TODO(b/274690042): Replace the hardcoded string with a localized string.
    name_tag_->SetTitle(u"Selected key");
  }

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditLabelTest;

  // ActionEditView:
  void ClickCallback() override { labels_view_->FocusLabel(); }
};

ButtonOptionsMenu::ButtonOptionsMenu(DisplayOverlayController* controller,
                                     Action* action)
    : TouchInjectorObserver(), controller_(controller), action_(action) {
  controller_->AddTouchInjectorObserver(this);
  Init();
}

ButtonOptionsMenu::~ButtonOptionsMenu() {
  controller_->RemoveTouchInjectorObserver(this);
}

void ButtonOptionsMenu::Init() {
  SetUseDefaultFillLayout(true);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddHeader();
  AddEditTitle();
  AddActionSelection();
  AddActionEdit();
}

void ButtonOptionsMenu::AddHeader() {
  // ------------------------------------
  // ||icon|  |"Button options"|  |icon||
  // ------------------------------------
  auto* container = AddChildView(std::make_unique<views::View>());
  container->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  /*horizontal_resize=*/1.0f,
                  views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/2.0f,
                 views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, views::TableLayout::kFixedSize, 0);
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 16, 0));

  container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&ButtonOptionsMenu::OnTrashButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsDeleteIcon,
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));

  container->AddChildView(ash::bubble_utils::CreateLabel(
      // TODO(b/274690042): Replace placeholder text with localized strings.
      ash::TypographyToken::kCrosTitle1, u"Button options",
      cros_tokens::kCrosSysOnSurface));

  done_button_ = container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&ButtonOptionsMenu::OnDoneButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsDoneIcon,
      // TODO(b/279117180): Replace placeholder names with a11y strings.
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
}

void ButtonOptionsMenu::AddEditTitle() {
  // ------------------------------
  // ||"Key assignment"|          |
  // ------------------------------
  auto* container = AddChildView(std::make_unique<views::View>());
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart);
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 12, 0));

  container->AddChildView(ash::bubble_utils::CreateLabel(
      // TODO(b/274690042): Replace placeholder text with localized strings.
      ash::TypographyToken::kCrosBody2, u"Key assignment",
      cros_tokens::kCrosSysOnSurface));
}

void ButtonOptionsMenu::AddActionSelection() {
  // ----------------------------------
  // | |feature_tile| |feature_title| |
  // ----------------------------------
  auto* container = AddChildView(std::make_unique<views::View>());
  container->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, /*top_radius=*/16,
      /*bottom_radius=*/0, /*for_border_thickness=*/0));
  container->SetUseDefaultFillLayout(true);
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 2, 0));

  button_group_ = container->AddChildView(
      ActionTypeButtonGroup::CreateButtonGroup(controller_, action_));
}

void ButtonOptionsMenu::AddActionEdit() {
  // ------------------------------
  // ||"Selected key" |key labels||
  // ||"key"                      |
  // ------------------------------
  action_edit_ = AddChildView(
      std::make_unique<ButtonOptionsActionEdit>(controller_, action_));
}

void ButtonOptionsMenu::OnTrashButtonPressed() {
  controller_->RemoveAction(action_);
}

void ButtonOptionsMenu::OnDoneButtonPressed() {
  controller_->SaveToProtoFile();
  controller_->MayShowEduNudgeForEditingTip();

  // Remove this view at last.
  controller_->RemoveButtonOptionsMenuWidget();
}

void ButtonOptionsMenu::OnActionRemoved(const Action& action) {
  if (action_ != &action) {
    return;
  }
  controller_->RemoveButtonOptionsMenuWidget();
}

void ButtonOptionsMenu::OnActionTypeChanged(Action* action,
                                            Action* new_action) {
  DCHECK_EQ(action_, action);
  action_ = new_action;
  button_group_->set_action(new_action);
  auto index = GetIndexOf(action_edit_);
  RemoveChildViewT(action_edit_);
  action_edit_ = AddChildViewAt(
      std::make_unique<ButtonOptionsActionEdit>(controller_, action_), *index);
  controller_->UpdateButtonOptionsMenuWidgetBounds(action_);
}

void ButtonOptionsMenu::OnActionInputBindingUpdated(const Action& action) {
  if (action_ == &action) {
    action_edit_->OnActionInputBindingUpdated();
  }
}

void ButtonOptionsMenu::OnActionNameUpdated(const Action& action) {
  NOTIMPLEMENTED();
}

void ButtonOptionsMenu::OnActionNewStateRemoved(const Action& action) {
  if (action_ == &action) {
    action_edit_->RemoveNewState();
  }
}

BEGIN_METADATA(ButtonOptionsMenu, views::View)
END_METADATA

}  // namespace arc::input_overlay
