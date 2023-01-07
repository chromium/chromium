// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/notification_permission_review_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
NotificationPermissionsReviewServiceFactory*
NotificationPermissionsReviewServiceFactory::GetInstance() {
  return base::Singleton<NotificationPermissionsReviewServiceFactory>::get();
}

// static
permissions::NotificationPermissionsReviewService*
NotificationPermissionsReviewServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<permissions::NotificationPermissionsReviewService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

NotificationPermissionsReviewServiceFactory::
    NotificationPermissionsReviewServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "NotificationPermissionsReviewService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

NotificationPermissionsReviewServiceFactory::
    ~NotificationPermissionsReviewServiceFactory() = default;

KeyedService*
NotificationPermissionsReviewServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new permissions::NotificationPermissionsReviewService(
      HostContentSettingsMapFactory::GetForProfile(context));
}
