// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_H_
#define CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
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
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  explicit RegionalCapabilitiesServiceClient(
      variations::VariationsService* variations_service);
#else
  RegionalCapabilitiesServiceClient();
#endif

  ~RegionalCapabilitiesServiceClient() override;

  int GetFallbackCountryId() override;

  void FetchCountryId(CountryIdCallback country_id_fetched_callback) override;

 private:
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  const int variations_country_id_;
#endif
};

}  // namespace regional_capabilities

#endif  // CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_H_
