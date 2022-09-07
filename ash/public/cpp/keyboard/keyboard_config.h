// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_CONFIG_H_
#define ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_CONFIG_H_

#include <tuple>

#include "ash/public/cpp/ash_public_export.h"

namespace keyboard {

// Determines how the keyboard overscroll enabled state is set.
enum class KeyboardOverscrollBehavior {
  // Use the default behavior.
  kDefault,

  // Enable keyboard overscroll if allowed.
  kEnabled,

  // Do not enable keyboard overscroll.
  kDisabled,
};

struct KeyboardConfig {
  // Whether the virtual keyboard can provide auto-complete.
  bool auto_complete = true;

  // Whether the virtual keyboard can provide auto-correct.
  bool auto_correct = true;

  // Whether the virtual keyboard can provide auto-capitalization.
  bool auto_capitalize = true;

  // Whether the virtual keyboard can provide input via handwriting recognition.
  bool handwriting = true;

  // Whether the virtual keyboard can provide spell-check.
  bool spell_check = true;

  // Whether the virtual keyboard can provide voice input.
  bool voice_input = true;

  // Whether overscroll is currently allowed by the active keyboard container.
  KeyboardOverscrollBehavior overscroll_behavior =
      KeyboardOverscrollBehavior::kDefault;

  bool operator==(const KeyboardConfig& other) const {
    return std::tie(auto_complete, auto_correct, auto_capitalize, handwriting,
                    spell_check, voice_input, overscroll_behavior) ==
           std::tie(other.auto_complete, other.auto_correct,
                    other.auto_capitalize, other.handwriting, other.spell_check,
                    other.voice_input, other.overscroll_behavior);
  }

  bool operator!=(const KeyboardConfig& other) const {
    return !(*this == other);
  }
};

}  // namespace keyboard

#endif  // ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_CONFIG_H_
