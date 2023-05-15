// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_menu.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace arc::input_overlay {
namespace {
constexpr char kFontStyle[] = "Roboto";
constexpr int kFontSize = 16;
constexpr int kCornerRadius = 6;
constexpr int kMenuHeight = 192;
constexpr int kMenuWidth = 76;
constexpr int kCheckIconSize = 12;
constexpr int kSpaceCheckLabel = 48;
constexpr int kButtonHeight = 44;
}  // namespace

class ActionEditMenu::BindingButton : public views::LabelButton {
 public:
  BindingButton(PressedCallback callback, int text_source_id)
      : LabelButton(callback, l10n_util::GetStringUTF16(text_source_id)) {
    SetAccessibleName(l10n_util::GetStringUTF16(text_source_id));
    SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(0, 16, 0, 12)));
    SetHorizontalAlignment(gfx::ALIGN_RIGHT);

    auto* color_provider = ash::AshColorProvider::Get();
    DCHECK(color_provider);
    if (!color_provider) {
      return;
    }
    SetTextColor(
        views::Button::STATE_NORMAL,
        color_provider->GetContentLayerColor(
            ash::AshColorProvider::ContentLayerType::kTextColorPrimary));
    SetTextColor(
        views::Button::STATE_HOVERED,
        color_provider->GetContentLayerColor(
            ash::AshColorProvider::ContentLayerType::kTextColorPrimary));
    label()->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL,
                                       kFontSize, gfx::Font::Weight::NORMAL));
    auto key_size = CalculatePreferredSize();
    SetMinSize(gfx::Size(key_size.width(), kButtonHeight));
    ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                          /*highlight_on_hover=*/true,
                                          /*highlight_on_focus=*/true);
  }

  void Layout() override {
    LabelButton::Layout();
    label()->SetPosition(gfx::Point(GetInsets().left(), GetInsets().top()));
  }

  void OnBinding() {
    auto* color_provider = ash::AshColorProvider::Get();
    DCHECK(color_provider);
    if (!color_provider) {
      return;
    }
    auto check_icon = gfx::CreateVectorIcon(
        ash::kHollowCheckCircleIcon, kCheckIconSize,
        color_provider->GetContentLayerColor(
            ash::AshColorProvider::ContentLayerType::kIconColorProminent));
    SetImage(views::Button::STATE_NORMAL, check_icon);
    SetImageLabelSpacing(kSpaceCheckLabel);
    image()->SetHorizontalAlignment(views::ImageView::Alignment::kTrailing);
  }

  ~BindingButton() override = default;
};

ActionEditMenu::ActionEditMenu(
    DisplayOverlayController* display_overlay_controller,
    ActionView* anchor)
    : display_overlay_controller_(display_overlay_controller),
      anchor_view_(anchor) {}

ActionEditMenu::~ActionEditMenu() = default;

// static
std::unique_ptr<ActionEditMenu> ActionEditMenu::BuildActionEditMenu(
    DisplayOverlayController* display_overlay_controller,
    ActionView* anchor,
    ActionType action_type) {
  if (!display_overlay_controller) {
    return nullptr;
  }
  display_overlay_controller->RemoveActionEditMenu();

  auto menu =
      std::make_unique<ActionEditMenu>(display_overlay_controller, anchor);

  switch (action_type) {
    case ActionType::TAP:
      menu->InitActionTapEditMenu();
      break;
    case ActionType::MOVE:
      menu->InitActionTapEditMenu();
      break;
    default:
      NOTREACHED();
  }

  return menu;
}

void ActionEditMenu::InitActionTapEditMenu() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto* color_provider = GetColorProvider();
  DCHECK(color_provider);
  if (!color_provider) {
    return;
  }
  const auto bg_color = color_provider->GetColor(cros_tokens::kBgColor);
  SetBackground(views::CreateRoundedRectBackground(bg_color, kCornerRadius));

  // Add each binding button.
  keyboard_key_ = AddChildView(std::make_unique<BindingButton>(
      base::BindRepeating(&ActionEditMenu::OnKeyBoardKeyBindingButtonPressed,
                          base::Unretained(this)),
      IDS_INPUT_OVERLAY_EDIT_MENU_KEYBOARD_KEY));
  mouse_left_ = AddChildView(std::make_unique<BindingButton>(
      base::BindRepeating(&ActionEditMenu::OnMouseLeftClickBindingButtonPressed,
                          base::Unretained(this)),
      IDS_INPUT_OVERLAY_EDIT_MENU_LEFT_MOUSE_CLICK));
  mouse_right_ = AddChildView(std::make_unique<BindingButton>(
      base::BindRepeating(
          &ActionEditMenu::OnMouseRightClickBindingButtonPressed,
          base::Unretained(this)),
      IDS_INPUT_OVERLAY_EDIT_MENU_RIGHT_MOUSE_CLICK));
  reset_ = AddChildView(std::make_unique<BindingButton>(
      base::BindRepeating(&ActionEditMenu::OnResetButtonPressed,
                          base::Unretained(this)),
      IDS_INPUT_OVERLAY_EDIT_MENU_RESET));

  int additional_width = std::max({keyboard_key_->GetMinSize().width(),
                                   mouse_left_->GetMinSize().width(),
                                   mouse_right_->GetMinSize().width()});
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(8, 0, 8, 0)));
  SetSize(gfx::Size(kMenuWidth + additional_width, kMenuHeight));
  SetPosition(anchor_view_->GetEditMenuPosition(size()));
  auto* action = anchor_view_->action();
  // It is possible that the action has no binding after customizing, such as
  // users bind the key to another action.
  auto& input_binding = action->GetCurrentDisplayedInput();
  if (IsKeyboardBound(input_binding)) {
    keyboard_key_->OnBinding();
  }
  if (IsMouseBound(input_binding)) {
    switch (input_binding.mouse_action()) {
      case MouseAction::PRIMARY_CLICK:
        mouse_left_->OnBinding();
        break;
      case MouseAction::SECONDARY_CLICK:
        mouse_right_->OnBinding();
        break;
      default:
        NOTREACHED();
    }
  }
}

void ActionEditMenu::InitActionMoveEditMenu() {
  // TODO(cuicuiruan): Implement after post MVP.
  NOTIMPLEMENTED();
}

void ActionEditMenu::OnKeyBoardKeyBindingButtonPressed() {
  DCHECK(anchor_view_);
  if (!anchor_view_) {
    return;
  }

  anchor_view_->OnBindingToKeyboard();
  display_overlay_controller_->RemoveActionEditMenu();
}

void ActionEditMenu::OnMouseLeftClickBindingButtonPressed() {
  DCHECK(anchor_view_);
  if (!anchor_view_) {
    return;
  }

  anchor_view_->OnBindingToMouse(kPrimaryClick);
  display_overlay_controller_->RemoveActionEditMenu();
}

void ActionEditMenu::OnMouseRightClickBindingButtonPressed() {
  DCHECK(anchor_view_);
  if (!anchor_view_) {
    return;
  }

  anchor_view_->OnBindingToMouse(kSecondaryClick);
  display_overlay_controller_->RemoveActionEditMenu();
}

void ActionEditMenu::OnResetButtonPressed() {
  DCHECK(anchor_view_);
  if (!anchor_view_) {
    return;
  }

  anchor_view_->OnResetBinding();
  display_overlay_controller_->RemoveActionEditMenu();
}

}  // namespace arc::input_overlay
