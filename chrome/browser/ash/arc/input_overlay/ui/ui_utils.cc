// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

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

int GetIndexOfActionName(const std::vector<std::u16string>& action_names,
                         const std::u16string& action_name) {
  auto it = std::find(action_names.begin(), action_names.end(), action_name);
  return it == action_names.end() ? -1 : it - action_names.begin();
}

std::u16string GetActionNameAtIndex(
    const std::vector<std::u16string>& action_names,
    int index) {
  if (index < 0 || index >= static_cast<int>(action_names.size())) {
    // TODO(b/274690042): Replace placeholder text with localized strings.
    return u"Unassigned";
  }
  return action_names[index];
}

}  // namespace arc::input_overlay
