// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_util.h"

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "components/country_codes/country_codes.h"
#include "components/embedder_support/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace chrome_browser_net::secure_dns {

class SecureDnsUtilTest : public testing::Test {};

TEST_F(SecureDnsUtilTest, MigrateProbesPrefForwardDefault) {
  const char kAlternateErrorPagesBackup[] = "alternate_error_pages.backup";
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterBooleanPref(
      embedder_support::kAlternateErrorPagesEnabled, true);
  prefs.registry()->RegisterBooleanPref(kAlternateErrorPagesBackup, true);

  const PrefService::Preference* current_pref =
      prefs.FindPreference(embedder_support::kAlternateErrorPagesEnabled);
  const PrefService::Preference* backup_pref =
      prefs.FindPreference(kAlternateErrorPagesBackup);

  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_FALSE(backup_pref->HasUserSetting());

  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(kAlternateErrorPagesBackup));
}

TEST_F(SecureDnsUtilTest, MigrateProbesPrefForwardCustomDisabled) {
  const char kAlternateErrorPagesBackup[] = "alternate_error_pages.backup";
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterBooleanPref(
      embedder_support::kAlternateErrorPagesEnabled, true);
  prefs.registry()->RegisterBooleanPref(kAlternateErrorPagesBackup, true);

  prefs.SetBoolean(embedder_support::kAlternateErrorPagesEnabled, false);

  const PrefService::Preference* current_pref =
      prefs.FindPreference(embedder_support::kAlternateErrorPagesEnabled);
  const PrefService::Preference* backup_pref =
      prefs.FindPreference(kAlternateErrorPagesBackup);

  EXPECT_TRUE(current_pref->HasUserSetting());
  EXPECT_FALSE(backup_pref->HasUserSetting());

  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_FALSE(prefs.GetBoolean(kAlternateErrorPagesBackup));
}

TEST(SecureDnsUtil, MakeProbeRunner) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto doh_config = *net::DnsOverHttpsConfig::FromString(
      "https://valid https://valid/{?dns}");
  network::TestNetworkContext test_context;
  auto prober = MakeProbeRunner(
      doh_config,
      base::BindLambdaForTesting(
          [&]() -> network::mojom::NetworkContext* { return &test_context; }));
  auto overrides = prober->GetConfigOverridesForTesting();
  EXPECT_EQ(1, overrides.attempts);
  EXPECT_EQ(std::vector<std::string>(), overrides.search);
  EXPECT_EQ(net::SecureDnsMode::kSecure, overrides.secure_dns_mode);
  EXPECT_EQ(doh_config, overrides.dns_over_https_config);
}

BASE_FEATURE(kDohProviderFeatureForProvider_Global1,
             "DohProviderFeatureForProvider_Global1",
             base::FEATURE_ENABLED_BY_DEFAULT);
const auto kProviderGlobal1 = net::DohProviderEntry::ConstructForTesting(
    "Provider_Global1",
    &kDohProviderFeatureForProvider_Global1,
    /*ip_strs=*/{},
    /*dns_over_tls_hostnames=*/{},
    "https://global1.provider/dns-query{?dns}",
    /*ui_name=*/"Global Provider 1",
    /*privacy_policy=*/"https://global1.provider/privacy_policy/",
    /*display_globally=*/true,
    /*display_countries=*/{});
BASE_FEATURE(kDohProviderFeatureForProvider_NoDisplay,
             "DohProviderFeatureForProvider_NoDisplay",
             base::FEATURE_ENABLED_BY_DEFAULT);
const auto kProviderNoDisplay = net::DohProviderEntry::ConstructForTesting(
    "Provider_NoDisplay",
    &kDohProviderFeatureForProvider_NoDisplay,
    /*ip_strs=*/{},
    /*dns_over_tls_hostnames=*/{},
    "https://nodisplay.provider/dns-query{?dns}",
    /*ui_name=*/"No Display Provider",
    /*privacy_policy=*/"https://nodisplay.provider/privacy_policy/",
    /*display_globally=*/false,
    /* display_countries */ {});
BASE_FEATURE(kDohProviderFeatureForProvider_EE_FR,
             "DohProviderFeatureForProvider_EE_FR",
             base::FEATURE_DISABLED_BY_DEFAULT);
const auto kProviderEeFrDisabled = net::DohProviderEntry::ConstructForTesting(
    "Provider_EE_FR",
    &kDohProviderFeatureForProvider_EE_FR,
    /*ip_strs=*/{},
    /*dns_over_tls_hostnames=*/{},
    "https://ee.fr.provider/dns-query{?dns}",
    /*ui_name=*/"EE/FR Provider",
    /*privacy_policy=*/"https://ee.fr.provider/privacy_policy/",
    /*display_globally=*/false,
    /*display_countries=*/{"EE", "FR"});
