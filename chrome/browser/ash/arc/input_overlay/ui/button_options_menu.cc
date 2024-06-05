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
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button_group.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"

namespace arc::input_overlay {

namespace {

constexpr float kDoneButtonCornerRadius = 16.0f;

// Gap from focus ring outer edge to the edge of the view.
constexpr float kDoneButtonHaloInset = -5.0f;
// Thickness of focus ring.
constexpr float kDoneButtonHaloThickness = 3.0f;

constexpr int kHeaderLeftMarginSpacing = 6;

}  // namespace

// ButtonOptionsActionEdit shows in ButtonOptions and is associated with each
// of Action.
// ----------------------------
// | |Name tag|        |keys| |
// ----------------------------
class ButtonOptionsActionEdit : public ActionEditView {
  METADATA_HEADER(ButtonOptionsActionEdit, ActionEditView)

 public:
  ButtonOptionsActionEdit(DisplayOverlayController* controller, Action* action)
      : ActionEditView(controller,
                       action,
                       /*is_editing_list=*/false) {
    name_tag_->SetTitle(l10n_util::GetStringUTF16(
        action_->is_new() ? IDS_INPUT_OVERLAY_BUTTON_OPTIONS_ASSIGN_NEW_KEY
                          : IDS_INPUT_OVERLAY_BUTTON_OPTIONS_ASSIGNED_KEY));
  }
  ButtonOptionsActionEdit(const ButtonOptionsActionEdit&) = delete;
  ButtonOptionsActionEdit& operator=(const ButtonOptionsActionEdit&) = delete;
  ~ButtonOptionsActionEdit() override = default;

  // ActionEditView:
  void OnActionInputBindingUpdated() override {
    ActionEditView::OnActionInputBindingUpdated();
    name_tag_->SetTitle(l10n_util::GetStringUTF16(
        IDS_INPUT_OVERLAY_BUTTON_OPTIONS_ASSIGNED_KEY));
  }

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override {
    if (is_visible && action_->is_new() &&
        labels_view_->IsFirstLabelUnassigned() &&
        controller_->HasSingleUserAddedAction()) {
      PerformPulseAnimation();
    }
  }

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditLabelTest;

  // ActionEditView:
  void ClickCallback() override { labels_view_->FocusLabel(); }
};

BEGIN_METADATA(ButtonOptionsActionEdit)
END_METADATA

// DoneButton shows in ButtonOptions and allows the user to finish editing the
// action.
// ------------------------------
// ||        Done button       ||
// ------------------------------
class DoneButton : public views::LabelButton {
  METADATA_HEADER(DoneButton, views::LabelButton)

 public:
  explicit DoneButton(PressedCallback pressed_callback)
      : LabelButton(std::move(pressed_callback),
                    l10n_util::GetStringUTF16(
                        IDS_INPUT_OVERLAY_EDITING_DONE_BUTTON_LABEL)) {
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDITING_DONE_BUTTON_LABEL));

    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase,
        /*radius=*/kDoneButtonCornerRadius));
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(14, 0)));
    SetProperty(views::kMarginsKey, gfx::Insets::TLBR(12, 0, 0, 0));

    ash::TypographyProvider::Get()->StyleLabel(
        ash::TypographyToken::kCrosButton2, *label());
    SetEnabledTextColorIds(cros_tokens::kCrosSysOnSurface);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);

    // Set highlight path.
    views::HighlightPathGenerator::Install(
        this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                  gfx::Insets(), /*corner_radius=*/kDoneButtonCornerRadius));

    // Set up highlight and focus ring for `DoneButton`.
    ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                          /*highlight_on_hover=*/false,
                                          /*highlight_on_focus=*/false);
    auto* focus_ring = views::FocusRing::Get(this);
    focus_ring->SetHaloInset(kDoneButtonHaloInset);
    focus_ring->SetHaloThickness(kDoneButtonHaloThickness);
  }

  DoneButton(const DoneButton&) = delete;
  DoneButton& operator=(const DoneButton&) = delete;

  ~DoneButton() override = default;
};

BEGIN_METADATA(DoneButton)
END_METADATA

ButtonOptionsMenu::ButtonOptionsMenu(DisplayOverlayController* controller,
                                     Action* action)
    : TouchInjectorObserver(), controller_(controller), action_(action) {
  controller_->AddTouchInjectorObserver(this);
  Init();
}

ButtonOptionsMenu::~ButtonOptionsMenu() {
  controller_->RemoveTouchInjectorObserver(this);
}

void ButtonOptionsMenu::UpdateWidget() {
  auto* widget = GetWidget();
  DCHECK(widget);

  controller_->UpdateWidgetBoundsInRootWindow(
      widget,
      gfx::Rect(action_->action_view()->CalculateAttachViewPositionInRootWindow(
                    /*available_bounds=*/CalculateAvailableBounds(
                        /*root_window=*/controller_->touch_injector()
                            ->window()
                            ->GetRootWindow()),
                    /*window_content_origin=*/
                    controller_->touch_injector()->content_bounds().origin(),
                    /*attached_view=*/this),
                GetPreferredSize()));
}

