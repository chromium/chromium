// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"

#include <set>

#include "chrome/browser/ash/arc/input_overlay/ui/action_tag.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"

namespace arc {
namespace input_overlay {
namespace {
// UI specs.
constexpr int kWidthPadding = 10;
constexpr gfx::Size kMinimumViewLabelSize(32, 32);
constexpr int kCornerRadiusView = 6;

constexpr SkColor kViewModeBgColor = SkColorSetA(SK_ColorGRAY, 0x99);
constexpr SkColor kEditModeBgColor = SK_ColorWHITE;
constexpr SkColor kEditedUnboundBgColor = gfx::kGoogleRed300;
constexpr SkColor kViewTextColor = SK_ColorWHITE;
constexpr SkColor kEditTextColor = gfx::kGoogleGrey900;

constexpr char kFontSytle[] = "Google Sans";
constexpr int kViewFontSize = 16;
constexpr int kUnFocusFontSize = 16;
constexpr int kFocusFontSize = 20;

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

ActionLabel::ActionLabel() : views::Label() {}
ActionLabel::ActionLabel(const std::u16string& text) : views::Label(text) {
  SetToViewMode();
}

ActionLabel::~ActionLabel() = default;

gfx::Size ActionLabel::CalculatePreferredSize() const {
  auto size = Label::CalculatePreferredSize();
  size.set_width(size.width() + kWidthPadding);
  size.SetToMax(kMinimumViewLabelSize);
  return size;
}

void ActionLabel::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::ET_KEY_PRESSED)
    return;

  DCHECK(parent());
  auto code = event->code();
  auto* parent_view = static_cast<ActionTag*>(parent());
  if (base::UTF8ToUTF16(GetDisplayText(code)) == GetText())
    parent_view->ShowErrorMsg(kEditErrorSameKey);

  parent_view->OnKeyBindingChange(code);
}

void ActionLabel::OnFocus() {
  SetToEditFocus();
  SelectAll();
  Label::OnFocus();
}

void ActionLabel::OnBlur() {
  SetToEditMode();
  ClearSelection();
  Label::OnBlur();
}

void ActionLabel::SetToViewMode() {
  SetFontList(gfx::FontList({kFontSytle}, gfx::Font::NORMAL, kViewFontSize,
                            gfx::Font::Weight::BOLD));
  SetFocusBehavior(FocusBehavior::NEVER);
  SetBackground(
      views::CreateRoundedRectBackground(kViewModeBgColor, kCornerRadiusView));
  SetAutoColorReadabilityEnabled(false);
  SetEnabledColor(kViewTextColor);
  SetSize(GetPreferredSize());
  SetSelectable(false);
  SetEnabled(false);
}

void ActionLabel::SetToEditMode() {
  SetFontList(gfx::FontList({kFontSytle}, gfx::Font::NORMAL, kUnFocusFontSize,
                            gfx::Font::Weight::BOLD));
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetBackground(
      views::CreateRoundedRectBackground(kEditModeBgColor, kCornerRadiusView));
  SetEnabledColor(kEditTextColor);
  SetSize(GetPreferredSize());
  SetSelectable(true);
  SetEnabled(true);
}

void ActionLabel::SetToEditedUnBind() {
  SetBackground(views::CreateRoundedRectBackground(kEditedUnboundBgColor,
                                                   kCornerRadiusView));
  SetSize(GetPreferredSize());
  SetSelectable(true);
  SetEnabled(true);
}

void ActionLabel::SetToEditFocus() {
  SetFontList(gfx::FontList({kFontSytle}, gfx::Font::NORMAL, kFocusFontSize,
                            gfx::Font::Weight::BOLD));
}

}  // namespace input_overlay
}  // namespace arc
