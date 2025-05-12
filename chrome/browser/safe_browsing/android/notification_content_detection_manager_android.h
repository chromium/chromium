// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_NOTIFICATION_CONTENT_DETECTION_MANAGER_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_NOTIFICATION_CONTENT_DETECTION_MANAGER_ANDROID_H_

#include <string>

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/gurl.h"

namespace safe_browsing {

void UpdateSuspiciousNotificationIds(HostContentSettingsMap* hcsm,
                                     const GURL& origin,
                                     std::string notification_id);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_NOTIFICATION_CONTENT_DETECTION_MANAGER_ANDROID_H_
