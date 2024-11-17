// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_MOCK_NOTIFICATION_CONTENT_DETECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_MOCK_NOTIFICATION_CONTENT_DETECTION_SERVICE_H_

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_service.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class MockNotificationContentDetectionService
    : public safe_browsing::NotificationContentDetectionService {
 public:
  MockNotificationContentDetectionService(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      content::BrowserContext* browser_context);
  ~MockNotificationContentDetectionService() override;

  MOCK_METHOD2(MaybeCheckNotificationContentDetectionModel,
               void(const blink::PlatformNotificationData&, const GURL&));

  static std::unique_ptr<KeyedService> FactoryForTests(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      content::BrowserContext* context);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_MOCK_NOTIFICATION_CONTENT_DETECTION_SERVICE_H_
