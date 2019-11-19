// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_service_factory.h"

#include "chrome/browser/chromeos/android_sms/pairing_lost_notifier.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace chromeos {

namespace android_sms {

namespace {

bool ShouldStartAndroidSmsService(Profile* profile) {
  const bool multidevice_feature_allowed = multidevice_setup::IsFeatureAllowed(
      multidevice_setup::mojom::Feature::kMessages, profile->GetPrefs());

  const bool has_user_for_profile =
      !!ProfileHelper::Get()->GetUserByProfile(profile);

  return web_app::AreWebAppsEnabled(profile) && !profile->IsGuestSession() &&
         multidevice_feature_allowed && has_user_for_profile;
}

content::BrowserContext* GetBrowserContextForAndroidSms(
    content::BrowserContext* context) {
  // Use original profile to create only one KeyedService instance.
  Profile* original_profile =
      Profile::FromBrowserContext(context)->GetOriginalProfile();
  return ShouldStartAndroidSmsService(original_profile) ? original_profile
                                                        : nullptr;
}

}  // namespace

// static
AndroidSmsServiceFactory* AndroidSmsServiceFactory::GetInstance() {
  static base::NoDestructor<AndroidSmsServiceFactory> factory_instance;
  return factory_instance.get();
}

// static
AndroidSmsService* AndroidSmsServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<AndroidSmsService*>(
      AndroidSmsServiceFactory::GetInstance()->GetServiceForBrowserContext(
          browser_context, true));
}

AndroidSmsServiceFactory::AndroidSmsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AndroidSmsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(app_list::AppListSyncableServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(chromeos::multidevice_setup::MultiDeviceSetupClientFactory::
                GetInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

AndroidSmsServiceFactory::~AndroidSmsServiceFactory() = default;

KeyedService* AndroidSmsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return new AndroidSmsService(
      profile, HostContentSettingsMapFactory::GetForProfile(profile),
      chromeos::multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
          profile),
      web_app::WebAppProviderFactory::GetForProfile(profile),
      app_list::AppListSyncableServiceFactory::GetForProfile(profile));
}

content::BrowserContext* AndroidSmsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextForAndroidSms(context);
}

bool AndroidSmsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AndroidSmsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

void AndroidSmsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  PairingLostNotifier::RegisterProfilePrefs(registry);
  AndroidSmsAppManagerImpl::RegisterProfilePrefs(registry);
}

}  // namespace android_sms

}  // namespace chromeos
