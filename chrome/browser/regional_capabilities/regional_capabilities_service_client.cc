// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"

#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "components/country_codes/country_codes.h"
#include "components/variations/service/variations_service.h"

using ::country_codes::CountryId;

namespace regional_capabilities {

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
  return country_codes::GetCurrentCountryID();
}

void RegionalCapabilitiesServiceClient::FetchCountryId(
    CountryIdCallback on_country_id_fetched) {
  std::move(on_country_id_fetched).Run(country_codes::GetCurrentCountryID());
}

}  // namespace regional_capabilities
