// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"

#include <set>

#include "ash/style/style_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace arc::input_overlay {
namespace {
// UI specs.
constexpr int kSideInset = 6;
constexpr gfx::Size kLabelSize(32, 32);
constexpr int kCornerRadiusView = 6;
constexpr int kIconSize = 20;
constexpr char kFontStyle[] = "Google Sans";
constexpr int kFontSize = 16;

// About colors.
constexpr SkColor kViewModeForeColor = SkColorSetA(SK_ColorBLACK, 0x29);
constexpr SkColor kViewModeBackColor = SkColorSetA(gfx::kGoogleGrey800, 0xCC);
constexpr SkColor kEditModeBgColor = SK_ColorWHITE;
constexpr SkColor kEditedUnboundBgColor = gfx::kGoogleRed300;
constexpr SkColor kViewTextColor = SK_ColorWHITE;
constexpr SkColor kEditTextColor = gfx::kGoogleGrey900;

// About focus ring.
// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -6;
// Thickness of focus ring.
constexpr float kHaloThickness = 4;

// Arrow symbols for arrow keys.
constexpr char kLeftArrow[] = "←";
constexpr char kUpArrow[] = "↑";
constexpr char kRightArrow[] = "→";
constexpr char kDownArrow[] = "↓";
constexpr char kBackQuote[] = "`";
constexpr char kMinus[] = "-";
constexpr char kEqual[] = "=";
constexpr char kBracketLeft[] = "[";
constexpr char kBracketRight[] = "]";
constexpr char kBackSlash[] = "\\";
constexpr char kSemicolon[] = ";";
constexpr char kQuote[] = "'";
constexpr char kComma[] = ",";
constexpr char kPeriod[] = ".";
constexpr char kSlash[] = "/";
constexpr char kBackSpace[] = "back";
constexpr char kEnter[] = "enter";
constexpr char kEscape[] = "esc";

// Modifier keys.
constexpr char kAlt[] = "alt";
constexpr char kCtrl[] = "ctrl";
constexpr char kShift[] = "shift";
constexpr char kCap[] = "cap";
}  // namespace

std::string GetDisplayText(const ui::DomCode code) {
  switch (code) {
    case ui::DomCode::NONE:
      return kUnknownBind;
    case ui::DomCode::ARROW_LEFT:
      return kLeftArrow;
    case ui::DomCode::ARROW_RIGHT:
      return kRightArrow;
    case ui::DomCode::ARROW_UP:
      return kUpArrow;
    case ui::DomCode::ARROW_DOWN:
      return kDownArrow;
    case ui::DomCode::BACKQUOTE:
      return kBackQuote;
    case ui::DomCode::MINUS:
      return kMinus;
    case ui::DomCode::EQUAL:
      return kEqual;
    case ui::DomCode::BRACKET_LEFT:
      return kBracketLeft;
    case ui::DomCode::BRACKET_RIGHT:
      return kBracketRight;
    case ui::DomCode::BACKSLASH:
      return kBackSlash;
    case ui::DomCode::SEMICOLON:
      return kSemicolon;
    case ui::DomCode::QUOTE:
      return kQuote;
    case ui::DomCode::COMMA:
      return kComma;
    case ui::DomCode::PERIOD:
      return kPeriod;
    case ui::DomCode::SLASH:
      return kSlash;
    case ui::DomCode::BACKSPACE:
      return kBackSpace;
    case ui::DomCode::ENTER:
      return kEnter;
    case ui::DomCode::ESCAPE:
      return kEscape;
    // Modifier keys.
    case ui::DomCode::ALT_LEFT:
    case ui::DomCode::ALT_RIGHT:
      return kAlt;
    case ui::DomCode::CONTROL_LEFT:
    case ui::DomCode::CONTROL_RIGHT:
      return kCtrl;
    case ui::DomCode::SHIFT_LEFT:
    case ui::DomCode::SHIFT_RIGHT:
      return kShift;
    case ui::DomCode::CAPS_LOCK:
      return kCap;
    default:
      std::string dom_code_string =
          ui::KeycodeConverter::DomCodeToCodeString(code);
      if (base::StartsWith(dom_code_string, "Key",
                           base::CompareCase::SENSITIVE))
        return base::ToLowerASCII(dom_code_string.substr(3));
      if (base::StartsWith(dom_code_string, "Digit",
                           base::CompareCase::SENSITIVE))
        return dom_code_string.substr(5);
      // TODO(cuicuiruan): better display for number pad. Current it shows in
      // the format of "numpad1" since the number keys on number pad are not
      // considered the same as numbers on the main keyboard.
      auto lower = base::ToLowerASCII(dom_code_string);
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
  DCHECK(mode != DisplayMode::kMenu && mode != DisplayMode::kPreMenu);
  if (mode == DisplayMode::kMenu || mode == DisplayMode::kPreMenu)
    return;

  switch (mode) {
    case DisplayMode::kView:
      SetToViewMode();
      SetFocusBehavior(FocusBehavior::NEVER);
      break;
    case DisplayMode::kEdit:
      SetToEditMode();
      SetFocusBehavior(FocusBehavior::ALWAYS);
      static_cast<ActionView*>(parent())->ShowInfoMsg(
          l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_INSTRUCTIONS), this);
      break;
    case DisplayMode::kEditedSuccess:
      SetToEditFocus();
      break;
    case DisplayMode::kEditedUnbound:
      SetToEditUnbindInput();
      break;
    case DisplayMode::kEditedError:
      SetToEditError();
      break;
    case DisplayMode::kRestore:
      SetToEditDefault();
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool ActionLabel::ClearFocus() {
  auto* focus_manager = GetFocusManager();
  bool has_focus = false;
  if (focus_manager) {
    has_focus = HasFocus();
    focus_manager->ClearFocus();

    // When it has to clear focus explicitly, set focused view back to its
    // parent, so it can find the focused view when Tab traversal key is
    // pressed.
    focus_manager->SetFocusedView(static_cast<ActionView*>(parent()));
  }
  return has_focus;
}

gfx::Size ActionLabel::CalculatePreferredSize() const {
  auto size = LabelButton::CalculatePreferredSize();
  size.SetToMax(kLabelSize);
  return size;
}

bool ActionLabel::OnKeyPressed(const ui::KeyEvent& event) {
  DCHECK(parent());
  auto code = event.code();
  auto* parent_view = static_cast<ActionView*>(parent());
  if (base::UTF8ToUTF16(GetDisplayText(code)) == GetText() ||
      parent_view->ShouldShowErrorMsg(code)) {
    return true;
  }

  parent_view->OnKeyBindingChange(this, code);
  return true;
}

void ActionLabel::OnMouseEntered(const ui::MouseEvent& event) {
  if (IsFocusable() && !HasFocus())
    SetToEditHover();
}

void ActionLabel::OnMouseExited(const ui::MouseEvent& event) {
  if (IsFocusable() && !HasFocus())
    SetToEditDefault();
}

void ActionLabel::OnFocus() {
  SetToEditFocus();
  LabelButton::OnFocus();
  if (IsInputUnbound()) {
    static_cast<ActionView*>(parent())->ShowErrorMsg(
        l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_MISSING_BINDING), this,
        /*ax_annouce=*/false);
  } else {
    static_cast<ActionView*>(parent())->ShowLabelFocusInfoMsg(
        l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_FOCUSED_KEY), this);
  }
}

