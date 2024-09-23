// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sparky/keyboard_util.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_util.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {
namespace {

// A mapping from the typeable characters on a US keyboard, to a pair describing
// how to type that character. The pair contains a) the key code and b) any
// modifiers. Modifiers are encoded as a bitset and 0 means 'no modifiers'.
constexpr auto kKeyboardCodeForCharacter =
    base::MakeFixedFlatMap<char, std::pair<ui::KeyboardCode, int>>({
        {' ', {ui::VKEY_SPACE, 0}},
        {'\t', {ui::VKEY_TAB, 0}},
        {'\n', {ui::VKEY_RETURN, 0}},

        {'a', {ui::VKEY_A, 0}},
        {'b', {ui::VKEY_B, 0}},
        {'c', {ui::VKEY_C, 0}},
        {'d', {ui::VKEY_D, 0}},
        {'e', {ui::VKEY_E, 0}},
        {'f', {ui::VKEY_F, 0}},
        {'g', {ui::VKEY_G, 0}},
        {'h', {ui::VKEY_H, 0}},
        {'i', {ui::VKEY_I, 0}},
        {'j', {ui::VKEY_J, 0}},
        {'k', {ui::VKEY_K, 0}},
        {'l', {ui::VKEY_L, 0}},
        {'m', {ui::VKEY_M, 0}},
        {'n', {ui::VKEY_N, 0}},
        {'o', {ui::VKEY_O, 0}},
        {'p', {ui::VKEY_P, 0}},
        {'q', {ui::VKEY_Q, 0}},
        {'r', {ui::VKEY_R, 0}},
        {'s', {ui::VKEY_S, 0}},
        {'t', {ui::VKEY_T, 0}},
        {'u', {ui::VKEY_U, 0}},
        {'v', {ui::VKEY_V, 0}},
        {'w', {ui::VKEY_W, 0}},
        {'x', {ui::VKEY_X, 0}},
        {'y', {ui::VKEY_Y, 0}},
        {'z', {ui::VKEY_Z, 0}},

        {'A', {ui::VKEY_A, ui::EF_SHIFT_DOWN}},
        {'B', {ui::VKEY_B, ui::EF_SHIFT_DOWN}},
        {'C', {ui::VKEY_C, ui::EF_SHIFT_DOWN}},
        {'D', {ui::VKEY_D, ui::EF_SHIFT_DOWN}},
        {'E', {ui::VKEY_E, ui::EF_SHIFT_DOWN}},
        {'F', {ui::VKEY_F, ui::EF_SHIFT_DOWN}},
        {'G', {ui::VKEY_G, ui::EF_SHIFT_DOWN}},
        {'H', {ui::VKEY_H, ui::EF_SHIFT_DOWN}},
        {'I', {ui::VKEY_I, ui::EF_SHIFT_DOWN}},
        {'J', {ui::VKEY_J, ui::EF_SHIFT_DOWN}},
        {'K', {ui::VKEY_K, ui::EF_SHIFT_DOWN}},
        {'L', {ui::VKEY_L, ui::EF_SHIFT_DOWN}},
        {'M', {ui::VKEY_M, ui::EF_SHIFT_DOWN}},
        {'N', {ui::VKEY_N, ui::EF_SHIFT_DOWN}},
        {'O', {ui::VKEY_O, ui::EF_SHIFT_DOWN}},
        {'P', {ui::VKEY_P, ui::EF_SHIFT_DOWN}},
        {'Q', {ui::VKEY_Q, ui::EF_SHIFT_DOWN}},
        {'R', {ui::VKEY_R, ui::EF_SHIFT_DOWN}},
        {'S', {ui::VKEY_S, ui::EF_SHIFT_DOWN}},
        {'T', {ui::VKEY_T, ui::EF_SHIFT_DOWN}},
        {'U', {ui::VKEY_U, ui::EF_SHIFT_DOWN}},
        {'V', {ui::VKEY_V, ui::EF_SHIFT_DOWN}},
        {'W', {ui::VKEY_W, ui::EF_SHIFT_DOWN}},
        {'X', {ui::VKEY_X, ui::EF_SHIFT_DOWN}},
        {'Y', {ui::VKEY_Y, ui::EF_SHIFT_DOWN}},
        {'Z', {ui::VKEY_Z, ui::EF_SHIFT_DOWN}},

        {'1', {ui::VKEY_1, 0}},
        {'2', {ui::VKEY_2, 0}},
        {'3', {ui::VKEY_3, 0}},
        {'4', {ui::VKEY_4, 0}},
        {'5', {ui::VKEY_5, 0}},
        {'6', {ui::VKEY_6, 0}},
        {'7', {ui::VKEY_7, 0}},
        {'8', {ui::VKEY_8, 0}},
        {'9', {ui::VKEY_9, 0}},
        {'0', {ui::VKEY_0, 0}},
        {'-', {ui::VKEY_OEM_MINUS, 0}},
        {'=', {ui::VKEY_OEM_PLUS, 0}},

        {'!', {ui::VKEY_1, ui::EF_SHIFT_DOWN}},
        {'@', {ui::VKEY_2, ui::EF_SHIFT_DOWN}},
        {'#', {ui::VKEY_3, ui::EF_SHIFT_DOWN}},
        {'$', {ui::VKEY_4, ui::EF_SHIFT_DOWN}},
        {'%', {ui::VKEY_5, ui::EF_SHIFT_DOWN}},
        {'^', {ui::VKEY_6, ui::EF_SHIFT_DOWN}},
        {'&', {ui::VKEY_7, ui::EF_SHIFT_DOWN}},
        {'*', {ui::VKEY_8, ui::EF_SHIFT_DOWN}},
        {'(', {ui::VKEY_9, ui::EF_SHIFT_DOWN}},
        {')', {ui::VKEY_0, ui::EF_SHIFT_DOWN}},
        {'_', {ui::VKEY_OEM_MINUS, ui::EF_SHIFT_DOWN}},
        {'+', {ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN}},

        {'`', {ui::VKEY_OEM_3, 0}},
        {',', {ui::VKEY_OEM_COMMA, 0}},
        {'.', {ui::VKEY_OEM_PERIOD, 0}},
        {'/', {ui::VKEY_OEM_2, 0}},
        {';', {ui::VKEY_OEM_1, 0}},
        {'\'', {ui::VKEY_OEM_7, 0}},
        {'[', {ui::VKEY_OEM_4, 0}},
        {']', {ui::VKEY_OEM_6, 0}},
        {'\\', {ui::VKEY_OEM_5, 0}},

        {'~', {ui::VKEY_OEM_3, ui::EF_SHIFT_DOWN}},
        {'<', {ui::VKEY_OEM_COMMA, ui::EF_SHIFT_DOWN}},
        {'>', {ui::VKEY_OEM_PERIOD, ui::EF_SHIFT_DOWN}},
        {'?', {ui::VKEY_OEM_2, ui::EF_SHIFT_DOWN}},
        {':', {ui::VKEY_OEM_1, ui::EF_SHIFT_DOWN}},
        {'"', {ui::VKEY_OEM_7, ui::EF_SHIFT_DOWN}},
        {'{', {ui::VKEY_OEM_4, ui::EF_SHIFT_DOWN}},
        {'}', {ui::VKEY_OEM_6, ui::EF_SHIFT_DOWN}},
        {'|', {ui::VKEY_OEM_5, ui::EF_SHIFT_DOWN}},
    });

// A mapping from the lowercased versions of a subset of strings defined in:
//
//   https://www.w3.org/TR/uievents-key/
//
// to their ui::KeyboardCodes.
constexpr auto kKeyboardCodeForDOMString =
    base::MakeFixedFlatMap<std::string, ui::KeyboardCode>({
        {"tab", ui::VKEY_TAB},
        {"enter", ui::VKEY_RETURN},
        {"space", ui::VKEY_SPACE},

        {"arrowleft", ui::VKEY_LEFT},
        {"arrowright", ui::VKEY_RIGHT},
        {"arrowdown", ui::VKEY_DOWN},
        {"arrowup", ui::VKEY_UP},
    });

}  // namespace

