// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
PlatformNotificationServiceImpl*
PlatformNotificationServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PlatformNotificationServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
PlatformNotificationServiceFactory*
PlatformNotificationServiceFactory::GetInstance() {
  return base::Singleton<PlatformNotificationServiceFactory>::get();
}

PlatformNotificationServiceFactory::PlatformNotificationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PlatformNotificationService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
  DependsOn(NotificationMetricsLoggerFactory::GetInstance());
  DependsOn(ukm::UkmBackgroundRecorderFactory::GetInstance());
}

KeyedService* PlatformNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PlatformNotificationServiceImpl(
      Profile::FromBrowserContext(context));
}

content::BrowserContext*
PlatformNotificationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
