// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "base/numerics/safe_conversions.h"
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

gfx::Rect CalculateAvailableBounds(aura::Window* root_window) {
  DCHECK(root_window->IsRootWindow());

  const auto* shelf =
      ash::RootWindowController::ForWindow(root_window)->shelf();
  DCHECK(shelf);
  if (!shelf->IsVisible()) {
    return root_window->bounds();
  }

  int x = 0, y = 0;
  int width = root_window->bounds().width();
  int height = root_window->bounds().height();
  const int shelf_size = ash::ShelfConfig::Get()->shelf_size();
  if (shelf->alignment() == ash::ShelfAlignment::kLeft) {
    x += shelf_size;
    width -= shelf_size;
  } else if (shelf->alignment() == ash::ShelfAlignment::kRight) {
    width -= shelf_size;
  } else {
    // Include `kBottom` and `kBottomLocked`. Shelf has no alignment on top.
    height -= shelf_size;
  }
  return gfx::Rect(x, y, width, height);
}

SkAlpha GetAlpha(float opacity_percent) {
  return base::saturated_cast<SkAlpha>(std::numeric_limits<SkAlpha>::max() *
                                       opacity_percent);
}

bool OffsetPositionByArrowKey(ui::KeyboardCode key, gfx::Point& position) {
  switch (key) {
    case ui::VKEY_LEFT:
      position.Offset(-kArrowKeyMoveDistance, 0);
      break;
    case ui::VKEY_RIGHT:
      position.Offset(kArrowKeyMoveDistance, 0);
      break;
    case ui::VKEY_UP:
      position.Offset(0, -kArrowKeyMoveDistance);
      break;
    case ui::VKEY_DOWN:
      position.Offset(0, kArrowKeyMoveDistance);
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace arc::input_overlay
