// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client_linux.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/variations/service/variations_service.h"

using ::country_codes::CountryId;

namespace regional_capabilities {

RegionalCapabilitiesServiceClientLinux::RegionalCapabilitiesServiceClientLinux(
    variations::VariationsService* variations_service)
    : RegionalCapabilitiesServiceClient(variations_service),
      variations_permanent_country_id_(
          variations_service
              ? CountryId(base::ToUpperASCII(
                    variations_service->GetStoredPermanentCountry()))
              : CountryId()) {}

RegionalCapabilitiesServiceClientLinux::
    ~RegionalCapabilitiesServiceClientLinux() = default;

void RegionalCapabilitiesServiceClientLinux::FetchCountryId(
    CountryIdCallback on_country_id_fetched) {
  const CountryId fetched_country_id =
      base::FeatureList::IsEnabled(
          switches::kUseFinchPermanentCountryForFetchCountryId)
          ? variations_permanent_country_id_
          : GetVariationsLatestCountryId();

  std::move(on_country_id_fetched).Run(fetched_country_id);
}

}  // namespace regional_capabilities
