// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/variations/service/variations_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client_android.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client_chromeos.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client_linux.h"
#endif

namespace regional_capabilities {
namespace {

std::unique_ptr<RegionalCapabilitiesService::Client>
CreateRegionalCapabilitiesServiceClient() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<RegionalCapabilitiesServiceClientAndroid>(
      g_browser_process->variations_service());
#elif BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<RegionalCapabilitiesServiceClientChromeOS>(
      g_browser_process->variations_service());
#elif BUILDFLAG(IS_LINUX)
  return std::make_unique<RegionalCapabilitiesServiceClientLinux>(
      g_browser_process->variations_service());
#else
  return std::make_unique<RegionalCapabilitiesServiceClient>(
      g_browser_process->variations_service());
#endif
}

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// static
bool RegionalCapabilitiesServiceFactory::
    IsInSearchEngineChoiceScreenRegionForSystemProfile(Profile* profile) {
  CHECK(profile);
  CHECK(profile->IsSystemProfile());
  std::unique_ptr<RegionalCapabilitiesService::Client> client =
      CreateRegionalCapabilitiesServiceClient();
  return RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
      CHECK_DEREF(client));
}
#endif  // BUILDFLAG(IS_WINDOWS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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
  return std::make_unique<RegionalCapabilitiesService>(
      CHECK_DEREF(profile->GetPrefs()),
      CreateRegionalCapabilitiesServiceClient());
}

}  // namespace regional_capabilities
