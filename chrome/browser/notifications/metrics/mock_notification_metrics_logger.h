// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_METRICS_MOCK_NOTIFICATION_METRICS_LOGGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_METRICS_MOCK_NOTIFICATION_METRICS_LOGGER_H_

#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockNotificationMetricsLogger : public NotificationMetricsLogger {
 public:
  ~MockNotificationMetricsLogger() override;

  // Factory function to be used with NotificationMetricsLoggerFactory's
  // SetTestingFactory method, overriding the default metrics logger.
  static std::unique_ptr<KeyedService> FactoryForTests(
      content::BrowserContext* browser_context);

  MOCK_METHOD0(LogPersistentNotificationClosedByUser, void());
  MOCK_METHOD0(LogPersistentNotificationClosedProgrammatically, void());
  MOCK_METHOD0(LogPersistentNotificationActionButtonClick, void());
  MOCK_METHOD0(LogPersistentNotificationClick, void());
  MOCK_METHOD0(LogPersistentNotificationClickWithoutPermission, void());
  MOCK_METHOD0(LogPersistentNotificationShown, void());
  MOCK_METHOD3(LogPersistentNotificationSize,
               void(const Profile*,
                    const blink::PlatformNotificationData&,
                    const GURL&));

 private:
  MockNotificationMetricsLogger();
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_METRICS_MOCK_NOTIFICATION_METRICS_LOGGER_H_
