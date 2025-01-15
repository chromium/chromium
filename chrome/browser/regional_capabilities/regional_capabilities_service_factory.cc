// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_service.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "components/variations/service/variations_service.h"
#endif

namespace regional_capabilities {

namespace {

class RegionalCapabilitiesServiceClient
    : public regional_capabilities::RegionalCapabilitiesService::Client {
 public:
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // On ChromeOS and Linux, get it from `VariationsService`, by polling at
  // every startup until it is found.
  explicit RegionalCapabilitiesServiceClient(
      variations::VariationsService* variations_service)
      : country_id_(
            variations_service
                ? country_codes::CountryStringToCountryID(base::ToUpperASCII(
                      variations_service->GetLatestCountry()))
                : country_codes::kCountryIDUnknown) {}

  void FetchCountryId(CountryIdCallback on_country_id_fetched) override {
    std::move(on_country_id_fetched).Run(country_id_);
  }

 private:
  int country_id_;
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
};

}  // namespace

// static
RegionalCapabilitiesService* RegionalCapabilitiesServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<RegionalCapabilitiesService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
RegionalCapabilitiesServiceFactory*
RegionalCapabilitiesServiceFactory::GetInstance() {
  static base::NoDestructor<RegionalCapabilitiesServiceFactory> instance;
  return instance.get();
}

RegionalCapabilitiesServiceFactory::RegionalCapabilitiesServiceFactory()
    : ProfileKeyedServiceFactory(
          "RegionalCapabilitiesService",
          ProfileSelections::Builder()
              // Scope rationale: Service needed where we have some features
              // or checks that could be dependent on the country or some other
              // profile or device state. OTR profiles should defer to the
              // parent profile.
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithSystem(ProfileSelection::kNone)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

RegionalCapabilitiesServiceFactory::~RegionalCapabilitiesServiceFactory() =
    default;

std::unique_ptr<KeyedService>
RegionalCapabilitiesServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto regional_capabilities_service_client =
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
      std::make_unique<RegionalCapabilitiesServiceClient>(
          g_browser_process->variations_service());
#else
      std::make_unique<RegionalCapabilitiesServiceClient>();
#endif

  return std::make_unique<RegionalCapabilitiesService>(
      CHECK_DEREF(profile->GetPrefs()),
      std::move(regional_capabilities_service_client));
}

}  // namespace regional_capabilities
