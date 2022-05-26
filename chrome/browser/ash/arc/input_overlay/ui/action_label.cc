// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"

#include <set>

#include "ash/style/style_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace arc {
namespace input_overlay {
namespace {
// UI specs.
constexpr int kSideInset = 6;
constexpr gfx::Size kMinimumSmallSize(32, 32);
constexpr gfx::Size kMinimumLargeSize(48, 48);
constexpr int kCornerRadiusView = 6;
constexpr int kIconSize = 20;
constexpr char kFontSytle[] = "Google Sans";
constexpr int kSmallFontSize = 16;
constexpr int kLargeFontSize = 20;

// About colors.
constexpr SkColor kViewModeBgColor = SkColorSetA(SK_ColorGRAY, 0x99);
constexpr SkColor kEditModeBgColor = SK_ColorWHITE;
constexpr SkColor kEditedUnboundBgColor = gfx::kGoogleRed300;
constexpr SkColor kViewTextColor = SK_ColorWHITE;
constexpr SkColor kEditTextColor = gfx::kGoogleGrey900;
constexpr SkColor kFocusRingGreyColor = SkColorSetA(gfx::kGoogleGrey200, 0x60);
constexpr SkColor kFocusRingBlueColor = gfx::kGoogleBlue300;
constexpr SkColor kFocusRingRedColor = gfx::kGoogleRed300;

// About focus ring.
// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -3;
// Thickness of focus ring.
constexpr float kHaloThickness = 2;

// UI strings.
// TODO(cuicuiruan): move the strings to chrome/app/generated_resources.grd
// after UX/UI strings are confirmed.
constexpr base::StringPiece kEditErrorSameKey("Same key");

// Arrow symbols for arrow keys.
constexpr char kLeftArrow[] = "←";
constexpr char kUpArrow[] = "↑";
constexpr char kRightArrow[] = "→";
constexpr char kDownArrow[] = "↓";

}  // namespace

std::string GetDisplayText(const ui::DomCode code) {
  switch (code) {
    case ui::DomCode::NONE:
      return "?";
    case ui::DomCode::ARROW_LEFT:
      return kLeftArrow;
    case ui::DomCode::ARROW_RIGHT:
      return kRightArrow;
    case ui::DomCode::ARROW_UP:
      return kUpArrow;
    case ui::DomCode::ARROW_DOWN:
      return kDownArrow;
    default:
      std::string dom_code_string =
          ui::KeycodeConverter::DomCodeToCodeString(code);
      if (base::StartsWith(dom_code_string, "Key",
                           base::CompareCase::SENSITIVE))
        return base::ToLowerASCII(dom_code_string.substr(3));
      if (base::StartsWith(dom_code_string, "Digit",
                           base::CompareCase::SENSITIVE))
        return dom_code_string.substr(5);
      auto lower = base::ToLowerASCII(dom_code_string);
      if (lower == "escape")
        return "esc";
      if (lower == "shiftleft" || lower == "shiftright")
        return "shift";
      if (lower == "controlleft" || lower == "controlright")
        return "ctrl";
      if (lower == "altleft" || lower == "altright")
        return "alt";
      // TODO(cuicuiruan): adjust more display text according to UX design
      // requirement.
      return lower;
  }
}

ActionLabel::ActionLabel() : views::LabelButton() {
  SetRequestFocusOnPress(true);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, kSideInset)));
}

ActionLabel::~ActionLabel() = default;

// static
std::unique_ptr<ActionLabel> ActionLabel::CreateTextActionLabel(
    const std::string& text) {
  auto label = std::make_unique<ActionLabel>();
  label->SetTextActionLabel(text);
  return label;
}

// static
std::unique_ptr<ActionLabel> ActionLabel::CreateImageActionLabel(
    MouseAction mouse_action) {
  DCHECK(mouse_action == MouseAction::PRIMARY_CLICK ||
         mouse_action == MouseAction::SECONDARY_CLICK);
  if (mouse_action != MouseAction::PRIMARY_CLICK &&
      mouse_action != MouseAction::SECONDARY_CLICK) {
    return nullptr;
  }
  auto label = std::make_unique<ActionLabel>();
  label->SetImageActionLabel(mouse_action);
  return label;
}

void ActionLabel::SetTextActionLabel(const std::string& text) {
  label()->SetText(base::UTF8ToUTF16(text));
  SetAccessibleName(label()->GetText());
}

void ActionLabel::SetImageActionLabel(MouseAction mouse_action) {
  SetAccessibleName(base::UTF8ToUTF16(GetClassName()));
  set_mouse_action(mouse_action);
}

void ActionLabel::SetDisplayMode(DisplayMode mode) {
  DCHECK(mode != DisplayMode::kMenu);
  if (mode == DisplayMode::kMenu)
    return;

  switch (mode) {
    case DisplayMode::kView:
      SetToViewMode();
      break;
    case DisplayMode::kEdit:
      SetToEditMode();
      break;
    case DisplayMode::kEditedSuccess:
      SetToEditFocus();
      break;
    case DisplayMode::kEditedUnbound:
      SetToEditUnBind();
      break;
    case DisplayMode::kEditedError:
      SetToEditError();
      break;
    case DisplayMode::kRestore:
      if (!ClearFocus())
        SetToEditDefault();
      break;
    default:
      NOTREACHED();
      break;
  }
}

