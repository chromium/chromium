// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/views/background.h"

namespace arc::input_overlay {

namespace {

// Arrow symbols for arrow keys.
constexpr char16_t kLeftArrow[] = u"←";
constexpr char16_t kUpArrow[] = u"↑";
constexpr char16_t kRightArrow[] = u"→";
constexpr char16_t kDownArrow[] = u"↓";
constexpr char16_t kBackQuote[] = u"`";
constexpr char16_t kMinus[] = u"-";
constexpr char16_t kEqual[] = u"=";
constexpr char16_t kBracketLeft[] = u"[";
constexpr char16_t kBracketRight[] = u"]";
constexpr char16_t kBackSlash[] = u"\\";
constexpr char16_t kSemicolon[] = u";";
constexpr char16_t kQuote[] = u"'";
constexpr char16_t kComma[] = u",";
constexpr char16_t kPeriod[] = u".";
constexpr char16_t kSlash[] = u"/";
constexpr char16_t kBackSpace[] = u"⌫";
constexpr char16_t kEnter[] = u"↵";
constexpr char16_t kSpace[] = u"␣";
constexpr char16_t kEscape[] = u"esc";

// Modifier keys.
constexpr char16_t kAlt[] = u"alt";
constexpr char16_t kCtrl[] = u"ctrl";
constexpr char16_t kShift[] = u"shift";
constexpr char16_t kCap[] = u"cap";

// Specifications for container with arrow background.
constexpr int kMenuWidth = 316;
constexpr int kTriangleLength = 20;
constexpr int kTriangleHeight = 14;
constexpr int kCornerRadius = 16;
constexpr int kBorderThickness = 2;

// Draws the dialog shape path with round corner. It starts after the corner
// radius on line #0 and draws clockwise.
//
// draw_triangle_on_left draws the triangle wedge on the left side of the box
// instead of the right if set to true.
//
// action_offset draws the triangle wedge higher or lower if the position of
// the action is too close to the top or bottom of the window. An offset of
// zero draws the triangle wedge at the vertical center of the box.
//  _0>__________
// |             |
// |             |
// |             |
// |              >
// |             |
// |             |
// |_____________|
//
SkPath BackgroundPath(int height,
                      bool draw_triangle_on_left,
                      int action_offset) {
  SkPath path;
  int short_length = kMenuWidth - kTriangleHeight - 2 * kCornerRadius;
  int short_height = height - 2 * kCornerRadius;
  // If the offset is greater than the limit or less than the negative
  // limit, set it respectively.
  const int limit = short_height / 2 - kTriangleLength / 2;
  if (action_offset > limit) {
    action_offset = limit;
  } else if (action_offset < -limit) {
    action_offset = -limit;
  }
  if (draw_triangle_on_left) {
    path.moveTo(kCornerRadius + kTriangleHeight, 0);
  } else {
    path.moveTo(kCornerRadius, 0);
  }
  // Top left after corner radius to top right corner radius.
  path.rLineTo(short_length, 0);
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, +kCornerRadius, +kCornerRadius);
  if (draw_triangle_on_left) {
    // Top right after corner radius to bottom right corner radius.
    path.rLineTo(0, short_height);
  } else {
    // Top right after corner radius to midway point.
    path.rLineTo(0, limit + action_offset);
    // Triangle shape.
    path.rLineTo(kTriangleHeight, kTriangleLength / 2);
    path.rLineTo(-kTriangleHeight, kTriangleLength / 2);
    // After midway point to bottom right corner radius.
    path.rLineTo(0, limit - action_offset);
  }
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -kCornerRadius, +kCornerRadius);
  // Bottom right after corner radius to bottom left corner radius.
  path.rLineTo(-short_length, 0);
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -kCornerRadius, -kCornerRadius);
  if (draw_triangle_on_left) {
    // bottom left after corner radius to midway point.
    path.rLineTo(0, -limit + action_offset);
    // Triangle shape.
    path.rLineTo(-kTriangleHeight, -kTriangleLength / 2);
    path.rLineTo(kTriangleHeight, -kTriangleLength / 2);
    // After midway point to bottom right corner radius.
    path.rLineTo(0, -limit - action_offset);
  } else {
    // Bottom left after corner radius to top left corner radius.
    path.rLineTo(0, -short_height);
  }
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, +kCornerRadius, -kCornerRadius);
  // Path finish.
  path.close();
  return path;
}

}  // namespace

std::u16string GetDisplayText(const ui::DomCode code) {
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
    case ui::DomCode::SPACE:
      return kSpace;
    default:
      std::string dom_code_string =
          ui::KeycodeConverter::DomCodeToCodeString(code);
      if (base::StartsWith(dom_code_string, "Key",
                           base::CompareCase::SENSITIVE)) {
        return base::UTF8ToUTF16(base::ToLowerASCII(dom_code_string.substr(3)));
      }
      if (base::StartsWith(dom_code_string, "Digit",
                           base::CompareCase::SENSITIVE)) {
        return base::UTF8ToUTF16(dom_code_string.substr(5));
      }
      // TODO(b/282843422): better display for number pad. Current it shows in
      // the format of "numpad1" since the number keys on number pad are not
      // considered the same as numbers on the main keyboard.
      auto lower = base::ToLowerASCII(dom_code_string);
      return base::UTF8ToUTF16(lower);
  }
}

std::u16string GetDisplayTextAccessibleName(const std::u16string& text) {
  if (text.compare(kSpace) == 0) {
    return l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_KEY_LABEL_SPACE);
  } else if (text.compare(kEnter) == 0) {
    return l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_KEY_LABEL_ENTER);
  } else if (text.compare(kBackSpace) == 0) {
    return l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_KEY_LABEL_BACKSPACE);
  } else {
    return text;
  }
}

void DrawBackgroundContainerWithArrow(gfx::Canvas* canvas,
                                      int height,
                                      bool arrow_on_left,
                                      int arrow_height_offset,
                                      SkColor background_color,
                                      SkColor border_color) {
  cc::PaintFlags flags;
  // Draw the shape.
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(background_color);
  canvas->DrawPath(BackgroundPath(height, arrow_on_left, arrow_height_offset),
                   flags);
  // Draw the border.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  // TODO(b/270969760): Change to "sys.BorderHighlight1" when added.
  flags.setColor(border_color);
  flags.setStrokeWidth(kBorderThickness);
  canvas->DrawPath(BackgroundPath(height, arrow_on_left, arrow_height_offset),
                   flags);
}

}  // namespace arc::input_overlay
