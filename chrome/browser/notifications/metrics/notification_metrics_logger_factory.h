// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_METRICS_NOTIFICATION_METRICS_LOGGER_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_METRICS_NOTIFICATION_METRICS_LOGGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

class NotificationMetricsLogger;

class NotificationMetricsLoggerFactory : public ProfileKeyedServiceFactory {
 public:
  static NotificationMetricsLogger* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static NotificationMetricsLoggerFactory* GetInstance();

  NotificationMetricsLoggerFactory(const NotificationMetricsLoggerFactory&) =
      delete;
  NotificationMetricsLoggerFactory& operator=(
      const NotificationMetricsLoggerFactory&) = delete;

 private:
  friend base::NoDestructor<NotificationMetricsLoggerFactory>;

  NotificationMetricsLoggerFactory();

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_METRICS_NOTIFICATION_METRICS_LOGGER_FACTORY_H_
