// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TRAY_H_
#define ASH_PUBLIC_CPP_SYSTEM_TRAY_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/strings/string16.h"

namespace ash {

struct LocaleInfo;
class SystemTrayClient;
enum class NotificationStyle;
enum class UpdateSeverity;
enum class UpdateType;

// Public interface to control the system tray bubble in ash.
class ASH_PUBLIC_EXPORT SystemTray {
 public:
  static SystemTray* Get();

  // Sets the client interface in the browser.
  virtual void SetClient(SystemTrayClient* client) = 0;

  // Sets the enabled state of the tray on the primary display. If not |enabled|
  // any open menu will be closed.
  virtual void SetPrimaryTrayEnabled(bool enabled) = 0;

  // Sets the visibility of the tray on the primary display.
  virtual void SetPrimaryTrayVisible(bool visible) = 0;

  // Sets the clock to use 24 hour time formatting if |use_24_hour| is true.
  // Otherwise sets 12 hour time formatting.
  virtual void SetUse24HourClock(bool use_24_hour) = 0;

  // Creates or updates an item in the system tray menu with information about
  // enterprise management. The item appears if |enterprise_display_domain| is
  // not empty or |active_directory_managed| is true.
  virtual void SetEnterpriseDisplayDomain(
      const std::string& enterprise_display_domain,
      bool active_directory_managed) = 0;

  // Shows or hides an item in the system tray indicating that performance
  // tracing is running.
  virtual void SetPerformanceTracingIconVisible(bool visible) = 0;

  // Sets the list of supported UI locales. |current_locale_iso_code| refers to
  // the locale currently used by the UI.
  virtual void SetLocaleList(std::vector<LocaleInfo> locale_list,
                             const std::string& current_locale_iso_code) = 0;

  // Shows an icon in the system tray or a notification on the unified system
  // menu indicating that a software update is available. Once shown, the icon
  // or the notification persists until reboot.
  // |severity| specifies how critical is the update.
  // |factory_reset_required| is true if during the update the device will
  //     be wiped.
  // |rollback| specifies whether the update is actually an admin-initiated
  //     rollback. This implies that a the device will be wiped.
  // |update_type| specifies the component which has been updated.
  //
  // These values are used to control the icon, color and the text of the
  // tooltip or the notification.
  virtual void ShowUpdateIcon(UpdateSeverity severity,
                              bool factory_reset_required,
                              bool rollback,
                              UpdateType update_type) = 0;

  // Sets new strings for update notification in the unified system menu,
  // according to different policies, when there is an update available
  // (it may be recommended or required, from Relaunch Notification policy,
  // for example).
  // Providing these strings allows the update countdown logic to remain in
  // //chrome/browser, where it is shared with other platforms.
  // |style| specifies the type of notification, according to the policy
  // (default, recommended or required).
  // |notification_title| the title of the notification, which overwrites
  // the default.
  // |notification_body| the new notification body which overwrites the default.
  virtual void SetUpdateNotificationState(
      NotificationStyle style,
      const base::string16& notification_title,
      const base::string16& notification_body) = 0;

  // If |visible| is true, shows an icon in the system tray which indicates that
  // a software update is available but user's agreement is required as current
  // connection is cellular. If |visible| is false, hides the icon because the
  // user's one time permission on update over cellular connection has been
  // granted.
  virtual void SetUpdateOverCellularAvailableIconVisible(bool visible) = 0;

  // Shows the volume slider bubble shown at the right bottom of screen.
  virtual void ShowVolumeSliderBubble() = 0;

  // Shows the network detailed view bubble at the right bottom of the primary
  // display. Set |show_by_click| to true if bubble is shown by mouse or gesture
  // click (it is used e.g. for timing histograms).
  virtual void ShowNetworkDetailedViewBubble(bool show_by_click) = 0;

 protected:
  SystemTray();
  virtual ~SystemTray();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TRAY_H_
