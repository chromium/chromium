// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client_chromeos.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/country_codes/country_codes.h"
#include "components/variations/service/variations_service.h"

using ::country_codes::CountryId;

namespace regional_capabilities {
namespace {

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

  std::optional<std::string_view> maybe_vpd_region =
      sys_info->GetMachineStatistic(ash::system::kRegionKey);
  if (!maybe_vpd_region.has_value()) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData, kRegionAbsent);
    return {};
  }

  std::string vpd_region = base::ToUpperASCII(maybe_vpd_region.value());
  if (vpd_region.empty()) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData, kRegionEmpty);
    return {};
  }
  if (vpd_region == "GCC" || vpd_region == "LATAM-ES-419" ||
      vpd_region == "NORDIC") {
    // TODO: crbug.com/377475851 - Implement a lookup for the groupings.
    base::UmaHistogramEnumeration(kCrOSMissingVariationData, kGroupedRegion);
    return {};
  }
  if (vpd_region.size() < 2) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData, kRegionTooShort);
    return {};
  }

  bool has_stripped_subkey_info = false;
  if (vpd_region.size() > 2) {
    if (vpd_region[2] != '.') {
      base::UmaHistogramEnumeration(kCrOSMissingVariationData, kRegionTooLong);
      return {};
    }
    vpd_region = vpd_region.substr(0, 2);
    has_stripped_subkey_info = true;
  }

  const CountryId country_code(vpd_region);
  if (has_stripped_subkey_info) {
    base::UmaHistogramBoolean(kVpdRegionSplittingOutcome,
                              country_code.IsValid());
  }
  if (!country_code.IsValid()) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData,
                                  kInvalidCountryCode);
    return {};
  }

  base::UmaHistogramEnumeration(kCrOSMissingVariationData, kValidCountryCode);
  return country_code;
}

}  // namespace

RegionalCapabilitiesServiceClientChromeOS::
    RegionalCapabilitiesServiceClientChromeOS(
        variations::VariationsService* variations_service)
    : RegionalCapabilitiesServiceClient(variations_service),
      variations_permanent_country_id_(
          variations_service
              ? CountryId(base::ToUpperASCII(
                    variations_service->GetStoredPermanentCountry()))
              : CountryId()) {}

RegionalCapabilitiesServiceClientChromeOS::
    ~RegionalCapabilitiesServiceClientChromeOS() = default;

CountryId RegionalCapabilitiesServiceClientChromeOS::GetFallbackCountryId() {
  return GetVpdCountry().value_or(country_codes::GetCurrentCountryID());
}

void RegionalCapabilitiesServiceClientChromeOS::FetchCountryId(
    CountryIdCallback on_country_id_fetched) {
  std::move(on_country_id_fetched).Run(variations_permanent_country_id_);
}

}  // namespace regional_capabilities
