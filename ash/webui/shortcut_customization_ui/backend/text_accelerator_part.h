// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_TEXT_ACCELERATOR_PART_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_TEXT_ACCELERATOR_PART_H_

#include <cstdint>
#include <optional>
#include <string>

#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ui/events/event_constants.h"

namespace ash {

// Used to separate text accelerator parts in the UI e.g ctrl + 1.
enum TextAcceleratorDelimiter {
  kPlusSign,
};

// Represents a replacement for part of a non-configurable accelerator.
// Contains the text to display as well as its type (Modifier, Key, Plain Text)
// which is needed to determine how to display the text in the shortcut
// customization app.
class TextAcceleratorPart : public mojom::TextAcceleratorPart {
 public:
  explicit TextAcceleratorPart(ui::EventFlags modifier);
  explicit TextAcceleratorPart(ui::KeyboardCode key_code);
  explicit TextAcceleratorPart(const std::u16string& plain_text);
  explicit TextAcceleratorPart(TextAcceleratorDelimiter delimiter);
  TextAcceleratorPart(const TextAcceleratorPart&);
  TextAcceleratorPart& operator=(const TextAcceleratorPart&);
  ~TextAcceleratorPart();

  // If the part is a keycode, we store it so that we will always have a way
  // to get the accurate localized key string to display.
  std::optional<ui::KeyboardCode> keycode;
};

}  // namespace ash

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_TEXT_ACCELERATOR_PART_H_
