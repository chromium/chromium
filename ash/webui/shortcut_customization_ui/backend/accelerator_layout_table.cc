// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"
#include <string>

#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check_op.h"
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
  text = KeycodeToKeyString(key_code);
  type = mojom::TextAcceleratorPartType::kKey;
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
                TextAcceleratorPart(TextAcceleratorDelimiter::kPlusSign),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_1),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_8)})},
          {NonConfigurableActions::kBrowserNewTab,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserCloseTab,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserCloseWindow,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserSelectLastTab,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_9, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserOpenFile,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_O, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserNewIncognitoWindow,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserNewWindow,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserRestoreTab,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserTabSearch,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserClearBrowsingData,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_BACK, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserCloseFindOrStop,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)})},
          {NonConfigurableActions::kBrowserFocusBookmarks,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)})},
          {NonConfigurableActions::kBrowserBack,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_LEFT, ui::EF_ALT_DOWN)})},
          {NonConfigurableActions::kBrowserForward,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_RIGHT, ui::EF_ALT_DOWN)})},
          {NonConfigurableActions::kBrowserFind,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_F, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserShowDownloads,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_J, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserShowHistory,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_H, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserFocusMenuBar,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_F10, ui::EF_NONE)})},
          {NonConfigurableActions::kBrowserPrint,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_P, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserReloadBypassingCache,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_R, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserZoomNormal,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_0, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserBookmarkAllTabs,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_D, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserSavePage,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_S, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserBookmarkThisTab,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_D, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserShowBookmarkManager,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_O, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserDevToolsConsole,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_J, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserDevToolsInspect,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_C, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserDevTools,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserShowBookmarkBar,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserViewSource,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_U, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserZoomMinus,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserZoomPlus,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserFocusToolbar,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserFocusInactivePopupForAccessibility,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)})},
      });
  return *nonConfigurableActionsMap;
}
}  // namespace ash
