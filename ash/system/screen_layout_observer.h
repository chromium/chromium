// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SCREEN_LAYOUT_OBSERVER_H_
#define ASH_SYSTEM_SCREEN_LAYOUT_OBSERVER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/manager/managed_display_info.h"

namespace ash {

// ScreenLayoutObserver is responsible to send notification to users when screen
// resolution changes or screen rotation changes.
class ASH_EXPORT ScreenLayoutObserver : public display::DisplayManagerObserver {
 public:
  ScreenLayoutObserver();

  ScreenLayoutObserver(const ScreenLayoutObserver&) = delete;
  ScreenLayoutObserver& operator=(const ScreenLayoutObserver&) = delete;

  ~ScreenLayoutObserver() override;

  static const char kNotificationId[];

  // display::DisplayManagerObserver:
  void OnDidApplyDisplayChanges() override;

  // No notification will be shown only for the next ui scale change for the
  // display with |display_id|. This state will be consumed and subsequent
  // changes won't be affected.
  void SetDisplayChangedFromSettingsUI(int64_t display_id);

  // Notifications are shown in production and are not shown in unit tests.
  // Allow individual unit tests to show notifications.
  void set_show_notifications_for_testing(bool show) {
    show_notifications_for_testing_ = show;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ScreenLayoutObserverTestMultiMirroring,
                           DisplayNotifications);
  friend class ScreenLayoutObserverTest;

  using DisplayInfoMap = std::map<int64_t, display::ManagedDisplayInfo>;

  // Scans the current display info and updates |display_info_|. Sets the
  // previous data to |old_info| if it's not NULL.
  void UpdateDisplayInfo(DisplayInfoMap* old_info);

  // Compares the current display settings with |old_info| and determine what
  // message should be shown for notification. Returns true if there's a
  // meaningful change. Note that it's possible to return true and set
  // |out_message| to empty, which means the notification should be removed. It
  // also sets |out_additional_message| which appears in the notification with
  // the |out_message|.
  bool GetUnassociatedDisplayMessage(const DisplayInfoMap& old_info,
                                     std::u16string* out_message,
                                     std::u16string* out_additional_message);

  // Creates or updates the display notification.
  void CreateOrUpdateNotification(const std::u16string& message,
                                  const std::u16string& additional_message);

  // Returns the notification message that should be shown when mirror display
  // mode is exited.
  bool GetExitMirrorModeMessage(std::u16string* out_message,
                                std::u16string* out_additional_message);

  DisplayInfoMap display_info_;

  enum class DisplayMode {
    SINGLE,
    EXTENDED_2,       // 2 displays in extended mode.
    EXTENDED_3_PLUS,  // 3+ displays in extended mode.
    MIRRORING,
    UNIFIED,
    DOCKED
  };

  DisplayMode old_display_mode_ = DisplayMode::SINGLE;
  DisplayMode current_display_mode_ = DisplayMode::SINGLE;

  bool has_unassociated_display_ = false;

  // When the UI scale of a display is modified from the Settings UI, we should
  // ignore this change and avoid showing a notification for it.
  std::set<int64_t> displays_changed_from_settings_ui_;

  bool show_notifications_for_testing_ = true;
};

}  // namespace ash

#endif  // ASH_SYSTEM_SCREEN_LAYOUT_OBSERVER_H_
