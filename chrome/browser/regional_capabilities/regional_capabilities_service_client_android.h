// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_ANDROID_H_
#define CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_ANDROID_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"

namespace variations {
class VariationsService;
}

namespace regional_capabilities {

// Helper that is responsible for providing the `RegionalCapabilitiesService`
// with country data that could be coming from Android platform or //chrome
// layer sources.
class RegionalCapabilitiesServiceClientAndroid
    : public RegionalCapabilitiesServiceClient {
 public:
  explicit RegionalCapabilitiesServiceClientAndroid(
      variations::VariationsService* variations_service);

  ~RegionalCapabilitiesServiceClientAndroid() override;

  void FetchCountryId(CountryIdCallback country_id_fetched_callback) override;

  Program GetDeviceProgram() override;
};

}  // namespace regional_capabilities

#endif  // CHROME_BROWSER_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_CLIENT_ANDROID_H_
