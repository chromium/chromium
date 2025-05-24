// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/regional_capabilities/regional_capabilities_test_utils.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::country_codes::CountryId;

namespace regional_capabilities {
namespace {

constexpr CountryId kUsaCountryId("US");

}  // namespace
class RegionalCapabilitiesServiceFactoryBrowserTest
    : public InProcessBrowserTest {};

struct VariationsCountryTestParam {
  std::string test_suffix;
  std::string variations_country_code;
  std::optional<CountryId> expected_country_id;
};

class RegionalCapabilitiesServiceFactoryBrowserTestForVariationsCountry
    : public RegionalCapabilitiesServiceFactoryBrowserTest,
      public testing::WithParamInterface<VariationsCountryTestParam> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry,
        GetVariationsCountryCode());
  }

  std::string GetVariationsCountryCode() const {
    return GetParam().variations_country_code;
  }

  CountryId GetExpectedCountryId() const {
    return GetParam().expected_country_id.value_or(kUsaCountryId);
  }
};

// Test devices config make `country_codes::GetCurrentCountryID()` point to "US"
// by default.
const VariationsCountryTestParam kTestParams[] = {
    {.test_suffix = "FR",
     .variations_country_code = "fr",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
     .expected_country_id = CountryId("FR")
#else
     .expected_country_id = kUsaCountryId
#endif
    },
    {.test_suffix = "Unset",
     .variations_country_code = "",
     .expected_country_id = kUsaCountryId},
};

IN_PROC_BROWSER_TEST_P(
    RegionalCapabilitiesServiceFactoryBrowserTestForVariationsCountry,
    GetCountryId) {
  auto& service = CHECK_DEREF(
      RegionalCapabilitiesServiceFactory::GetForProfile(browser()->profile()));

  EXPECT_EQ(service.GetCountryId().GetForTesting(), GetExpectedCountryId());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    RegionalCapabilitiesServiceFactoryBrowserTestForVariationsCountry,
    testing::ValuesIn(kTestParams),
    [](const testing::TestParamInfo<VariationsCountryTestParam>& info) {
      return info.param.test_suffix;
    });

}  // namespace regional_capabilities
