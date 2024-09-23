// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"

#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"

// static
NotificationMetricsLogger*
NotificationMetricsLoggerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<NotificationMetricsLogger*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 true /* create */));
}

// static
NotificationMetricsLoggerFactory*
NotificationMetricsLoggerFactory::GetInstance() {
  static base::NoDestructor<NotificationMetricsLoggerFactory> instance;
  return instance.get();
}

NotificationMetricsLoggerFactory::NotificationMetricsLoggerFactory()
    : ProfileKeyedServiceFactory(
          "NotificationMetricsLogger",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

std::unique_ptr<KeyedService>
NotificationMetricsLoggerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<NotificationMetricsLogger>();
}
