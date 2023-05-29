// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service_factory.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_chrome_management_client_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/features.h"
#include "components/sync/service/sync_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

class FilterDelegateImpl
    : public supervised_user::SupervisedUserURLFilter::Delegate {
 public:
  std::string GetCountryCode() override {
    std::string country;
    variations::VariationsService* variations_service =
        g_browser_process->variations_service();
    if (variations_service) {
      country = variations_service->GetStoredPermanentCountry();
      if (country.empty()) {
        country = variations_service->GetLatestCountry();
      }
    }
    return country;
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
KeyedService* SupervisedUserServiceFactory::BuildInstanceFor(Profile* profile) {
  return new supervised_user::SupervisedUserService(
      KidsChromeManagementClientFactory::GetInstance()->GetForProfile(profile),
      *profile->GetPrefs(),
      *SupervisedUserSettingsServiceFactory::GetInstance()->GetForKey(
          profile->GetProfileKey()),
      *SyncServiceFactory::GetInstance()->GetForProfile(profile),
      base::BindRepeating(supervised_user::IsSupportedChromeExtensionURL),
      std::make_unique<FilterDelegateImpl>(),
      /*can_show_first_time_interstitial_banner=*/!profile->IsNewProfile());
}

SupervisedUserServiceFactory::SupervisedUserServiceFactory()
    : ProfileKeyedServiceFactory(
          "SupervisedUserService",
          base::FeatureList::IsEnabled(
              supervised_user::kUpdateSupervisedUserFactoryCreation)
              ? supervised_user::BuildProfileSelectionsForRegularAndGuest()
              : ProfileSelections::Builder()
                    .WithRegular(ProfileSelection::kRedirectedToOriginal)
                    // TODO(crbug.com/1418376): Check if this service is needed
                    // in Guest mode.
                    .WithGuest(ProfileSelection::kRedirectedToOriginal)
                    .Build()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#endif
  DependsOn(KidsChromeManagementClientFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
}

SupervisedUserServiceFactory::~SupervisedUserServiceFactory() = default;

KeyedService* SupervisedUserServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile));
}
