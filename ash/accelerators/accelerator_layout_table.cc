// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_layout_table.h"

#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/notreached.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

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
  return std::u16string();
}
}  // namespace

TextAcceleratorPart::TextAcceleratorPart(ui::EventFlags modifier) {
  text = GetTextForModifier(modifier);
  type = mojom::TextAcceleratorPartType::kModifier;
}

TextAcceleratorPart::TextAcceleratorPart(ui::KeyboardCode key_code) {
  text = KeycodeToKeyString(key_code);
  type = mojom::TextAcceleratorPartType::kKey;
}

TextAcceleratorPart::TextAcceleratorPart(const TextAcceleratorPart&) = default;
TextAcceleratorPart::~TextAcceleratorPart() = default;
TextAcceleratorPart& TextAcceleratorPart::operator=(
    const TextAcceleratorPart&) = default;

AcceleratorTextDetails::AcceleratorTextDetails(
    int message_id,
    std::vector<TextAcceleratorPart> parts) {
  this->message_id = message_id;
  this->text_accelerator_parts = std::move(parts);
}

AcceleratorTextDetails::AcceleratorTextDetails(const AcceleratorTextDetails&) =
    default;

AcceleratorTextDetails::~AcceleratorTextDetails() = default;
const NonConfigurableActionsTextDetailsMap& GetTextDetailsMap() {
  static base::NoDestructor<NonConfigurableActionsTextDetailsMap>
      textDetailsMap({
          {NonConfigurableActions::kBrowserSelectTabByIndex,
           AcceleratorTextDetails(
               IDS_TEXT_ACCELERATOR_GO_TO_TAB_IN_RANGE,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_1),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_8)})},
      });
  return *textDetailsMap;
}
}  // namespace ash
