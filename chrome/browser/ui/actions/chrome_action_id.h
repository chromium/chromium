// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_ID_H_
#define CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_ID_H_

#include "chrome/app/chrome_command_ids.h"
#include "ui/actions/action_id.h"

// The references to the IDC_XXXX command ids is intended purely for
// documentation purposes in order to maintain the correlation between the new
// action id and the legacy command id. NOTE: The ordinal values will *not* be
// the same. Eventually, these references may be removed once the transition to
// pure ActionItems is complete.

// clang-format off
#define CHROME_COMMON_ACTION_IDS \
  /* Navigation commands */ \
  E(kActionBack, IDC_BACK, kChromeActionsStart) \
  E(kActionForward, IDC_FORWARD) \
  E(kActionReload, IDC_RELOAD) \
  E(kActionHome, IDC_HOME) \
  E(kActionOpenCurrentUrl, IDC_OPEN_CURRENT_URL) \
  E(kActionStop, IDC_STOP) \
  E(kActionReloadBypassingCache, IDC_RELOAD_BYPASSING_CACHE) \
  E(kActionReloadClearingCache, IDC_RELOAD_CLEARING_CACHE) \
  /* Window management commands */ \
  E(kActionNewWindow, IDC_NEW_WINDOW) \
  E(kActionNewIncognitoWindow, IDC_NEW_INCOGNITO_WINDOW) \
  E(kActionCloseWindow, IDC_CLOSE_WINDOW) \
  E(kActionAlwaysOnTop, IDC_ALWAYS_ON_TOP) \
  E(kActionNewTab, IDC_NEW_TAB) \
  E(kActionCloseTab, IDC_CLOSE_TAB) \
  E(kSelectNextTab, IDC_SELECT_NEXT_TAB) \
  E(kSelectPreviousTab, IDC_SELECT_PREVIOUS_TAB) \
  E(kSelectTab0, IDC_SELECT_TAB_0) \
  E(kSelectTab1, IDC_SELECT_TAB_1) \
  E(kSelectTab2, IDC_SELECT_TAB_2) \
  E(kSelectTab3, IDC_SELECT_TAB_3) \
  E(kSelectTab4, IDC_SELECT_TAB_4) \
  E(kSelectTab5, IDC_SELECT_TAB_5) \
  E(kSelectTab6, IDC_SELECT_TAB_6) \
  E(kSelectTab7, IDC_SELECT_TAB_7) \
  E(kSelectTabLastTab, IDC_SELECT_LAST_TAB) \
  E(kDuplicateTab, IDC_DUPLICATE_TAB) \
  E(kRestoreTab, IDC_RESTORE_TAB) \
  E(kShowAsTab, IDC_SHOW_AS_TAB) \
  E(kFullscreen, IDC_FULLSCREEN) \
  E(kExit, IDC_EXIT) \
  E(kMoveTabNext, IDC_MOVE_TAB_NEXT) \
  E(kMoveTabPrevious, IDC_MOVE_TAB_PREVIOUS) \
  E(kSearch, IDC_SEARCH) \
  E(kMoveWindowMenu, IDC_WINDOW_MENU) \
  E(kMinimizeWindow, IDC_MINIMIZE_WINDOW) \
  E(kMaximizeWindow, IDC_MAXIMIZE_WINDOW) \
  E(kAllWindowsFront, IDC_ALL_WINDOWS_FRONT) \
  E(kNameWindow, IDC_NAME_WINDOW)

#if BUILDFLAG(IS_CHROMEOS)
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS \
  E(kToggleMultitaskMenu, IDC_TOGGLE_MULTITASK_MENU)
#elif BUILDFLAG(IS_LINUX)
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS \
  E(kUseSystemTitleBar, IDC_USE_SYSTEM_TITLE_BAR) \
  E(kRestoreWindow, IDC_RESTORE_WINDOW)
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS \
  E(kRestoreWindow, IDC_RESTORE_WINDOW)
#else
#define CHROME_PLATFORM_SPECIFIC_ACTION_IDS
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#define CHROME_ACTION_IDS \
    CHROME_COMMON_ACTION_IDS \
    CHROME_PLATFORM_SPECIFIC_ACTION_IDS

#include "ui/actions/action_id_macros.inc"

enum ChromeActionIds : actions::ActionId {
  kChromeActionsStart = actions::kActionsEnd,

  CHROME_ACTION_IDS

  kChromeActionsEnd,
};

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/actions/action_id_macros.inc"

// clang-format on

#endif  // CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_ID_H_
