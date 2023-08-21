// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/android_sms_service_factory.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace ash {
namespace android_sms {

namespace {

bool ShouldStartAndroidSmsService(Profile* profile) {
  if (base::FeatureList::IsEnabled(
          features::kDisableMessagesCrossDeviceIntegration)) {
    return false;
  }

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
  DependsOn(multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

AndroidSmsServiceFactory::~AndroidSmsServiceFactory() = default;

std::unique_ptr<KeyedService>
AndroidSmsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return std::make_unique<AndroidSmsService>(
      profile, HostContentSettingsMapFactory::GetForProfile(profile),
      multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(profile),
      web_app::WebAppProvider::GetDeprecated(profile),
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
  AndroidSmsAppManagerImpl::RegisterProfilePrefs(registry);
}

}  // namespace android_sms
}  // namespace ash