void ButtonOptionsMenu::Init() {
  SetUseDefaultFillLayout(true);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddHeader();
  AddEditTitle();
  AddActionSelection();
  AddActionEdit();
  AddDoneButton();
}

void ButtonOptionsMenu::AddHeader() {
  // ------------------------------------
  // ||"Button options"|          |icon||
  // ------------------------------------
  auto* container = AddChildView(std::make_unique<views::View>());
  auto* layout = container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  container->SetProperty(views::kMarginsKey,
                         gfx::Insets::TLBR(0, kHeaderLeftMarginSpacing, 12, 0));

  action_name_label_ = container->AddChildView(ash::bubble_utils::CreateLabel(
      // TODO(b/274690042): Replace placeholder text with localized strings.
      ash::TypographyToken::kCrosTitle1, u"", cros_tokens::kCrosSysOnSurface));
  action_name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  action_name_label_->SetMultiLine(true);
  // Flex `action_name_label_` to fill empty space.
  layout->SetFlexForView(action_name_label_, /*flex=*/1);

  trash_button_ = container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&ButtonOptionsMenu::OnTrashButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsDeleteIcon,
      IDS_INPUT_OVERLAY_BUTTON_OPTIONS_DELETE_TOOLTIP_TEXT));
}

void ButtonOptionsMenu::AddEditTitle() {
  // ------------------------------
  // ||"Buttons let..."|          |
  // ------------------------------
  auto* label = AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosAnnotation2,
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_BUTTON_OPTIONS_EDIT_INFO),
      cros_tokens::kCrosSysOnSurface));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMaximumWidth(kButtonOptionsMenuWidth -
                         2 * kArrowContainerHorizontalBorderInset -
                         kHeaderLeftMarginSpacing);
  label->SetProperty(views::kMarginsKey,
                     gfx::Insets::TLBR(0, kHeaderLeftMarginSpacing, 16, 0));
}

void ButtonOptionsMenu::AddActionSelection() {
  // ----------------------------------
  // | |"Choose your button type:"  | |
  // | |feature_tile| |feature_title| |
  // ----------------------------------
  auto* container = AddChildView(std::make_unique<views::View>());
  container->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, /*top_radius=*/16.0f,
      /*bottom_radius=*/0.0f));
  container->SetUseDefaultFillLayout(true);
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 2, 0));
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/gfx::Insets::TLBR(12, 16, 16, 16),
      /*between_child_spacing=*/12));

  auto* label = container->AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosButton2,
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_BUTTON_OPTIONS_BUTTON_TYPE),
      cros_tokens::kCrosSysOnSurface));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMaximumWidth(kButtonOptionsMenuWidth -
                         2 * kArrowContainerHorizontalBorderInset);

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
  action_name_label_->SetText(action_edit_->CalculateActionName());
}

void ButtonOptionsMenu::AddDoneButton() {
  // ------------------------------
  // ||        Done button       ||
  // ------------------------------
  AddChildView(std::make_unique<DoneButton>(base::BindRepeating(
      &ButtonOptionsMenu::OnDoneButtonPressed, base::Unretained(this))));
}

void ButtonOptionsMenu::OnTrashButtonPressed() {
  RecordButtonOptionsMenuFunctionTriggered(controller_->GetPackageName(),
                                           ButtonOptionsMenuFunction::kDelete);
  controller_->RemoveAction(action_);
}

void ButtonOptionsMenu::OnDoneButtonPressed() {
  controller_->SaveToProtoFile();
  RecordButtonOptionsMenuFunctionTriggered(controller_->GetPackageName(),
                                           ButtonOptionsMenuFunction::kDone);

  controller_->SetEditingListVisibility(/*visible=*/true);

  // Remove this view at last.
  controller_->RemoveButtonOptionsMenuWidget();
}

void ButtonOptionsMenu::OnActionRemoved(const Action& action) {
  if (action_ != &action) {
    return;
  }

  controller_->SetEditingListVisibility(/*visible=*/true);

  // Remove this view at last.
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
  action_name_label_->SetText(action_edit_->CalculateActionName());
  UpdateWidget();
}

void ButtonOptionsMenu::OnActionInputBindingUpdated(const Action& action) {
  if (action_ == &action) {
    action_edit_->OnActionInputBindingUpdated();
    action_name_label_->SetText(action_edit_->CalculateActionName());
  }
}

void ButtonOptionsMenu::OnActionNewStateRemoved(const Action& action) {
  if (action_ == &action) {
    action_edit_->RemoveNewState();
  }
}

BEGIN_METADATA(ButtonOptionsMenu)
END_METADATA

}  // namespace arc::input_overlay
