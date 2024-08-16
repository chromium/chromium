// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/text_accelerator_part.h"

#include <optional>
#include <string>

namespace ash {

namespace {
std::u16string GetTextForModifier(ui::EventFlags modifier) {
  switch (modifier) {
    case ui::EF_SHIFT_DOWN:
      return u"shift";
    case ui::EF_CONTROL_DOWN:
      return u"ctrl";
    case ui::EF_ALT_DOWN:
      return u"alt";
    case ui::EF_COMMAND_DOWN:
      return u"meta";
  }
  NOTREACHED();
}

std::u16string GetTextForDelimiter(TextAcceleratorDelimiter delimiter) {
  // Note: Use a switch statement to perform string lookup if/when more
  // delimiters are added to the TextAcceleratorDelimiter enum.
  CHECK_EQ(delimiter, TextAcceleratorDelimiter::kPlusSign);
  return u"+";
}
}  // namespace

TextAcceleratorPart::TextAcceleratorPart(ui::EventFlags modifier) {
  text = GetTextForModifier(modifier);
  type = mojom::TextAcceleratorPartType::kModifier;
}

TextAcceleratorPart::TextAcceleratorPart(ui::KeyboardCode key_code) {
  type = mojom::TextAcceleratorPartType::kKey;
  keycode = key_code;
}

TextAcceleratorPart::TextAcceleratorPart(const std::u16string& plain_text) {
  text = plain_text;
  type = mojom::TextAcceleratorPartType::kPlainText;
}

TextAcceleratorPart::TextAcceleratorPart(TextAcceleratorDelimiter delimiter) {
  text = GetTextForDelimiter(delimiter);
  type = mojom::TextAcceleratorPartType::kDelimiter;
}

TextAcceleratorPart::TextAcceleratorPart(const TextAcceleratorPart&) = default;
TextAcceleratorPart::~TextAcceleratorPart() = default;
TextAcceleratorPart& TextAcceleratorPart::operator=(
    const TextAcceleratorPart&) = default;

}  // namespace ash
