// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_url_filtering_service_factory.h"

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/family_link_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/device_parental_controls_url_filter.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"
#include "components/supervised_user/core/browser/family_link_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace supervised_user {

namespace {

class FilterDelegateImpl : public FamilyLinkUrlFilter::Delegate {
 public:
  bool SupportsWebstoreURL(const GURL& url) const override {
    return IsSupportedChromeExtensionURL(url);
  }
};

// Returns the country code reported by the variations service, or an empty
// string if it is unavailable.
std::string GetSupervisedUserCountryCode() {
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

}  // namespace

// static
SupervisedUserUrlFilteringService*
SupervisedUserUrlFilteringServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SupervisedUserUrlFilteringService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SupervisedUserUrlFilteringService*
SupervisedUserUrlFilteringServiceFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<SupervisedUserUrlFilteringService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
SupervisedUserUrlFilteringServiceFactory*
SupervisedUserUrlFilteringServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserUrlFilteringServiceFactory> instance;
  return instance.get();
}

SupervisedUserUrlFilteringServiceFactory::
    SupervisedUserUrlFilteringServiceFactory()
    : ProfileKeyedServiceFactory("SupervisedUserUrlFilteringService",
                                 BuildProfileSelectionsForRegularAndGuest()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(FamilyLinkSettingsServiceFactory::GetInstance());
}

SupervisedUserUrlFilteringServiceFactory::
    ~SupervisedUserUrlFilteringServiceFactory() = default;

std::unique_ptr<KeyedService>
SupervisedUserUrlFilteringServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  const std::string country_code = GetSupervisedUserCountryCode();
  const version_info::Channel channel = chrome::GetChannel();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();
  FamilyLinkSettingsService& family_link_settings_service =
      CHECK_DEREF(FamilyLinkSettingsServiceFactory::GetInstance()->GetForKey(
          profile->GetProfileKey()));

  return std::make_unique<SupervisedUserUrlFilteringService>(
      std::make_unique<FamilyLinkUrlFilter>(
          family_link_settings_service, *profile->GetPrefs(),
          std::make_unique<FilterDelegateImpl>(),
          std::make_unique<SupervisedUserUrlCheckerClient>(
              identity_manager, url_loader_factory, *profile->GetPrefs(),
              country_code, channel)),
      std::make_unique<DeviceParentalControlsUrlFilter>(
          g_browser_process->device_parental_controls(),
          std::make_unique<SupervisedUserUrlCheckerClient>(
              url_loader_factory, country_code, channel)));
}

}  // namespace supervised_user
