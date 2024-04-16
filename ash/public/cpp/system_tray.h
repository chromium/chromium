// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TRAY_H_
#define ASH_PUBLIC_CPP_SYSTEM_TRAY_H_

#include <string>

#include "ash/ash_export.h"

#include "base/memory/raw_ptr.h"

namespace ash {

class SystemTrayClient;
enum class DeferredUpdateState;
enum class NotificationStyle;
enum class UpdateSeverity;
struct DeviceEnterpriseInfo;
struct LocaleInfo;
struct RelaunchNotificationState;

namespace phonehub {
class PhoneHubManager;
}

// Public interface to control the system tray bubble in ash.
class ASH_EXPORT SystemTray {
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
  // enterprise management.
  virtual void SetDeviceEnterpriseInfo(
      const DeviceEnterpriseInfo& device_enterprise_info) = 0;

  // Creates or updates an item in the system tray menu with information about
  // enterprise management.
  // |account_domain_manager| may be either a domain name (foo.com) or an email
  // address (user@foo.com). These strings will not be sanitized and so must
  // come from a trusted location.
  virtual void SetEnterpriseAccountDomainInfo(
      const std::string& account_domain_manager) = 0;

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
                              bool rollback) = 0;

  // Changes the update notification in the unified system menu,
  // according to different policies, when there is an update available
  // (it may be recommended or required, from Relaunch Notification policy,
  // for example).
  // Providing the `RelaunchNotificationState` allows the update countdown logic
  // to remain in //chrome/browser, where it is shared with other platforms.
  virtual void SetRelaunchNotificationState(
      const RelaunchNotificationState& relaunch_notification_state) = 0;

  // Resets update state to hide the update icon and notification. It is called
  // when a new update starts before the current update is applied.
  virtual void ResetUpdateState() = 0;

  // Set deferred update state to be either showing a dialog or showing an icon
  // in the system tray to indicate that a update is downloaded but deferred.
  virtual void SetUpdateDeferred(DeferredUpdateState state) = 0;

  // If |visible| is true, shows an icon in the system tray which indicates that
  // a software update is available but user's agreement is required as current
  // connection is cellular. If |visible| is false, hides the icon because the
  // user's one time permission on update over cellular connection has been
  // granted.
  virtual void SetUpdateOverCellularAvailableIconVisible(bool visible) = 0;

  // Sets whether end of life notice should be shown in quick settings.
  virtual void SetShowEolNotice(bool show) = 0;

  // Sets whether the extended updates support notice should be shown
  // in quick settings.
  virtual void SetShowExtendedUpdatesNotice(bool show) = 0;

  // Shows the volume slider bubble shown at the right bottom of screen.
  virtual void ShowVolumeSliderBubble() = 0;

  // Shows the network detailed view bubble at the right bottom of the primary
  // display.
  virtual void ShowNetworkDetailedViewBubble() = 0;

  // Provides Phone Hub functionality to the system tray.
  virtual void SetPhoneHubManager(
      phonehub::PhoneHubManager* phone_hub_manager) = 0;

 protected:
  SystemTray() = default;
  virtual ~SystemTray() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TRAY_H_