BASE_FEATURE(kDohProviderFeatureForProvider_FR,
             "DohProviderFeatureForProvider_FR",
             base::FEATURE_ENABLED_BY_DEFAULT);
const auto kProviderFr = net::DohProviderEntry::ConstructForTesting(
    "provider_FR",
    &kDohProviderFeatureForProvider_FR,
    /*ip_strs=*/{},
    /*dns_over_tls_hostnames=*/{},
    "https://fr.provider/dns-query{?dns}",
    /*ui_name=*/"FR Provider",
    /*privacy_policy=*/"https://fr.provider/privacy_policy/",
    /*display_globally=*/false,
    /*display_countries=*/{"FR"});
BASE_FEATURE(kDohProviderFeatureForProvider_Global2,
             "DohProviderFeatureForProvider_Global2",
             base::FEATURE_ENABLED_BY_DEFAULT);
const auto kProviderGlobal2 = net::DohProviderEntry::ConstructForTesting(
    "Provider_Global2",
    &kDohProviderFeatureForProvider_Global2,
    /*ip_strs=*/{},
    /*dns_over_tls_hostnames=*/{},
    "https://global2.provider/dns-query{?dns}",
    /*ui_name=*/"Global Provider 2",
    /*privacy_policy=*/"https://global2.provider/privacy_policy/",
    /*display_globally=*/true,
    /*display_countries=*/{});
BASE_FEATURE(kDohProviderFeatureForProvider_Global3,
             "DohProviderFeatureForProvider_Global3",
             base::FEATURE_DISABLED_BY_DEFAULT);
const auto kProviderGlobal3Disabled =
    net::DohProviderEntry::ConstructForTesting(
        "Provider_Global3",
        &kDohProviderFeatureForProvider_Global3,
        /*ip_strs=*/{},
        /*dns_over_tls_hostnames=*/{},
        "https://global3.provider/dns-query{?dns}",
        /*ui_name=*/"Global Provider 3",
        /*privacy_policy=*/"https://global3.provider/privacy_policy/",
        /*display_globally=*/true,
        /*display_countries=*/{});

net::DohProviderEntry::List GetProvidersForTesting() {
  return {&kProviderGlobal1, &kProviderNoDisplay, &kProviderEeFrDisabled,
          &kProviderFr,      &kProviderGlobal2,   &kProviderGlobal3Disabled};
}

TEST(SecureDnsUtil, LocalProviders) {
  const auto providers = GetProvidersForTesting();

  const auto fr_providers = ProvidersForCountry(
      providers, country_codes::CountryStringToCountryID("FR"));
  EXPECT_THAT(
      fr_providers,
      ElementsAre(&kProviderGlobal1, &kProviderEeFrDisabled, &kProviderFr,
                  &kProviderGlobal2, &kProviderGlobal3Disabled));

  const auto ee_providers = ProvidersForCountry(
      providers, country_codes::CountryStringToCountryID("EE"));
  EXPECT_THAT(ee_providers,
              ElementsAre(&kProviderGlobal1, &kProviderEeFrDisabled,
                          &kProviderGlobal2, &kProviderGlobal3Disabled));

  const auto us_providers = ProvidersForCountry(
      providers, country_codes::CountryStringToCountryID("US"));
  EXPECT_THAT(us_providers, ElementsAre(&kProviderGlobal1, &kProviderGlobal2,
                                        &kProviderGlobal3Disabled));

  const auto unknown_providers =
      ProvidersForCountry(providers, country_codes::kCountryIDUnknown);
  EXPECT_THAT(unknown_providers,
              ElementsAre(&kProviderGlobal1, &kProviderGlobal2,
                          &kProviderGlobal3Disabled));
}

TEST(SecureDnsUtil, SelectEnabledProviders) {
  const net::DohProviderEntry::List kListEmpty;
  const net::DohProviderEntry::List kListOneEnabled{&kProviderGlobal1};
  const net::DohProviderEntry::List kListTwoDisabled{&kProviderGlobal3Disabled,
                                                     &kProviderEeFrDisabled};
  EXPECT_EQ(SelectEnabledProviders(kListEmpty), kListEmpty);
  EXPECT_EQ(SelectEnabledProviders(kListOneEnabled), kListOneEnabled);
  EXPECT_EQ(SelectEnabledProviders(kListTwoDisabled), kListEmpty);
  EXPECT_THAT(SelectEnabledProviders(GetProvidersForTesting()),
              testing::ElementsAre(&kProviderGlobal1, &kProviderNoDisplay,
                                   &kProviderFr, &kProviderGlobal2));
}

}  // namespace chrome_browser_net::secure_dns
