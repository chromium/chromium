// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"
#include <string>

#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
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
          // Ambient Accelerator with replacement.
          {NonConfigurableActions::kBrowserSelectTabByIndex,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_GO_TO_TAB_IN_RANGE,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(TextAcceleratorDelimiter::kPlusSign),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_1),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_8)})},
          {NonConfigurableActions::kBrowserFindNext,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_FIND_NEXT,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_G),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_RETURN)})},
          {NonConfigurableActions::kBrowserFindPrevious,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_FIND_PREVIOUS,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::EF_SHIFT_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_G),
                TextAcceleratorPart(ui::EF_SHIFT_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_RETURN)})},
          {NonConfigurableActions::kAmbientOpenLinkInTab,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_OPEN_LINK_IN_TAB,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::EF_SHIFT_DOWN)})},
          {NonConfigurableActions::kAmbientOpenLinkInTabBackground,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_OPEN_LINK_IN_TAB_BACKGROUND,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kAmbientOpenLinkInWindow,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_OPEN_LINK_IN_WINDOW,
               {TextAcceleratorPart(ui::EF_SHIFT_DOWN)})},
          {NonConfigurableActions::kAmbientOpenPageInNewTab,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_OPEN_PAGE_IN_NEW_TAB,
               {TextAcceleratorPart(ui::EF_ALT_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_RETURN)})},
          {NonConfigurableActions::kAmbientCycleBackwardMRU,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_CYCLE_BACKWARD_MRU,
               {TextAcceleratorPart(ui::EF_ALT_DOWN),
                TextAcceleratorPart(ui::EF_SHIFT_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_TAB)})},
          {NonConfigurableActions::kBrowserFocusSearch,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_FOCUS_SEARCH,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_K),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_E)})},
          {NonConfigurableActions::kBrowserReload,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_RELOAD,
               {TextAcceleratorPart(ui::KeyboardCode::VKEY_BROWSER_REFRESH),
                TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_R)})},
          {NonConfigurableActions::kBrowserRightClick,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_RIGHT_CLICK,
               {TextAcceleratorPart(ui::EF_ALT_DOWN)})},
          {NonConfigurableActions::kBrowserShowAppMenu,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_SHOW_APP_MENU,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_E),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_F)})},
          {NonConfigurableActions::kAmbientLaunchNumberedApp,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_LAUNCH_NUMBERED_APP,
               {TextAcceleratorPart(ui::EF_ALT_DOWN),
                TextAcceleratorPart(TextAcceleratorDelimiter::kPlusSign),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_1),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_8)})},
          {NonConfigurableActions::kAmbientOpenFile,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_OPEN_FILE,
               {TextAcceleratorPart(ui::KeyboardCode::VKEY_SPACE)})},
          {NonConfigurableActions::kAmbientHighlightNextItemOnShelf,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_HIGHLIGHT_NEXT_ITEM_ON_SHELF,
               {TextAcceleratorPart(ui::EF_SHIFT_DOWN),
                TextAcceleratorPart(ui::EF_ALT_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_I),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_TAB),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_RIGHT)})},
          {NonConfigurableActions::kAmbientHighlightPreviousItemOnShelf,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_HIGHTLIGHT_PREVIOUS_ITEM_ON_SHELF,
               {TextAcceleratorPart(ui::EF_SHIFT_DOWN),
                TextAcceleratorPart(ui::EF_ALT_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_I),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_TAB),
                TextAcceleratorPart(ui::EF_SHIFT_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_LEFT)})},
          {NonConfigurableActions::kAmbientOpenHighlightedItemOnShelf,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_OPEN_HIGHLIGHTED_ITEM_ON_SHELF,
               {TextAcceleratorPart(ui::EF_SHIFT_DOWN),
                TextAcceleratorPart(ui::EF_ALT_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_I),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_SPACE),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_RETURN)})},
          {NonConfigurableActions::kAmbientRemoveHighlightOnShelf,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_REMOVE_HIGHLIGHT_ON_SHELF,
               {TextAcceleratorPart(ui::EF_SHIFT_DOWN),
                TextAcceleratorPart(ui::EF_ALT_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_I),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_ESCAPE)})},
          {NonConfigurableActions::kAmbientSwitchFocus,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_SWITCH_FOCUS,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_BROWSER_BACK),
                TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::EF_SHIFT_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_BROWSER_BACK)})},
          {NonConfigurableActions::kAmbientMoveAppsInGrid,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_MOVE_APPS_IN_GRID,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_LEFT),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_RIGHT),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_UP),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_DOWN)})},
          {NonConfigurableActions::kAmbientMoveAppsInOutFolder,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_MOVE_APPS_IN_OUT_FOLDER,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN),
                TextAcceleratorPart(ui::EF_SHIFT_DOWN),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_LEFT),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_RIGHT),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_UP),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_DOWN)})},
          {NonConfigurableActions::kAmbientOpenLinkInTabBackground,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_OPEN_LINK_IN_TAB_BACKGROUND,
               {TextAcceleratorPart(ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserStopDragTab,
           NonConfigurableAcceleratorDetails(
               IDS_BROWSER_ACCELERATOR_STOP_DRAG_TAB,
               {TextAcceleratorPart(ui::KeyboardCode::VKEY_ESCAPE)})},
          {NonConfigurableActions::kAmbientActivateIndexedDesk,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_ACTIVATE_INDEXED_DESK,
               {TextAcceleratorPart(ui::EF_SHIFT_DOWN),
                TextAcceleratorPart(ui::EF_COMMAND_DOWN),
                TextAcceleratorPart(TextAcceleratorDelimiter::kPlusSign),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_1),
                TextAcceleratorPart(ui::KeyboardCode::VKEY_8)})},
          // Ambient accelerators that only contain plain text (no
          // replacements).
          {NonConfigurableActions::kAmbientDragLinkInNewTab,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_DRAG_LINK_IN_NEW_TAB)},
          {NonConfigurableActions::kAmbientDragLinkInSameTab,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_DRAG_LINK_IN_SAME_TAB)},
          {NonConfigurableActions::kAmbientSaveLinkAsBookmark,
           NonConfigurableAcceleratorDetails(
               IDS_AMBIENT_ACCELERATOR_SAVE_LINK_AS_BOOKMARK)},
          // Standard accelerators.
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
               ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)})},
          {NonConfigurableActions::kBrowserFocusInactivePopupForAccessibility,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)})},
          // TODO(jimmyxgong): Re-enable these shortcuts. These conflict with
          // the 6-pack keys Home and End.
          //   {NonConfigurableActions::kBrowserBottomPage,
          //    NonConfigurableAcceleratorDetails(
          //        {ui::Accelerator(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN)})},
          //   {NonConfigurableActions::kBrowserTopPage,
          //    NonConfigurableAcceleratorDetails(
          //        {ui::Accelerator(ui::VKEY_LEFT, ui::EF_COMMAND_DOWN)})},
          //   {NonConfigurableActions::kAmbientDeleteNextWord,
          //    NonConfigurableAcceleratorDetails(
          //        {ui::Accelerator(ui::VKEY_BACK, ui::EF_COMMAND_DOWN)})},
          //   {NonConfigurableActions::kBrowserPageUp,
          //    NonConfigurableAcceleratorDetails(
          //        {ui::Accelerator(ui::VKEY_UP, ui::EF_COMMAND_DOWN)})},
          //   {NonConfigurableActions::kBrowserPageDown,
          //    NonConfigurableAcceleratorDetails(
          //        {ui::Accelerator(ui::VKEY_DOWN, ui::EF_COMMAND_DOWN)})},
          //   {NonConfigurableActions::kAmbientGoToBeginningOfLine,
          //    NonConfigurableAcceleratorDetails(
          //        {ui::Accelerator(ui::VKEY_LEFT, ui::EF_COMMAND_DOWN)})},
          //   {NonConfigurableActions::kAmbientGoToBeginningOfLine,
          //    NonConfigurableAcceleratorDetails(
          //        {ui::Accelerator(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN)})},
          {NonConfigurableActions::kBrowserNextPane,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_BROWSER_BACK,
                                ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)})},
          {NonConfigurableActions::kAmbientOpenRightClickMenu,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_VOLUME_UP, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)})},
          {NonConfigurableActions::kAmbientDisplayHiddenFiles,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_OEM_PERIOD, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kAmbientCaretBrowsing,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_7, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN)})},
          {NonConfigurableActions::kBrowserAutoComplete,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_RETURN, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserHome,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_HOME, ui::EF_ALT_DOWN)})},
          {NonConfigurableActions::kBrowserSelectNextTab,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_TAB, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kBrowserSelectPreviousTab,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_TAB, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)})},
          {NonConfigurableActions::kAmbientCopy,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kAmbientCut,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_X, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kAmbientPaste,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_V, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kAmbientPastePlainText,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_V, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)})},
          {NonConfigurableActions::kAmbientDeletePreviousWord,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_BACK, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kAmbientUndo,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_Z, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kAmbientRedo,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_Z, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)})},
          {NonConfigurableActions::kAmbientContentContextSelectAll,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kAmbientSelectTextToBeginning,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_LEFT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)})},
          {NonConfigurableActions::kAmbientSelectTextToEndOfLine,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)})},
          {NonConfigurableActions::kAmbientSelectPreviousWord,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_LEFT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)})},
          {NonConfigurableActions::kAMbientSelectNextWord,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)})},
          {NonConfigurableActions::kAmbientGoToBeginningOfDocument,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_LEFT, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN)})},
          {NonConfigurableActions::kAmbientGoToEndOfDocument,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN)})},
          {NonConfigurableActions::kAmbientMoveStartOfPreviousWord,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kAmbientMoveToEndOfWord,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN)})},
          {NonConfigurableActions::kSixPackDelete,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_BACK, ui::EF_COMMAND_DOWN)})},
          {NonConfigurableActions::kSixPackHome,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_LEFT, ui::EF_COMMAND_DOWN)})},
          {NonConfigurableActions::kSixPackPageUp,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_UP, ui::EF_COMMAND_DOWN)})},
          {NonConfigurableActions::kSixPackEnd,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN)})},
          {NonConfigurableActions::kSixPackPageDown,
           NonConfigurableAcceleratorDetails(
               {ui::Accelerator(ui::VKEY_DOWN, ui::EF_COMMAND_DOWN)})},
          {NonConfigurableActions::kSixPackInsert,
           NonConfigurableAcceleratorDetails({ui::Accelerator(
               ui::VKEY_BACK, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN)})},
      });
  return *nonConfigurableActionsMap;
}
}  // namespace ash
