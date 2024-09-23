// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_MENU_CONSTANTS_H_
#define ASH_PUBLIC_CPP_APP_MENU_CONSTANTS_H_

namespace ash {

// Defines command ids used in Shelf and AppList app context menus. These are
// used in histograms, do not remove/renumber entries. If you're adding to this
// enum with the intention that it will be logged, add checks to ensure
// stability of the enum and update the ChromeOSUICommands enum listing in
// tools/metrics/histograms/enums.xml.
enum CommandId {
  // Used by ShelfContextMenu.
  // Superseded by LAUNCH_NEW.
  DEPRECATED_MENU_OPEN_NEW = 0,
  MENU_CLOSE = 1,
  // Superseded by TOGGLE_PIN.
  DEPRECATED_MENU_PIN = 2,
  // Superseded by USE_LAUNCH_TYPE_PINNED.
  DEPRECATED_LAUNCH_TYPE_PINNED_TAB = 3,
  // Superseded by USE_LAUNCH_TYPE_REGULAR.
  DEPRECATED_LAUNCH_TYPE_REGULAR_TAB = 4,
  // Superseded by USE_LAUNCH_TYPE_FULLSCREEN.
  DEPRECATED_LAUNCH_TYPE_FULLSCREEN = 5,
  // Superseded by USE_LAUNCH_TYPE_WINDOW.
  DEPRECATED_LAUNCH_TYPE_WINDOW = 6,
  // Superseded by APP_CONTEXT_MENU_NEW_WINDOW.
  DEPRECATED_MENU_NEW_WINDOW = 7,
  // Superseded by APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW.
  DEPRECATED_MENU_NEW_INCOGNITO_WINDOW = 8,

  // Used by AppMenuModelAdapter.
  NOTIFICATION_CONTAINER = 9,

  // Used by CrostiniShelfContextMenu.
  CROSTINI_USE_LOW_DENSITY = 10,
  CROSTINI_USE_HIGH_DENSITY = 11,

  // Allow reordering of shelf items with the keyboard. Only used for shelf.
  SWAP_WITH_NEXT = 12,
  SWAP_WITH_PREVIOUS = 13,

  // Used by ShelfContextMenu.
  // Superseded by USE_LAUNCH_TYPE_TABBED_WINDOW.
  DEPRECATED_LAUNCH_TYPE_TABBED_WINDOW = 20,

  // Used by AppContextMenu and/or ShelfContextMenu.
  LAUNCH_NEW = 100,
  TOGGLE_PIN = 101,
  SHOW_APP_INFO = 102,
  OPTIONS = 103,
  UNINSTALL = 104,
  REMOVE_FROM_FOLDER = 105,
  APP_CONTEXT_MENU_NEW_WINDOW = 106,
  APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW = 107,
  INSTALL = 108,
  SETTINGS = 109,
  USE_LAUNCH_TYPE_COMMAND_START = 200,
  // No longer supported launch type for hosted apps.
  DEPRECATED_USE_LAUNCH_TYPE_PINNED = USE_LAUNCH_TYPE_COMMAND_START,
  USE_LAUNCH_TYPE_REGULAR = 201,
  // No longer supported launch type for hosted apps.
  DEPRECATED_USE_LAUNCH_TYPE_FULLSCREEN = 202,
  USE_LAUNCH_TYPE_WINDOW = 203,
  USE_LAUNCH_TYPE_TABBED_WINDOW = 204,
  USE_LAUNCH_TYPE_COMMAND_END,
  // The reorder options used by AppsGridContextMenu, ShelfContextMenuModel and
  // AppServiceContextMenu.
  REORDER_SUBMENU = 300,
  REORDER_BY_NAME_ALPHABETICAL = 301,
  REORDER_BY_NAME_REVERSE_ALPHABETICAL = 302,
  REORDER_BY_COLOR = 303,

  // Range of command ids reserved for launching app shortcuts from context
  // menu for Android app. Used by AppContextMenu and ShelfContextMenu.
  LAUNCH_APP_SHORTCUT_FIRST = 1000,
  LAUNCH_APP_SHORTCUT_LAST = 1999,

  // Command for shutting down a VM associated with an App. Used by
  // AppContextMenu and ShelfContextMenu.
  SHUTDOWN_GUEST_OS = 2000,
  SHUTDOWN_BRUSCHETTA_OS = 2001,

  // Range of command IDs reserved for shelf app menu items when there are
  // multiple instances of the same app.
  APP_MENU_ITEM_ID_FIRST = 3000,
  APP_MENU_ITEM_ID_LAST = 3999,

  // Reserved range for extension/app custom menus as defined by
  //   IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST
  //   IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST
  // in chrome/app/chrome_command_ids.h and used in ContextMenuMatcher.
  EXTENSIONS_CONTEXT_CUSTOM_FIRST = 49000,
  EXTENSIONS_CONTEXT_CUSTOM_LAST = 50000,

  COMMAND_ID_COUNT
};

// The size of app context menu items' icons.
constexpr int kAppContextMenuIconSize = 20;

// Minimum padding for children of NotificationMenuView in dips.
constexpr int kNotificationHorizontalPadding = 16;
constexpr int kNotificationVerticalPadding = 8;

// Height of the NotificationItemView in dips.
constexpr int kNotificationItemViewHeight = 48;

// The maximum number of overflow icons which can be shown without the
// showing |overflow_icon_|.
constexpr int kMaxOverflowIcons = 9;

// The identifier used for notifications in the NotificationOverflowView.
constexpr int kNotificationOverflowIconId = 43;

// The identifier used for the overflow icon in NotificationOverflowView.
constexpr int kOverflowIconId = 44;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_MENU_CONSTANTS_H_
