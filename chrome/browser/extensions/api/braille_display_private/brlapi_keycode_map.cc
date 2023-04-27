// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/brlapi_keycode_map.h"

#include <stdint.h>

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"

namespace extensions {
namespace api {
namespace braille_display_private {

namespace {
// Bitmask for all braille dots in a key command argument, which coincides
// with the representation in the braille_dots member of the KeyEvent
// class.
const int kAllDots = BRLAPI_DOT1 | BRLAPI_DOT2 | BRLAPI_DOT3 | BRLAPI_DOT4 |
                     BRLAPI_DOT5 | BRLAPI_DOT6 | BRLAPI_DOT7 | BRLAPI_DOT8;

// Maximum Latin 1 character keyboard symbol.
const brlapi_keyCode_t kMaxLatin1KeySym = 0xff;

// Range of function keys that we support.
// See ui/events/keycodes/dom/dom_code_data.inc for the list of all
// key codes.
const brlapi_keyCode_t kMinFunctionKey = BRLAPI_KEY_SYM_FUNCTION;
const brlapi_keyCode_t kMaxFunctionKey = BRLAPI_KEY_SYM_FUNCTION + 23;

// Maps the keyboard modifier flags to their corresponding flags in a
// |KeyEvent|.
void MapModifierFlags(brlapi_keyCode_t code, KeyEvent* event) {
  if (code & BRLAPI_KEY_FLG_CONTROL)
    event->ctrl_key = true;
  if (code & BRLAPI_KEY_FLG_META)
    event->alt_key = true;
  if (code & BRLAPI_KEY_FLG_SHIFT)
    event->shift_key = true;
}

// Maps a brlapi keysym, which is similar to an X keysym into the
// provided event.
// See ui/events/keycodes/dom/dom_code_data.cc for the full
// list of key codes.
void MapKeySym(brlapi_keyCode_t code, KeyEvent* event) {
  brlapi_keyCode_t key_sym = code & BRLAPI_KEY_CODE_MASK;
  if (key_sym < kMaxLatin1KeySym ||
      (key_sym & BRLAPI_KEY_SYM_UNICODE) != 0) {
    base_icu::UChar32 code_point = key_sym & ~BRLAPI_KEY_SYM_UNICODE;
    if (!base::IsValidCharacter(code_point))
      return;
    event->standard_key_char.emplace();
    base::WriteUnicodeCharacter(code_point, &*event->standard_key_char);
  } else if (key_sym >= kMinFunctionKey && key_sym <= kMaxFunctionKey) {
    // Function keys are 0-based here, so we need to add one to get e.g.
    // 'F1' for the first key.
    int function_key_number = key_sym - kMinFunctionKey + 1;
    event->standard_key_code = base::StringPrintf("F%d", function_key_number);
  } else {
    // Explicitly map the keys that brlapi provides.
    const char* code_string;
    switch (key_sym) {
      case BRLAPI_KEY_SYM_BACKSPACE:
        code_string = "Backspace";
        break;
      case BRLAPI_KEY_SYM_TAB:
        code_string = "Tab";
        break;
      case BRLAPI_KEY_SYM_LINEFEED:
        code_string = "Enter";
        break;
      case BRLAPI_KEY_SYM_ESCAPE:
        code_string = "Escape";
        break;
      case BRLAPI_KEY_SYM_HOME:
        code_string = "Home";
        break;
      case BRLAPI_KEY_SYM_LEFT:
        code_string = "ArrowLeft";
        break;
      case BRLAPI_KEY_SYM_UP:
        code_string = "ArrowUp";
        break;
      case BRLAPI_KEY_SYM_RIGHT:
        code_string = "ArrowRight";
        break;
      case BRLAPI_KEY_SYM_DOWN:
        code_string = "ArrowDown";
        break;
      case BRLAPI_KEY_SYM_PAGE_UP:
        code_string = "PageUp";
        break;
      case BRLAPI_KEY_SYM_PAGE_DOWN:
        code_string = "PageDown";
        break;
      case BRLAPI_KEY_SYM_END:
        code_string = "End";
        break;
      case BRLAPI_KEY_SYM_INSERT:
        code_string = "Insert";
        break;
      case BRLAPI_KEY_SYM_DELETE:
        code_string = "Delete";
        break;
      default:
        return;
    }
    event->standard_key_code = code_string;
  }
  MapModifierFlags(code, event);
  event->command = KeyCommand::kStandardKey;
}

void MapCommand(brlapi_keyCode_t code, KeyEvent* event) {
  brlapi_keyCode_t argument = code & BRLAPI_KEY_CMD_ARG_MASK;
  switch (code & BRLAPI_KEY_CODE_MASK) {
    case BRLAPI_KEY_CMD_LNUP:
      event->command = KeyCommand::kLineUp;
      break;
    case BRLAPI_KEY_CMD_LNDN:
      event->command = KeyCommand::kLineDown;
      break;
    case BRLAPI_KEY_CMD_FWINLT:
      event->command = KeyCommand::kPanLeft;
      break;
    case BRLAPI_KEY_CMD_FWINRT:
      event->command = KeyCommand::kPanRight;
      break;
    case BRLAPI_KEY_CMD_TOP:
      event->command = KeyCommand::kTop;
      break;
    case BRLAPI_KEY_CMD_BOT:
      event->command = KeyCommand::kBottom;
      break;
    default:
      switch (code & BRLAPI_KEY_CMD_BLK_MASK) {
        case BRLAPI_KEY_CMD_ROUTE:
          event->command = KeyCommand::kRouting;
          event->display_position = argument;
          break;
        case BRLAPI_KEY_CMD_PASSDOTS:
          unsigned int dots = argument & kAllDots;
          event->braille_dots = dots;

          // BRLAPI_DOTC represents when the braille space key is pressed.
          if (dots && (argument & BRLAPI_DOTC))
            event->command = KeyCommand::kChord;
          else
            event->command = KeyCommand::kDots;
          MapModifierFlags(code, event);
          break;
      }
  }
}

}  // namespace

std::unique_ptr<KeyEvent> BrlapiKeyCodeToEvent(brlapi_keyCode_t code) {
  std::unique_ptr<KeyEvent> result(new KeyEvent);
  result->command = KeyCommand::kNone;
  switch (code & BRLAPI_KEY_TYPE_MASK) {
    case BRLAPI_KEY_TYPE_SYM:
      MapKeySym(code, result.get());
      break;
    case BRLAPI_KEY_TYPE_CMD:
      MapCommand(code, result.get());
      break;
  }
  if (result->command == KeyCommand::kNone) {
    result.reset();
  }
  return result;
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions
