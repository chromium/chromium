// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_H_
#define CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_service.h"

namespace variations {
class VariationsService;
}

namespace regional_capabilities {

// Helper that is responsible for providing the `RegionalCapabilitiesService`
// with country data that could be coming from platform-specific or //chrome
// layer sources.
class RegionalCapabilitiesServiceClient
    : public RegionalCapabilitiesService::Client {
 public:
  explicit RegionalCapabilitiesServiceClient(
      variations::VariationsService* variations_service);

  ~RegionalCapabilitiesServiceClient() override;

  country_codes::CountryId GetVariationsLatestCountryId() override;

  country_codes::CountryId GetFallbackCountryId() override;

  void FetchCountryId(CountryIdCallback country_id_fetched_callback) override;

 private:
  const country_codes::CountryId variations_latest_country_id_;
};

}  // namespace regional_capabilities

#endif  // CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_H_
