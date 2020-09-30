// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ABUSIVE_ORIGIN_NOTIFICATIONS_PERMISSION_REVOCATION_CONFIG_H_
#define CHROME_BROWSER_PERMISSIONS_ABUSIVE_ORIGIN_NOTIFICATIONS_PERMISSION_REVOCATION_CONFIG_H_

#include "build/build_config.h"

// Field trial configuration for the abusive origin notifications permission
// revocation.
class AbusiveOriginNotificationsPermissionRevocationConfig {
 public:
  // Whether or not automatically revoking the notification permission from
  // abusive origins is enabled.
  static bool IsEnabled();
};

#endif  // CHROME_BROWSER_PERMISSIONS_ABUSIVE_ORIGIN_NOTIFICATIONS_PERMISSION_REVOCATION_CONFIG_H_
