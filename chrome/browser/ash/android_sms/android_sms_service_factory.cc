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
  // Android Messages is deprecated and no longer supported.
  return nullptr;
}

bool AndroidSmsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AndroidSmsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace android_sms
}  // namespace ash
