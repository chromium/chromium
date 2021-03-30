// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_util.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "components/country_codes/country_codes.h"
#include "components/embedder_support/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/doh_provider_entry.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace chrome_browser_net {

namespace secure_dns {

class SecureDnsUtilTest : public testing::Test {};

#if !defined(OS_ANDROID)
TEST_F(SecureDnsUtilTest, MigrateProbesPref) {
  const char kAlternateErrorPagesBackup[] = "alternate_error_pages.backup";
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterBooleanPref(
      embedder_support::kAlternateErrorPagesEnabled, true);
  prefs.registry()->RegisterBooleanPref(kAlternateErrorPagesBackup, true);

  const PrefService::Preference* current_pref =
      prefs.FindPreference(embedder_support::kAlternateErrorPagesEnabled);
  const PrefService::Preference* backup_pref =
      prefs.FindPreference(kAlternateErrorPagesBackup);

  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(kAlternateErrorPagesBackup));
}
#endif  // !defined(OS_ANDROID)

TEST(SecureDnsUtil, SplitGroup) {
  EXPECT_THAT(SplitGroup("a"), ElementsAre("a"));
  EXPECT_THAT(SplitGroup("a b"), ElementsAre("a", "b"));
  EXPECT_THAT(SplitGroup("a \tb\nc"), ElementsAre("a", "b\nc"));
  EXPECT_THAT(SplitGroup(" \ta b\n"), ElementsAre("a", "b"));
}

TEST(SecureDnsUtil, IsValidGroup) {
  EXPECT_TRUE(IsValidGroup(""));
  EXPECT_TRUE(IsValidGroup("https://valid"));
  EXPECT_TRUE(IsValidGroup("https://valid https://valid2"));

  EXPECT_FALSE(IsValidGroup("https://valid invalid"));
  EXPECT_FALSE(IsValidGroup("invalid https://valid"));
  EXPECT_FALSE(IsValidGroup("invalid"));
  EXPECT_FALSE(IsValidGroup("invalid invalid2"));
}

TEST(SecureDnsUtil, ApplyDohTemplatePost) {
  std::string post_template("https://valid");
  net::DnsConfigOverrides overrides;
  ApplyTemplate(&overrides, post_template);

  EXPECT_THAT(overrides.dns_over_https_servers,
              testing::Optional(ElementsAre(net::DnsOverHttpsServerConfig(
                  {post_template, true /* use_post */}))));
}

TEST(SecureDnsUtil, ApplyDohTemplateGet) {
  std::string get_template("https://valid/{?dns}");
  net::DnsConfigOverrides overrides;
  ApplyTemplate(&overrides, get_template);

  EXPECT_THAT(overrides.dns_over_https_servers,
              testing::Optional(ElementsAre(net::DnsOverHttpsServerConfig(
                  {get_template, false /* use_post */}))));
}

net::DohProviderEntry::List GetProvidersForTesting() {
  static const auto global1 = net::DohProviderEntry::ConstructForTesting(
      "Provider_Global1", net::DohProviderIdForHistogram(-1), {} /*ip_strs */,
      {} /* dot_hostnames */, "https://global1.provider/dns-query{?dns}",
      "Global Provider 1" /* ui_name */,
      "https://global1.provider/privacy_policy/" /* privacy_policy */,
      true /* display_globally */, {} /* display_countries */);
  static const auto no_display = net::DohProviderEntry::ConstructForTesting(
      "Provider_NoDisplay", net::DohProviderIdForHistogram(-2), {} /*ip_strs */,
      {} /* dot_hostnames */, "https://nodisplay.provider/dns-query{?dns}",
      "No Display Provider" /* ui_name */,
      "https://nodisplay.provider/privacy_policy/" /* privacy_policy */,
      false /* display_globally */, {} /* display_countries */);
  static const auto ee_fr = net::DohProviderEntry::ConstructForTesting(
      "Provider_EE_FR", net::DohProviderIdForHistogram(-3), {} /*ip_strs */,
      {} /* dot_hostnames */, "https://ee.fr.provider/dns-query{?dns}",
      "EE/FR Provider" /* ui_name */,
      "https://ee.fr.provider/privacy_policy/" /* privacy_policy */,
      false /* display_globally */, {"EE", "FR"} /* display_countries */);
  static const auto fr = net::DohProviderEntry::ConstructForTesting(
      "Provider_FR", net::DohProviderIdForHistogram(-4), {} /*ip_strs */,
      {} /* dot_hostnames */, "https://fr.provider/dns-query{?dns}",
      "FR Provider" /* ui_name */,
      "https://fr.provider/privacy_policy/" /* privacy_policy */,
      false /* display_globally */, {"FR"} /* display_countries */);
  static const auto global2 = net::DohProviderEntry::ConstructForTesting(
      "Provider_Global2", net::DohProviderIdForHistogram(-5), {} /*ip_strs */,
      {} /* dot_hostnames */, "https://global2.provider/dns-query{?dns}",
      "Global Provider 2" /* ui_name */,
      "https://global2.provider/privacy_policy/" /* privacy_policy */,
      true /* display_globally */, {} /* display_countries */);
  return {&global1, &no_display, &ee_fr, &fr, &global2};
}

