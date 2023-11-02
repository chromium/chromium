// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_NOTIFICATIONS_PERMISSION_REVOCATION_CONFIG_H_
#define CHROME_BROWSER_PERMISSIONS_NOTIFICATIONS_PERMISSION_REVOCATION_CONFIG_H_

#include "build/build_config.h"

// Field trial configuration for the notifications permission
// revocation.
class NotificationsPermissionRevocationConfig {
 public:
  // Whether or not automatically revoking the notification permission from
  // abusive origins is enabled.
  static bool IsAbusiveOriginPermissionRevocationEnabled();

  // Whether or not automatically revoking the notification permission from
  // sites that may send disruptive notifications is enabled.
  static bool IsDisruptiveOriginPermissionRevocationEnabled();
};

#endif  // CHROME_BROWSER_PERMISSIONS_NOTIFICATIONS_PERMISSION_REVOCATION_CONFIG_H_
