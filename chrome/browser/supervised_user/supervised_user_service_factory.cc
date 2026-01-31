// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/version_info/channel.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/family_link_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/browser/family_link_url_filter.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/supervised_user/android/supervised_user_service_platform_delegate.h"
#else
#include "chrome/browser/supervised_user/desktop/supervised_user_service_platform_delegate.h"
#endif

class FilterDelegateImpl
    : public supervised_user::FamilyLinkUrlFilter::Delegate {
 public:
  bool SupportsWebstoreURL(const GURL& url) const override {
    return supervised_user::IsSupportedChromeExtensionURL(url);
  }
};

// static
supervised_user::SupervisedUserService*
SupervisedUserServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<supervised_user::SupervisedUserService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

supervised_user::SupervisedUserService*
SupervisedUserServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return GetForProfile(Profile::FromBrowserContext(context));
}

// static
supervised_user::SupervisedUserService*
SupervisedUserServiceFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<supervised_user::SupervisedUserService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
SupervisedUserServiceFactory* SupervisedUserServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserServiceFactory> instance;
  return instance.get();
}

// static
std::unique_ptr<KeyedService> SupervisedUserServiceFactory::BuildInstanceFor(
    Profile* profile) {
  std::unique_ptr<SupervisedUserServicePlatformDelegate> platform_delegate =
      std::make_unique<SupervisedUserServicePlatformDelegate>(*profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();
  supervised_user::FamilyLinkSettingsService& family_link_settings_service =
      CHECK_DEREF(
          supervised_user::FamilyLinkSettingsServiceFactory::GetInstance()
              ->GetForKey(profile->GetProfileKey()));
  return std::make_unique<supervised_user::SupervisedUserService>(
      identity_manager, url_loader_factory, *profile->GetPrefs(),
      family_link_settings_service,
      SyncServiceFactory::GetInstance()->GetForProfile(profile),
      std::make_unique<supervised_user::FamilyLinkUrlFilter>(
          family_link_settings_service, *profile->GetPrefs(),
          std::make_unique<FilterDelegateImpl>(),
          std::make_unique<
              supervised_user::KidsChromeManagementURLCheckerClient>(
              identity_manager, url_loader_factory, *profile->GetPrefs(),
              platform_delegate->GetCountryCode(),
              platform_delegate->GetChannel())),
      std::move(platform_delegate),
      g_browser_process->device_parental_controls());
}

SupervisedUserServiceFactory::SupervisedUserServiceFactory()
    : ProfileKeyedServiceFactory(
          "SupervisedUserService",
          supervised_user::BuildProfileSelectionsForRegularAndGuest()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#endif
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(supervised_user::FamilyLinkSettingsServiceFactory::GetInstance());
}

SupervisedUserServiceFactory::~SupervisedUserServiceFactory() = default;

std::unique_ptr<KeyedService>
SupervisedUserServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile));
}
