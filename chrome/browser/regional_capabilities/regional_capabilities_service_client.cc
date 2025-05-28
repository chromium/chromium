// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"

#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/variations/service/variations_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

using ::country_codes::CountryId;

namespace regional_capabilities {
namespace {
#if BUILDFLAG(IS_CHROMEOS)
std::optional<CountryId> GetVpdCountry() {
  using enum ChromeOSFallbackCountry;

  ash::system::StatisticsProvider* sys_info =
      ash::system::StatisticsProvider::GetInstance();
  if (!sys_info) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData,
                                  kNoStatisticsProvider);
    return {};
  }

  if (sys_info->GetLoadingState() !=
      ash::system::StatisticsProvider::LoadingState::kFinished) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData,
                                  kStatisticsLoadingNotFinished);
    return {};
  }

  std::string vpd_region = base::ToUpperASCII(
      sys_info->GetMachineStatistic(ash::system::kRegionKey).value_or(""));
  if (vpd_region == "GCC" || vpd_region == "LATAM-ES-419" ||
      vpd_region == "NORDIC") {
    // TODO: crbug.com/377475851 - Implement a lookup for the groupings.
    base::UmaHistogramEnumeration(kCrOSMissingVariationData, kGroupedRegion);
    return {};
  }

  if (vpd_region.size() < 2) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData, kRegionTooShort);
    return {};
  } else if (vpd_region.size() > 2) {
    if (vpd_region[2] != '.') {
      base::UmaHistogramEnumeration(kCrOSMissingVariationData, kRegionTooLong);
      return {};
    }
    vpd_region = vpd_region.substr(0, 2);
    base::UmaHistogramEnumeration(kCrOSMissingVariationData,
                                  kStrippedSubkeyInformation);
  }

  const CountryId country_code(vpd_region);
  base::UmaHistogramEnumeration(kCrOSMissingVariationData, kValidCountryCode);
  return country_code;
}
#endif
}  // namespace

RegionalCapabilitiesServiceClient::RegionalCapabilitiesServiceClient(
    variations::VariationsService* variations_service)
    : variations_latest_country_id_(
          variations_service ? CountryId(base::ToUpperASCII(
                                   variations_service->GetLatestCountry()))
                             : CountryId()) {}

RegionalCapabilitiesServiceClient::~RegionalCapabilitiesServiceClient() =
    default;

CountryId RegionalCapabilitiesServiceClient::GetVariationsLatestCountryId() {
  return variations_latest_country_id_;
}

CountryId RegionalCapabilitiesServiceClient::GetFallbackCountryId() {
#if BUILDFLAG(IS_CHROMEOS)
  if (const std::optional<CountryId> vpd_country = GetVpdCountry();
      vpd_country.has_value()) {
    return *vpd_country;
  }
#endif
  return country_codes::GetCurrentCountryID();
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
void RegionalCapabilitiesServiceClient::FetchCountryId(
    CountryIdCallback on_country_id_fetched) {
  std::move(on_country_id_fetched).Run(variations_latest_country_id_);
}
#else
// On other platforms, defer to `GetCurrentCountryID()`.
void RegionalCapabilitiesServiceClient::FetchCountryId(
    CountryIdCallback on_country_id_fetched) {
  std::move(on_country_id_fetched).Run(country_codes::GetCurrentCountryID());
}
#endif

}  // namespace regional_capabilities
