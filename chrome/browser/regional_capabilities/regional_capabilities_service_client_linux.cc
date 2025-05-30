// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client_linux.h"

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"
#include "components/variations/service/variations_service.h"

namespace regional_capabilities {

RegionalCapabilitiesServiceClientLinux::RegionalCapabilitiesServiceClientLinux(
    variations::VariationsService* variations_service)
    : RegionalCapabilitiesServiceClient(variations_service) {}

RegionalCapabilitiesServiceClientLinux::
    ~RegionalCapabilitiesServiceClientLinux() = default;

void RegionalCapabilitiesServiceClientLinux::FetchCountryId(
    CountryIdCallback on_country_id_fetched) {
  std::move(on_country_id_fetched).Run(GetVariationsLatestCountryId());
}

}  // namespace regional_capabilities
