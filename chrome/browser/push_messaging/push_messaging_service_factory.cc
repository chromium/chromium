// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_service_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
PushMessagingServiceImpl* PushMessagingServiceFactory::GetForProfile(
    content::BrowserContext* context) {
  // The Push API is not currently supported in incognito mode.
  // See https://crbug.com/401439.
  if (context->IsOffTheRecord())
    return nullptr;

  if (!instance_id::InstanceIDProfileService::IsInstanceIDEnabled(
          Profile::FromBrowserContext(context)->GetPrefs())) {
    LOG(WARNING) << "PushMessagingService could not be built because "
                    "InstanceID is unexpectedly disabled";
    return nullptr;
  }

  return static_cast<PushMessagingServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PushMessagingServiceFactory* PushMessagingServiceFactory::GetInstance() {
  return base::Singleton<PushMessagingServiceFactory>::get();
}

PushMessagingServiceFactory::PushMessagingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PushMessagingProfileService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(PermissionManagerFactory::GetInstance());
  DependsOn(SiteEngagementServiceFactory::GetInstance());
}

PushMessagingServiceFactory::~PushMessagingServiceFactory() {}

void PushMessagingServiceFactory::RestoreFactoryForTests(
    content::BrowserContext* context) {
  SetTestingFactory(context,
                    base::BindRepeating([](content::BrowserContext* context) {
                      return base::WrapUnique(
                          GetInstance()->BuildServiceInstanceFor(context));
                    }));
}

KeyedService* PushMessagingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(!profile->IsOffTheRecord());
  return new PushMessagingServiceImpl(profile);
}

content::BrowserContext* PushMessagingServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
