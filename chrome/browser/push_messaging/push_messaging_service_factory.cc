// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_service_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/android_sms/android_sms_service_factory.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#endif

// static
PushMessagingServiceImpl* PushMessagingServiceFactory::GetForProfile(
    content::BrowserContext* context) {
  // The Push API is not currently supported in incognito mode.
  // See https://crbug.com/401439.
  if (context->IsOffTheRecord())
    return nullptr;

  return static_cast<PushMessagingServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PushMessagingServiceFactory* PushMessagingServiceFactory::GetInstance() {
  static base::NoDestructor<PushMessagingServiceFactory> instance;
  return instance.get();
}

PushMessagingServiceFactory::PushMessagingServiceFactory()
    : ProfileKeyedServiceFactory(
          "PushMessagingProfileService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(PermissionManagerFactory::GetInstance());
  DependsOn(site_engagement::SiteEngagementServiceFactory::GetInstance());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DependsOn(ash::android_sms::AndroidSmsServiceFactory::GetInstance());
  DependsOn(
      ash::multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
#endif
}

PushMessagingServiceFactory::~PushMessagingServiceFactory() = default;

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
