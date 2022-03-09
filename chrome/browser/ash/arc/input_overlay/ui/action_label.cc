// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"

#include <set>

#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"

namespace arc {
namespace input_overlay {
namespace {
// UI specs.
constexpr int kWidthPadding = 10;
constexpr gfx::Size kMinimumLabelSize(32, 32);
constexpr int kCornerRadius = 6;

constexpr SkColor kViewModeBgColor = SkColorSetA(SK_ColorGRAY, 0x99);
constexpr SkColor kEditModeBgColor = SK_ColorWHITE;
constexpr SkColor kViewTextColor = SK_ColorWHITE;
constexpr SkColor kEditTextColor = gfx::kGoogleGrey900;

constexpr char kFontSytle[] = "Google Sans";
constexpr int kViewFontSize = 16;
constexpr int kUnFocusFontSize = 16;
constexpr int kFocusFontSize = 20;
}  // namespace

std::string GetDisplayText(const ui::DomCode code) {
  std::string dom_code_string = ui::KeycodeConverter::DomCodeToCodeString(code);
  if (base::StartsWith(dom_code_string, "Key", base::CompareCase::SENSITIVE))
    return base::ToLowerASCII(dom_code_string.substr(3));
  if (base::StartsWith(dom_code_string, "Digit", base::CompareCase::SENSITIVE))
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

ActionLabel::ActionLabel() : views::Label() {}
ActionLabel::ActionLabel(const std::u16string& text) : views::Label(text) {
  SetDisplayMode(DisplayMode::kView);
}

ActionLabel::~ActionLabel() = default;

void ActionLabel::SetDisplayMode(DisplayMode mode) {
  switch (mode) {
    case DisplayMode::kMenu:
    case DisplayMode::kView:
      SetToView();
      break;
    case DisplayMode::kEdit:
      SetToEditDefault();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ActionLabel::SetPositionFromCenterPosition(gfx::PointF& center_position) {
  auto size = GetPreferredSize();
  SetSize(size);
  int left = std::max(0, (int)(center_position.x() - size.width() / 2));
  int top = std::max(0, (int)(center_position.y() - size.height() / 2));
  // SetPosition function needs the top-left position.
  SetPosition(gfx::Point(left, top));
}

gfx::Size ActionLabel::CalculatePreferredSize() const {
  auto size = Label::CalculatePreferredSize();
  size.set_width(size.width() + kWidthPadding);
  size.SetToMax(kMinimumLabelSize);
  return size;
}

void ActionLabel::OnKeyEvent(ui::KeyEvent* event) {
  // TODO(cuicuiruan): Change the key mapping according to the key input.
}

void ActionLabel::OnFocus() {
  SetToEditFocus();
  SelectAll();
  Label::OnFocus();
  static_cast<ActionView*>(parent())->RemoveEditMenu();
}

void ActionLabel::OnBlur() {
  SetToEditDefault();
  ClearSelection();
  Label::OnBlur();
}

void ActionLabel::SetToView() {
  SetFontList(gfx::FontList({kFontSytle}, gfx::Font::NORMAL, kViewFontSize,
                            gfx::Font::Weight::BOLD));
  SetFocusBehavior(FocusBehavior::NEVER);
  SetBackground(
      views::CreateRoundedRectBackground(kViewModeBgColor, kCornerRadius));
  SetAutoColorReadabilityEnabled(false);
  SetEnabledColor(kViewTextColor);
  SetSelectable(false);
  SetEnabled(false);
}

void ActionLabel::SetToEditDefault() {
  SetFontList(gfx::FontList({kFontSytle}, gfx::Font::NORMAL, kUnFocusFontSize,
                            gfx::Font::Weight::BOLD));
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetBackground(
      views::CreateRoundedRectBackground(kEditModeBgColor, kCornerRadius));
  SetEnabledColor(kEditTextColor);
  SetSelectable(true);
  SetEnabled(true);
}

void ActionLabel::SetToEditFocus() {
  SetFontList(gfx::FontList({kFontSytle}, gfx::Font::NORMAL, kFocusFontSize,
                            gfx::Font::Weight::BOLD));
}

}  // namespace input_overlay
}  // namespace arc