gfx::Size ActionLabel::CalculatePreferredSize() const {
  auto size = LabelButton::CalculatePreferredSize();
  switch (edit_state_) {
    case EditState::kNone:
    case EditState::kEditDefault:
    case EditState::kEditUnbind:
      size.SetToMax(kMinimumSmallSize);
      break;
    default:
      size.SetToMax(kMinimumLargeSize);
      break;
  }
  return size;
}

bool ActionLabel::OnKeyPressed(const ui::KeyEvent& event) {
  DCHECK(parent());
  auto code = event.code();
  auto* parent_view = static_cast<ActionView*>(parent());
  if (base::UTF8ToUTF16(GetDisplayText(code)) == GetText()) {
    parent_view->ShowErrorMsg(kEditErrorSameKey, this);
  } else {
    parent_view->OnKeyBindingChange(this, code);
  }
  return true;
}

void ActionLabel::OnMouseEntered(const ui::MouseEvent& event) {
  if (!HasFocus())
    SetToEditHover();
}

void ActionLabel::OnMouseExited(const ui::MouseEvent& event) {
  if (!HasFocus())
    SetToEditDefault();
}

void ActionLabel::OnFocus() {
  SetToEditFocus();
  LabelButton::OnFocus();
}

void ActionLabel::OnBlur() {
  SetToEditDefault();
  LabelButton::OnBlur();
}

bool ActionLabel::ClearFocus() {
  auto* focus_manager = GetFocusManager();
  bool has_focus = false;
  if (focus_manager) {
    has_focus = HasFocus();
    focus_manager->ClearFocus();
  }
  return has_focus;
}

void ActionLabel::SetToViewMode() {
  ClearFocus();
  SetInstallFocusRingOnFocus(false);
  label()->SetFontList(gfx::FontList({kFontSytle}, gfx::Font::NORMAL,
                                     kSmallFontSize, gfx::Font::Weight::BOLD));
  SetEnabledTextColors(kViewTextColor);

  if (mouse_action_ != MouseAction::NONE) {
    if (mouse_action_ == MouseAction::PRIMARY_CLICK) {
      auto left_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseLeftClickViewIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, left_click_icon);
    } else {
      auto right_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseRightClickViewIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, right_click_icon);
    }
  }

  SetBackground(
      views::CreateRoundedRectBackground(kViewModeBgColor, kCornerRadiusView));
  SetPreferredSize(CalculatePreferredSize());
}

void ActionLabel::SetToEditMode() {
  SetInstallFocusRingOnFocus(true);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kHaloInset);
  focus_ring->SetHaloThickness(kHaloThickness);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return view->IsMouseHovered() || view->HasFocus();
  });

  SetEnabledTextColors(kEditTextColor);

  if (mouse_action_ != MouseAction::NONE) {
    if (mouse_action_ == MouseAction::PRIMARY_CLICK) {
      auto left_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseLeftClickEditIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, left_click_icon);
    } else {
      auto right_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseRightClickEditIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, right_click_icon);
    }
  }
  SetToEditDefault();
}

void ActionLabel::SetToEditDefault() {
  edit_state_ = EditState::kEditDefault;
  label()->SetFontList(gfx::FontList({kFontSytle}, gfx::Font::NORMAL,
                                     kSmallFontSize, gfx::Font::Weight::BOLD));
  SetPreferredSize(CalculatePreferredSize());
  SetBackground(
      views::CreateRoundedRectBackground(kEditModeBgColor, kCornerRadiusView));
}

void ActionLabel::SetToEditHover() {
  edit_state_ = EditState::kEditHover;
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColor(kFocusRingGreyColor);
  SetPreferredSize(CalculatePreferredSize());
}

void ActionLabel::SetToEditFocus() {
  edit_state_ = EditState::kEditFocus;
  views::FocusRing::Get(this)->SetColor(kFocusRingBlueColor);
  label()->SetFontList(gfx::FontList({kFontSytle}, gfx::Font::NORMAL,
                                     kLargeFontSize, gfx::Font::Weight::BOLD));
  SetPreferredSize(CalculatePreferredSize());
  SetBackground(
      views::CreateRoundedRectBackground(kEditModeBgColor, kCornerRadiusView));
}

void ActionLabel::SetToEditError() {
  edit_state_ = EditState::kEditError;
  views::FocusRing::Get(this)->SetColor(kFocusRingRedColor);
}

void ActionLabel::SetToEditUnBind() {
  edit_state_ = EditState::kEditUnbind;
  SetPreferredSize(CalculatePreferredSize());
  SetBackground(views::CreateRoundedRectBackground(kEditedUnboundBgColor,
                                                   kCornerRadiusView));
}

}  // namespace input_overlay
}  // namespace arc
