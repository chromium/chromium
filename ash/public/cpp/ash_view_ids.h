// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_VIEW_IDS_H_
#define ASH_PUBLIC_CPP_ASH_VIEW_IDS_H_

namespace ash {

enum ViewID {
  VIEW_ID_NONE = 0,

  // Ash IDs start above the range used in Chrome (c/b/ui/view_ids.h).
  VIEW_ID_ASH_START = 10000,

  // Row for the virtual keyboard feature in accessibility detailed view.
  VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD,
  // Icon that indicates the virtual keyboard is enabled.
  VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD_ENABLED,

  // Feature tile ids.
  VIEW_ID_ACCESSIBILITY_FEATURE_TILE,
  VIEW_ID_SCREEN_CAPTURE_FEATURE_TILE,
  VIEW_ID_DND_FEATURE_TILE,
  VIEW_ID_AUTOROTATE_FEATURE_TILE,

  // Accessibility feature pod button in main view.
  VIEW_ID_ACCESSIBILITY_TRAY_ITEM,
  // System tray AddUserButton in UserChooserView.
  VIEW_ID_ADD_USER_BUTTON,
  VIEW_ID_BLUETOOTH_DEFAULT_VIEW,
  // System tray casting row elements.
  VIEW_ID_CAST_CAST_VIEW,
  VIEW_ID_CAST_CAST_VIEW_LABEL,
  VIEW_ID_CAST_MAIN_VIEW,
  VIEW_ID_CAST_SELECT_VIEW,
  // The entry to add wifi network in the quick settings network subpage.
  VIEW_ID_JOIN_NETWORK_ENTRY,

  VIEW_ID_MEDIA_TRAY_VIEW,

  // System tray quick settings view buttons shown in the root QS view:
  VIEW_ID_QS_MIN,
  VIEW_ID_QS_BATTERY_BUTTON = VIEW_ID_QS_MIN,
  VIEW_ID_QS_COLLAPSE_BUTTON,
  VIEW_ID_QS_DATE_VIEW_BUTTON,
  VIEW_ID_QS_FEEDBACK_BUTTON,
  VIEW_ID_QS_LOCK_BUTTON,
  VIEW_ID_QS_MANAGED_BUTTON,
  VIEW_ID_QS_POWER_BUTTON,
  VIEW_ID_QS_POWER_EMAIL_MENU_BUTTON,
  VIEW_ID_QS_POWER_LOCK_MENU_BUTTON,
  VIEW_ID_QS_POWER_OFF_MENU_BUTTON,
  VIEW_ID_QS_POWER_RESTART_MENU_BUTTON,
  VIEW_ID_QS_POWER_SIGNOUT_MENU_BUTTON,
  VIEW_ID_QS_SETTINGS_BUTTON,
  VIEW_ID_QS_SIGN_OUT_BUTTON,
  VIEW_ID_QS_SUPERVISED_BUTTON,
  VIEW_ID_QS_USER_AVATAR_BUTTON,
  VIEW_ID_QS_VERSION_BUTTON,
  VIEW_ID_QS_EOL_NOTICE_BUTTON,
  VIEW_ID_QS_MAX = VIEW_ID_QS_EOL_NOTICE_BUTTON,

  // Shown in system tray detailed views:
  VIEW_ID_QS_DETAILED_VIEW_BACK_BUTTON,

  // QS revamped display detailed view:
  VIEW_ID_QS_DISPLAY_MIN,
  VIEW_ID_QS_DISPLAY_BRIGHTNESS_SLIDER = VIEW_ID_QS_DISPLAY_MIN,
  VIEW_ID_QS_DISPLAY_SCROLL_CONTENT,
  VIEW_ID_QS_DISPLAY_TILE_CONTAINER,
  VIEW_ID_QS_DISPLAY_MAX = VIEW_ID_QS_DISPLAY_TILE_CONTAINER,

  // Status area trays:
  VIEW_ID_SA_MIN,
  VIEW_ID_SA_DATE_TRAY = VIEW_ID_SA_MIN,
  VIEW_ID_SA_NOTIFICATION_TRAY,
  VIEW_ID_SA_MAX = VIEW_ID_SA_NOTIFICATION_TRAY,

  // Sticky header rows in a scroll view.
  VIEW_ID_STICKY_HEADER,
  // System tray up-arrow icon that shows an update is available.
  VIEW_ID_TRAY_UPDATE_ICON,
  // System tray menu item label for updates (e.g. "Restart to update").
  VIEW_ID_TRAY_UPDATE_MENU_LABEL,

  // Start and end of system tray UserItemButton in UserChooserView. First
  // user gets VIEW_ID_USER_ITEM_BUTTON_START. DCHECKs if the number of user
  // is more than 10.
  VIEW_ID_USER_ITEM_BUTTON_START,
  VIEW_ID_USER_ITEM_BUTTON_END = VIEW_ID_USER_ITEM_BUTTON_START + 10,

  VIEW_ID_USER_VIEW_MEDIA_INDICATOR,
  // Keep alphabetized.
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_VIEW_IDS_H_
