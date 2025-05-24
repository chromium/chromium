// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for the SafeBrowsing notification content detection code.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_UTIL_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom-forward.h"

namespace content {
struct NotificationDatabaseData;
}  // namespace content

namespace optimization_guide {
class ModelQualityLogsUploaderService;
}  // namespace optimization_guide

namespace safe_browsing {

struct NotificationContentDetectionMQLSMetadata {
  NotificationContentDetectionMQLSMetadata(
      bool did_show_warning,
      bool did_user_unsubscribe,
      blink::mojom::EngagementLevel site_engagement_score);
  bool did_show_warning_;
  bool did_user_unsubscribe_;
  blink::mojom::EngagementLevel site_engagement_score_;
};

void SendNotificationContentDetectionDataToMQLSServer(
    base::WeakPtr<optimization_guide::ModelQualityLogsUploaderService>
        logs_uploader_service,
    NotificationContentDetectionMQLSMetadata metadata,
    bool success,
    const content::NotificationDatabaseData& notification_database_data);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_UTIL_H_
