// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_MODIFIER_KEY_COMBO_RECORDER_H_
#define ASH_ACCELERATORS_MODIFIER_KEY_COMBO_RECORDER_H_

#include "ash/ash_export.h"
#include "ash/events/prerewritten_event_forwarder.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"

namespace ash {

// Emits a metric for what keys a user presses on their keyboard. It categorizes
// in broad categories like "Alphabetic" or "Numpad" including their modifiers
// within the computed metric hash.
class ASH_EXPORT ModifierKeyComboRecorder
    : public PrerewrittenEventForwarder::Observer {
 public:
  enum class ModifierLocation { kLeft, kRight };

  enum class Modifier {
    kMeta,
    kControl,
    kAlt,
    kShift,
    kMaxValue = kShift,
  };

  enum class ModifierFlag : uint8_t {
    kMetaLeft,
    kMetaRight,
    kControlLeft,
    kControlRight,
    kAltLeft,
    kAltRight,
    kShiftLeft,
    kShiftRight,
    kAltGr,
    kFunction,
    kMaxValue = kFunction,
  };

  ModifierKeyComboRecorder();
  ModifierKeyComboRecorder(const ModifierKeyComboRecorder&) = delete;
  ModifierKeyComboRecorder& operator=(const ModifierKeyComboRecorder&) = delete;
  ~ModifierKeyComboRecorder() override;

  void Initialize();

  // ui::PrerewrittenEventForwarder::Observer:
  void OnPrerewriteKeyInputEvent(const ui::KeyEvent& event) override;

 private:
  uint32_t GenerateModifierFlagsFromKeyEvent(const ui::KeyEvent& event);
  uint32_t GetModifierFlagFromModifier(Modifier modifier);

  void UpdateModifierLocations(const ui::KeyEvent& event);

  // Contains which location (left or right) for each modifier that was last
  // pressed.
  std::array<ModifierLocation, static_cast<uint32_t>(Modifier::kMaxValue) + 1>
      modifier_locations_{ModifierLocation::kLeft};
  bool initialized_ = false;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_MODIFIER_KEY_COMBO_RECORDER_H_
