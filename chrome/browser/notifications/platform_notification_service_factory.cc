// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/platform_notification_service_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"

// static
PlatformNotificationServiceImpl*
PlatformNotificationServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PlatformNotificationServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
PlatformNotificationServiceFactory*
PlatformNotificationServiceFactory::GetInstance() {
  static base::NoDestructor<PlatformNotificationServiceFactory> instance;
  return instance.get();
}

PlatformNotificationServiceFactory::PlatformNotificationServiceFactory()
    : ProfileKeyedServiceFactory(
          "PlatformNotificationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
  DependsOn(NotificationMetricsLoggerFactory::GetInstance());
  DependsOn(ukm::UkmBackgroundRecorderFactory::GetInstance());
}

std::unique_ptr<KeyedService>
PlatformNotificationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<PlatformNotificationServiceImpl>(
      Profile::FromBrowserContext(context));
}
