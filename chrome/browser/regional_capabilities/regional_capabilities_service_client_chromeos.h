// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_CHROMEOS_H_
#define CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_CHROMEOS_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_service.h"

namespace variations {
class VariationsService;
}

namespace regional_capabilities {

inline constexpr const char kCrOSMissingVariationData[] =
    "ChromeOS.CountryCode.MissingVariationData";
inline constexpr const char kVpdRegionSplittingOutcome[] =
    "ChromeOS.CountryCode.VPDRegionSplittingOutcome";

// LINT.IfChange(ChromeOSFallbackCountry)
enum class ChromeOSFallbackCountry {
  kNoStatisticsProvider = 0,
  kStatisticsLoadingNotFinished = 1,
  kGroupedRegion = 2,
  kRegionTooShort = 3,
  kRegionTooLong = 4,
  kValidCountryCode = 5,
  // kStrippedSubkeyInformation = 6, // Obsolete
  kRegionAbsent = 7,
  kRegionEmpty = 8,
  kInvalidCountryCode = 9,
  kMaxValue = kInvalidCountryCode,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/chromeos/enums.xml:ChromeOSFallbackCountry)

// Helper that is responsible for providing the `RegionalCapabilitiesService`
// with country data that could be coming from ChromeOS platform or //chrome
// layer sources.
class RegionalCapabilitiesServiceClientChromeOS
    : public RegionalCapabilitiesServiceClient {
 public:
  explicit RegionalCapabilitiesServiceClientChromeOS(
      variations::VariationsService* variations_service);

  ~RegionalCapabilitiesServiceClientChromeOS() override;

  country_codes::CountryId GetFallbackCountryId() override;

  void FetchCountryId(CountryIdCallback country_id_fetched_callback) override;

 private:
  const country_codes::CountryId variations_permanent_country_id_;
};

}  // namespace regional_capabilities

#endif  // CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_CHROMEOS_H_