std::pair<ui::KeyEvent, ui::KeyEvent> MakeKeyEventPair(
    ui::KeyboardCode key_code,
    bool control,
    bool alt,
    bool shift) {
  const auto dom_code = ui::UsLayoutKeyboardCodeToDomCode(key_code);

  int modifiers = 0;
  if (control) {
    modifiers |= ui::EF_CONTROL_DOWN;
  }
  if (alt) {
    modifiers |= ui::EF_ALT_DOWN;
  }
  if (shift) {
    modifiers |= ui::EF_SHIFT_DOWN;
  }

  return {
      ui::KeyEvent(ui::EventType::kKeyPressed, key_code, dom_code, modifiers),
      ui::KeyEvent(ui::EventType::kKeyReleased, key_code, dom_code, modifiers)};
}

std::optional<std::vector<ui::KeyEvent>> KeyEventsForText(
    const std::string& text) {
  std::vector<ui::KeyEvent> events;
  for (const char& character : text) {
    const auto it = kKeyboardCodeForCharacter.find(character);
    if (it == kKeyboardCodeForCharacter.end()) {
      return std::nullopt;
    }

    const auto key_code = it->second.first;
    const auto modifier = it->second.second;
    const auto pressed_released =
        MakeKeyEventPair(key_code, false, false, modifier);

    events.push_back(pressed_released.first);
    events.push_back(pressed_released.second);
  }

  return events;
}

std::optional<ui::KeyboardCode> KeyboardCodeForDOMString(
    const std::string& key) {
  if (!base::IsStringASCII(key)) {
    return std::nullopt;
  }

  const auto it = kKeyboardCodeForDOMString.find(base::ToLowerASCII(key));
  if (it != kKeyboardCodeForDOMString.end()) {
    return it->second;
  }
  return std::nullopt;
}

}  // namespace ash
