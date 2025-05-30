// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"

#include "components/country_codes/country_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace regional_capabilities {
namespace {

TEST(RegionalCapabilitiesServiceClientTest, GetFallbackCountryId) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);

  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
}

}  // namespace
}  // namespace regional_capabilities