TEST(SecureDnsUtil, LocalProviders) {
  const auto providers = GetProvidersForTesting();

  const auto fr_providers = ProvidersForCountry(
      providers, country_codes::CountryStringToCountryID("FR"));
  EXPECT_THAT(fr_providers, ElementsAre(providers[0], providers[2],
                                        providers[3], providers[4]));

  const auto ee_providers = ProvidersForCountry(
      providers, country_codes::CountryStringToCountryID("EE"));
  EXPECT_THAT(ee_providers,
              ElementsAre(providers[0], providers[2], providers[4]));

  const auto us_providers = ProvidersForCountry(
      providers, country_codes::CountryStringToCountryID("US"));
  EXPECT_THAT(us_providers, ElementsAre(providers[0], providers[4]));

  const auto unknown_providers =
      ProvidersForCountry(providers, country_codes::kCountryIDUnknown);
  EXPECT_THAT(unknown_providers, ElementsAre(providers[0], providers[4]));
}

TEST(SecureDnsUtil, DisabledProviders) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kDnsOverHttps,
      {{"DisabledProviders", "Provider_Global2, , Provider_EE_FR,Unexpected"}});
  EXPECT_THAT(GetDisabledProviders(),
              ElementsAre("Provider_Global2", "Provider_EE_FR", "Unexpected"));
}

TEST(SecureDnsUtil, RemoveDisabled) {
  const auto providers = GetProvidersForTesting();
  const auto filtered_providers = RemoveDisabledProviders(
      providers, {"Provider_Global2", "", "Provider_EE_FR", "Unexpected"});
  EXPECT_THAT(filtered_providers,
              ElementsAre(providers[0], providers[1], providers[3]));
}

TEST(SecureDnsUtil, UpdateDropdownHistograms) {
  base::HistogramTester histograms;

  const auto providers = GetProvidersForTesting();
  UpdateDropdownHistograms(providers, providers[4]->dns_over_https_template,
                           providers[0]->dns_over_https_template);

  const std::string kUmaBase = "Net.DNS.UI.DropdownSelectionEvent";
  histograms.ExpectTotalCount(kUmaBase + ".Ignored", 4u);
  histograms.ExpectTotalCount(kUmaBase + ".Selected", 1u);
  histograms.ExpectTotalCount(kUmaBase + ".Unselected", 1u);
}

TEST(SecureDnsUtil, UpdateDropdownHistogramsCustom) {
  base::HistogramTester histograms;

  const auto providers = GetProvidersForTesting();
  UpdateDropdownHistograms(providers, std::string(),
                           providers[2]->dns_over_https_template);

  const std::string kUmaBase = "Net.DNS.UI.DropdownSelectionEvent";
  histograms.ExpectTotalCount(kUmaBase + ".Ignored", 4u);
  histograms.ExpectTotalCount(kUmaBase + ".Selected", 1u);
  histograms.ExpectTotalCount(kUmaBase + ".Unselected", 1u);
}

}  // namespace secure_dns

}  // namespace chrome_browser_net
