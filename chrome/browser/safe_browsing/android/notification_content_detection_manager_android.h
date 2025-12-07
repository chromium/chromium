// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_NOTIFICATION_CONTENT_DETECTION_MANAGER_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_NOTIFICATION_CONTENT_DETECTION_MANAGER_ANDROID_H_

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/gurl.h"

namespace safe_browsing {

// Enum used to log the suspicious notification warning interaction UKM. Entries
// should not be renumbered and numeric values should never be reused. Must be
// kept in sync with safe_browsing/enums.xml and
// NotificationContentDetectionManager.java.
enum class SuspiciousNotificationWarningInteractions {
  kWarningShown = 0,
  kShowOriginalNotification = 1,
  kUnsubscribe = 2,
  kAlwaysAllow = 3,
  kDismiss = 4,
  kReportAsSafe = 5,
  kReportWarnedNotificationAsSpam = 6,
  kReportUnwarnedNotificationAsSpam = 7,
  kSuppressDuplicateWarning = 8,
  kMaxValue = kSuppressDuplicateWarning,
};

void UpdateSuspiciousNotificationIds(HostContentSettingsMap* hcsm,
                                     const GURL& origin,
                                     std::string notification_id);

void MaybeLogSuspiciousNotificationUnsubscribeUkm(HostContentSettingsMap* hcsm,
                                                  const GURL& origin,
                                                  std::string notification_id,
                                                  Profile* profile);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_NOTIFICATION_CONTENT_DETECTION_MANAGER_ANDROID_H_
