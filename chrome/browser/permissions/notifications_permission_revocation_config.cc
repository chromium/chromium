// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/notifications_permission_revocation_config.h"

#include "chrome/common/chrome_features.h"

bool NotificationsPermissionRevocationConfig::
    IsAbusiveOriginPermissionRevocationEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAbusiveNotificationPermissionRevocation);
}

bool NotificationsPermissionRevocationConfig::
    IsDisruptiveOriginPermissionRevocationEnabled() {
  return base::FeatureList::IsEnabled(
      features::kDisruptiveNotificationPermissionRevocation);
}
