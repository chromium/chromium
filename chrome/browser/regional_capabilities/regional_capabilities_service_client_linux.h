// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_LINUX_H_
#define CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_LINUX_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"

namespace variations {
class VariationsService;
}

namespace regional_capabilities {

// Helper that is responsible for providing the `RegionalCapabilitiesService`
// with country data that could be coming from Linux platform or //chrome
// layer sources.
class RegionalCapabilitiesServiceClientLinux
    : public RegionalCapabilitiesServiceClient {
 public:
  explicit RegionalCapabilitiesServiceClientLinux(
      variations::VariationsService* variations_service);

  ~RegionalCapabilitiesServiceClientLinux() override;

  void FetchCountryId(CountryIdCallback country_id_fetched_callback) override;

 private:
  const country_codes::CountryId variations_permanent_country_id_;
};

}  // namespace regional_capabilities

#endif  // CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_LINUX_H_