void ActionLabel::OnBlur() {
  SetToEditDefault();
  LabelButton::OnBlur();
  static_cast<ActionView*>(parent())->RemoveMessage();
}

void ActionLabel::SetToViewMode() {
  if (IsInputUnbound()) {
    SetVisible(false);
    return;
  }
  ClearFocus();
  SetInstallFocusRingOnFocus(false);
  label()->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL, kFontSize,
                                     gfx::Font::Weight::BOLD));
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

  SetBackground(views::CreateRoundedRectBackground(
      color_utils::GetResultingPaintColor(kViewModeForeColor,
                                          kViewModeBackColor),
      kCornerRadiusView));
  SetPreferredSize(CalculatePreferredSize());
}

void ActionLabel::SetToEditMode() {
  if (IsInputUnbound())
    SetVisible(true);

  SetInstallFocusRingOnFocus(true);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kCornerRadiusView);
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
  label()->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL, kFontSize,
                                     gfx::Font::Weight::BOLD));
  views::FocusRing::Get(this)->SetColorId(absl::nullopt);
  if (IsInputUnbound()) {
    SetBackground(views::CreateRoundedRectBackground(kEditedUnboundBgColor,
                                                     kCornerRadiusView));
  } else {
    SetBackground(views::CreateRoundedRectBackground(kEditModeBgColor,
                                                     kCornerRadiusView));
  }
}

void ActionLabel::SetToEditHover() {
  views::FocusRing::Get(this)->SetColorId(
      ui::kColorAshActionLabelFocusRingHover);
}

void ActionLabel::SetToEditFocus() {
  label()->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL, kFontSize,
                                     gfx::Font::Weight::BOLD));
  SetPreferredSize(CalculatePreferredSize());
  if (IsInputUnbound()) {
    views::FocusRing::Get(this)->SetColorId(
        ui::kColorAshActionLabelFocusRingError);
    SetBackground(views::CreateRoundedRectBackground(kEditedUnboundBgColor,
                                                     kCornerRadiusView));
  } else {
    views::FocusRing::Get(this)->SetColorId(
        ui::kColorAshActionLabelFocusRingEdit);
    SetBackground(views::CreateRoundedRectBackground(kEditModeBgColor,
                                                     kCornerRadiusView));
  }
}

void ActionLabel::SetToEditError() {
  views::FocusRing::Get(this)->SetColorId(
      ui::kColorAshActionLabelFocusRingError);
}

void ActionLabel::SetToEditUnbindInput() {
  SetPreferredSize(CalculatePreferredSize());
  SetBackground(views::CreateRoundedRectBackground(kEditedUnboundBgColor,
                                                   kCornerRadiusView));
}

bool ActionLabel::IsInputUnbound() {
  return base::UTF16ToUTF8(GetText()) == kUnknownBind;
}

}  // namespace arc::input_overlay
