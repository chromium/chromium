// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"

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

TextAcceleratorPart::TextAcceleratorPart(const std::u16string& plain_text) {
  text = plain_text;
  type = mojom::TextAcceleratorPartType::kPlainText;
}

TextAcceleratorPart::TextAcceleratorPart(const TextAcceleratorPart&) = default;
TextAcceleratorPart::~TextAcceleratorPart() = default;
TextAcceleratorPart& TextAcceleratorPart::operator=(
    const TextAcceleratorPart&) = default;

// Constructor used for text-based layout accelerators.
NonConfigurableAcceleratorDetails::NonConfigurableAcceleratorDetails(
    int message_id,
    std::vector<TextAcceleratorPart> replacements) {
  this->message_id = message_id;
  this->replacements = std::move(replacements);
}

NonConfigurableAcceleratorDetails::NonConfigurableAcceleratorDetails(
    int resource_id) {
  message_id = resource_id;
}

// Constructor used for standard accelerators (i.e, it contains at least one
// modifier and a set of keys).
NonConfigurableAcceleratorDetails::NonConfigurableAcceleratorDetails(
    std::vector<ui::Accelerator> accels) {
  accelerators = std::move(accels);
}

NonConfigurableAcceleratorDetails::NonConfigurableAcceleratorDetails(
    const NonConfigurableAcceleratorDetails&) = default;
NonConfigurableAcceleratorDetails& NonConfigurableAcceleratorDetails::operator=(
    const NonConfigurableAcceleratorDetails&) = default;

NonConfigurableAcceleratorDetails::~NonConfigurableAcceleratorDetails() =
    default;

const NonConfigurableActionsMap& GetNonConfigurableActionsMap() {
  static base::NoDestructor<NonConfigurableActionsMap>
      nonConfigurableActionsMap({
          {NonConfigurableActions::kBrowserSelectTabByIndex,
           NonConfigurableAcceleratorDetails(
               IDS_TEXT_ACCELERATOR_GO_TO_TAB_IN_RANGE,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_1),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_8)})},
          {NonConfigurableActions::kBrowserNewTab,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kAmbientDragLinkInSameTab,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_DRAG_LINK_IN_SAME_TAB)},
      });
  return *nonConfigurableActionsMap;
}
}  // namespace ash
