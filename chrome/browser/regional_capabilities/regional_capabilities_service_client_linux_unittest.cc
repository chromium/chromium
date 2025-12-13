// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client_linux.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_test_environment.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/test_variations_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::country_codes::CountryId;

namespace regional_capabilities {
namespace {

class RegionalCapabilitiesServiceClientLinuxTest : public testing::Test {
 public:
  RegionalCapabilitiesServiceClientLinuxTest() = default;

  ~RegionalCapabilitiesServiceClientLinuxTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;

  RegionalCapabilitiesTestEnvironment rcaps_env_;
};

TEST_F(RegionalCapabilitiesServiceClientLinuxTest, FetchCountryId) {
  // Set up variations_service::GetLatestCountry().
  rcaps_env_.pref_service().SetString(variations::prefs::kVariationsCountry,
                                      "fr");

  // Set up variations_service::GetStoredPermanentCountry().
  rcaps_env_.variations_service().OverrideStoredPermanentCountry("DE");

  RegionalCapabilitiesServiceClientLinux client(
      &rcaps_env_.variations_service());

  base::test::TestFuture<CountryId> future;
  client.FetchCountryId(future.GetCallback());
  EXPECT_EQ(future.Get(), CountryId("DE"));
}

}  // namespace
}  // namespace regional_capabilities
