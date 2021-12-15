// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access/switch_access_menu_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/switch_access/switch_access_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_node_data.mojom-shared.h"
#include "ui/events/event.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

namespace {
constexpr int kMaxColumns = 3;

struct ButtonInfo {
  const gfx::VectorIcon* icon;
  int label_id;
};

// These strings must match the values of
// accessibility_private::SwitchAccessMenuAction.
const base::flat_map<std::string, ButtonInfo>& GetMenuButtonDetails() {
  static base::NoDestructor<base::flat_map<std::string, ButtonInfo>>
      menu_button_details({});

  if (menu_button_details->empty()) {
    base::flat_map<std::string, ButtonInfo> base_menu_button_details(
        {{"copy", {&kSwitchAccessCopyIcon, IDS_ASH_SWITCH_ACCESS_COPY}},
         {"cut", {&kSwitchAccessCutIcon, IDS_ASH_SWITCH_ACCESS_CUT}},
         {"decrement",
          {&kSwitchAccessDecrementIcon, IDS_ASH_SWITCH_ACCESS_DECREMENT}},
         {"dictation",
          {&kDictationOnNewuiIcon, IDS_ASH_SWITCH_ACCESS_DICTATION}},
         {"endTextSelection",
          {&kSwitchAccessEndTextSelectionIcon,
           IDS_ASH_SWITCH_ACCESS_END_TEXT_SELECTION}},
         {"increment",
          {&kSwitchAccessIncrementIcon, IDS_ASH_SWITCH_ACCESS_INCREMENT}},
         {"itemScan",
          {&kSwitchAccessItemScanIcon, IDS_ASH_SWITCH_ACCESS_ITEM_SCAN}},
         {"jumpToBeginningOfText",
          {&kSwitchAccessJumpToBeginningOfTextIcon,
           IDS_ASH_SWITCH_ACCESS_JUMP_TO_BEGINNING_OF_TEXT}},
         {"jumpToEndOfText",
          {&kSwitchAccessJumpToEndOfTextIcon,
           IDS_ASH_SWITCH_ACCESS_JUMP_TO_END_OF_TEXT}},
         {"keyboard",
          {&kSwitchAccessKeyboardIcon, IDS_ASH_SWITCH_ACCESS_KEYBOARD}},
         {"moveBackwardOneCharOfText",
          {&kSwitchAccessMoveBackwardOneCharOfTextIcon,
           IDS_ASH_SWITCH_ACCESS_MOVE_BACKWARD_ONE_CHAR_OF_TEXT}},
         {"moveBackwardOneWordOfText",
          {&kSwitchAccessMoveBackwardOneWordOfTextIcon,
           IDS_ASH_SWITCH_ACCESS_MOVE_BACKWARD_ONE_WORD_OF_TEXT}},
         {"moveCursor",
          {&kSwitchAccessMoveCursorIcon, IDS_ASH_SWITCH_ACCESS_MOVE_CURSOR}},
         {"moveDownOneLineOfText",
          {&kSwitchAccessMoveDownOneLineOfTextIcon,
           IDS_ASH_SWITCH_ACCESS_MOVE_DOWN_ONE_LINE_OF_TEXT}},
         {"moveForwardOneCharOfText",
          {&kSwitchAccessMoveForwardOneCharOfTextIcon,
           IDS_ASH_SWITCH_ACCESS_MOVE_FORWARD_ONE_CHAR_OF_TEXT}},
         {"moveForwardOneWordOfText",
          {&kSwitchAccessMoveForwardOneWordOfTextIcon,
           IDS_ASH_SWITCH_ACCESS_MOVE_FORWARD_ONE_WORD_OF_TEXT}},
         {"moveUpOneLineOfText",
          {&kSwitchAccessMoveUpOneLineOfTextIcon,
           IDS_ASH_SWITCH_ACCESS_MOVE_UP_ONE_LINE_OF_TEXT}},
         {"paste", {&kSwitchAccessPasteIcon, IDS_ASH_SWITCH_ACCESS_PASTE}},
         {"scrollDown",
          {&kSwitchAccessScrollDownIcon, IDS_ASH_SWITCH_ACCESS_SCROLL_DOWN}},
         {"scrollLeft",
          {&kSwitchAccessScrollLeftIcon, IDS_ASH_SWITCH_ACCESS_SCROLL_LEFT}},
         {"scrollRight",
          {&kSwitchAccessScrollRightIcon, IDS_ASH_SWITCH_ACCESS_SCROLL_RIGHT}},
         {"scrollUp",
          {&kSwitchAccessScrollUpIcon, IDS_ASH_SWITCH_ACCESS_SCROLL_UP}},
         {"select", {&kSwitchAccessSelectIcon, IDS_ASH_SWITCH_ACCESS_SELECT}},
         {"settings",
          {&kSwitchAccessSettingsIcon, IDS_ASH_SWITCH_ACCESS_SETTINGS}},
         {"startTextSelection",
          {&kSwitchAccessStartTextSelectionIcon,
           IDS_ASH_SWITCH_ACCESS_START_TEXT_SELECTION}},
         {"pointScan",
          {&kSwitchAccessPointScanIcon, IDS_ASH_SWITCH_ACCESS_POINT_SCAN}},
         {"leftClick",
          {&kSwitchAccessLeftClickIcon, IDS_ASH_SWITCH_ACCESS_LEFT_CLICK}},
         {"rightClick",
          {&kSwitchAccessRightClickIcon, IDS_ASH_SWITCH_ACCESS_RIGHT_CLICK}}});

    if (switches::IsSwitchAccessMultistepAutomationEnabled()) {
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "shortcuts",
          {&kSwitchAccessQuickCommandsIcon, IDS_ASH_SWITCH_ACCESS_SHORTCUTS}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "leaveGroup",
          {&kSwitchAccessLeaveGroupIcon, IDS_ASH_SWITCH_ACCESS_LEAVE_GROUP}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "webMenu",
          {&kSwitchAccessWebMenuIcon, IDS_ASH_SWITCH_ACCESS_WEB_MENU}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "webBookmark",
          {&kSwitchAccessWebBookmarkIcon, IDS_ASH_SWITCH_ACCESS_WEB_BOOKMARK}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "webBottomOfPage", {&kSwitchAccessWebBottomOfPageIcon,
                              IDS_ASH_SWITCH_ACCESS_WEB_BOTTOM_OF_PAGE}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "webTopOfPage", {&kSwitchAccessWebTopOfPageIcon,
                           IDS_ASH_SWITCH_ACCESS_WEB_TOP_OF_PAGE}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "webFindInPage", {&kSwitchAccessWebFindInPageIcon,
                            IDS_ASH_SWITCH_ACCESS_WEB_FIND_IN_PAGE}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "webDownloads", {&kSwitchAccessWebDownloadsIcon,
                           IDS_ASH_SWITCH_ACCESS_WEB_DOWNLOADS}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "webClearHistory", {&kSwitchAccessWebClearHistoryIcon,
                              IDS_ASH_SWITCH_ACCESS_WEB_CLEAR_HISTORY}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "systemMenu",
          {&kSwitchAccessSystemMenuIcon, IDS_ASH_SWITCH_ACCESS_SYSTEM_MENU}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "systemStatusBar", {&kSwitchAccessSystemStatusBarIcon,
                              IDS_ASH_SWITCH_ACCESS_SYSTEM_STATUS_BAR}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "systemLauncher", {&kSwitchAccessSystemLauncherIcon,
                             IDS_ASH_SWITCH_ACCESS_SYSTEM_LAUNCHER}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "systemTaskManager", {&kSwitchAccessSystemTaskManagerIcon,
                                IDS_ASH_SWITCH_ACCESS_SYSTEM_TASK_MANAGER}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "systemDiagnostics", {&kSwitchAccessSystemDiagnosticsIcon,
                                IDS_ASH_SWITCH_ACCESS_SYSTEM_DIAGNOSTICS}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "systemScreenshot", {&kSwitchAccessSystemScreenshotIcon,
                               IDS_ASH_SWITCH_ACCESS_SYSTEM_SCREENSHOT}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "systemHelp",
          {&kSwitchAccessSystemHelpIcon, IDS_ASH_SWITCH_ACCESS_SYSTEM_HELP}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "mediaMenu",
          {&kSwitchAccessMediaMenuIcon, IDS_ASH_SWITCH_ACCESS_MEDIA_MENU}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "mediaMute",
          {&kSwitchAccessMediaMuteIcon, IDS_ASH_SWITCH_ACCESS_MEDIA_MUTE}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "mediaVolumeDown", {&kSwitchAccessMediaVolumeDownIcon,
                              IDS_ASH_SWITCH_ACCESS_MEDIA_VOLUME_DOWN}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "mediaVolumeUp", {&kSwitchAccessMediaVolumeUpIcon,
                            IDS_ASH_SWITCH_ACCESS_MEDIA_VOLUME_UP}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "mediaRewind",
          {&kSwitchAccessMediaRewindIcon, IDS_ASH_SWITCH_ACCESS_MEDIA_REWIND}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "mediaPlayPause", {&kSwitchAccessMediaPlayPauseIcon,
                             IDS_ASH_SWITCH_ACCESS_MEDIA_PLAY_PAUSE}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "mediaFastforward", {&kSwitchAccessMediaFastforwardIcon,
                               IDS_ASH_SWITCH_ACCESS_MEDIA_FASTFORWARD}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "displayMenu",
          {&kSwitchAccessDisplayMenuIcon, IDS_ASH_SWITCH_ACCESS_DISPLAY_MENU}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "displayMirror", {&kSwitchAccessDisplayMirrorIcon,
                            IDS_ASH_SWITCH_ACCESS_DISPLAY_MIRROR}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "displayBrightnessDown",
          {&kSwitchAccessDisplayBrightnessDownIcon,
           IDS_ASH_SWITCH_ACCESS_DISPLAY_BRIGHTNESS_DOWN}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "displayBrightnessUp",
          {&kSwitchAccessDisplayBrightnessUpIcon,
           IDS_ASH_SWITCH_ACCESS_DISPLAY_BRIGHTNESS_UP}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "displayRotate", {&kSwitchAccessDisplayRotateIcon,
                            IDS_ASH_SWITCH_ACCESS_DISPLAY_ROTATE}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "displayZoomOut", {&kSwitchAccessDisplayZoomOutIcon,
                             IDS_ASH_SWITCH_ACCESS_DISPLAY_ZOOM_OUT}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "displayZoomIn", {&kSwitchAccessDisplayZoomInIcon,
                            IDS_ASH_SWITCH_ACCESS_DISPLAY_ZOOM_IN}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "userMenu",
          {&kSwitchAccessUserMenuIcon, IDS_ASH_SWITCH_ACCESS_USER_MENU}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "userLock",
          {&kSwitchAccessUserLockIcon, IDS_ASH_SWITCH_ACCESS_USER_LOCK}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "userPreviousUser", {&kSwitchAccessUserPreviousUserIcon,
                               IDS_ASH_SWITCH_ACCESS_USER_PREVIOUS_USER}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "userNextUser", {&kSwitchAccessUserNextUserIcon,
                           IDS_ASH_SWITCH_ACCESS_USER_NEXT_USER}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "userSignOut", {&kSwitchAccessUserSignOutIcon,
                          IDS_ASH_SWITCH_ACCESS_USER_SIGN_OUT}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "actionRecorder", {&kSwitchAccessActionRecorderIcon,
                             IDS_ASH_SWITCH_ACCESS_ACTION_RECORDER}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "startRecording", {&kSwitchAccessStartRecordingIcon,
                             IDS_ASH_SWITCH_ACCESS_START_RECORDING}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "stopRecording", {&kSwitchAccessStopRecordingIcon,
                            IDS_ASH_SWITCH_ACCESS_STOP_RECORDING}));
      base_menu_button_details.insert(std::pair<std::string, ButtonInfo>(
          "executeMacro", {&kSwitchAccessExecuteMacroIcon,
                           IDS_ASH_SWITCH_ACCESS_EXECUTE_MACRO}));
    }

