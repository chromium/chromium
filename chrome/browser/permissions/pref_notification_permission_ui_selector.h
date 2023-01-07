// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREF_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
#define CHROME_BROWSER_PERMISSIONS_PREF_NOTIFICATION_PERMISSION_UI_SELECTOR_H_

#include "base/memory/raw_ptr.h"
#include "components/permissions/permission_ui_selector.h"

class Profile;

namespace permissions {
class PermissionRequest;
enum class RequestType;
}

// Determines if the quiet prompt UI should be used to display a notification
// permission request on a given site according to user prefs. The quiet UI can
// be enabled in prefs for all sites, either directly by the user in settings,
// or by the AdaptiveQuietNotificationPermissionUiEnabler.
//
// Each instance of this class is long-lived and can support multiple requests.
class PrefNotificationPermissionUiSelector
    : public permissions::PermissionUiSelector {
 public:
  // Constructs an instance in the context of the given |profile|.
  explicit PrefNotificationPermissionUiSelector(Profile* profile);
  ~PrefNotificationPermissionUiSelector() override;

  // Disallow copying and assigning.
  PrefNotificationPermissionUiSelector(
      const PrefNotificationPermissionUiSelector&) = delete;
  PrefNotificationPermissionUiSelector& operator=(
      const PrefNotificationPermissionUiSelector&) = delete;

  // NotificationPermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override;

  void Cancel() override;

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override;

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREF_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
