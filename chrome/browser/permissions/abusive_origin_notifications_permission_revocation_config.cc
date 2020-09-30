// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/abusive_origin_notifications_permission_revocation_config.h"

#include "chrome/common/chrome_features.h"

bool AbusiveOriginNotificationsPermissionRevocationConfig::IsEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAbusiveNotificationPermissionRevocation);
}