    menu_button_details->swap(base_menu_button_details);
  }
  return *menu_button_details;
}

}  // namespace

SwitchAccessMenuView::SwitchAccessMenuView() = default;
SwitchAccessMenuView::~SwitchAccessMenuView() = default;

void SwitchAccessMenuView::SetActions(std::vector<std::string> actions) {
  RemoveAllChildViews();

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddPaddingColumn(0 /* resize_percent */, kBubbleMenuPadding);
  for (int i = 0; i < kMaxColumns; i++) {
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                       0, /* resize_percent */
                       views::GridLayout::ColumnSize::kFixed,
                       SwitchAccessMenuButton::kWidthDip, 0);
    columns->AddPaddingColumn(0 /* resize_percent */, kBubbleMenuPadding);
  }

  int button_count = 0;
  for (std::string action : actions) {
    auto it = GetMenuButtonDetails().find(action);
    if (it == GetMenuButtonDetails().end())
      continue;
    ButtonInfo info = it->second;
    // If this is the first button of a new row, tell the layout to start a
    // new row.
    if (button_count % kMaxColumns == 0)
      layout->StartRowWithPadding(0, 0, 0, kBubbleMenuPadding);
    layout->AddView(std::make_unique<SwitchAccessMenuButton>(action, *info.icon,
                                                             info.label_id));
    button_count++;
  }
  layout->AddPaddingRow(0, kBubbleMenuPadding);
  InvalidateLayout();
}

int SwitchAccessMenuView::GetBubbleWidthDip() const {
  // In the future this will vary with the number of menu items displayed.
  return (kMaxColumns * SwitchAccessMenuButton::kWidthDip) +
         ((kMaxColumns - 1) * kBubbleMenuPadding) +
         kUnifiedMenuItemPadding.left() + kUnifiedMenuItemPadding.right();
}

void SwitchAccessMenuView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kMenu;
}

const char* SwitchAccessMenuView::GetClassName() const {
  return "SwitchAccessMenuView";
}

}  // namespace ash
