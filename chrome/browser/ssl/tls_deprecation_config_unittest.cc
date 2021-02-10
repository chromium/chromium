// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/tls_deprecation_config.h"

#include <memory>
#include <string>

#include "chrome/browser/ssl/tls_deprecation_test_utils.h"
#include "services/network/public/proto/tls_deprecation_config.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using chrome_browser_ssl::LegacyTLSExperimentConfig;

class TLSDeprecationConfigTest : public testing::Test {
 protected:
  void SetUp() override { ResetTLSDeprecationConfigForTesting(); }
  void TearDown() override { ResetTLSDeprecationConfigForTesting(); }
};

// Tests the case where no proto has been set by the component installer.
TEST_F(TLSDeprecationConfigTest, NoProto) {
  EXPECT_TRUE(ShouldSuppressLegacyTLSWarning(GURL("https://example.test")));
}

// This tests that when no sites are in the control set,
// IsTLSDeprecationControlSite() returns false.
TEST_F(TLSDeprecationConfigTest, NoControlSites) {
  GURL control_site("https://control.test");
  std::string control_site_hex =
      "f12b47771bb3c2bcc85a5347d195523013ec5a23b4c761b5d6aacf04bafc5e23";

  // Setup proto (as if read from component installer), but don't add any
  // control sites to it.
  auto config = std::make_unique<LegacyTLSExperimentConfig>();
  config->set_version_id(1);
  SetRemoteTLSDeprecationConfig(config->SerializeAsString());

  EXPECT_FALSE(ShouldSuppressLegacyTLSWarning(control_site));
}

// This tests that when only a single control site is in the control set,
// IsTLSDeprecationControlSite() works correctly for the site in the control and
// for sites not in the control.
TEST_F(TLSDeprecationConfigTest, SingleControlSite) {
  GURL control_site("https://control.test");
  std::string control_site_hex =
      "f12b47771bb3c2bcc85a5347d195523013ec5a23b4c761b5d6aacf04bafc5e23";
  GURL non_control_site("https://not-control.test");
  std::string non_control_site_hex =
      "a11de9f014acb9e53c8997d9c48b10ed23c8b2fa7e790b125ea5fb78af5359cb";
  GURL http_site("http://noncryptographic.test");
  std::string http_site_hex =
      "fd288ddecdac673aedb343e41117443f1ce88114dcc7bfa3c4a65a1e725d8fbe";

  // Setup proto (as if read from component installer).
  auto config = std::make_unique<LegacyTLSExperimentConfig>();
  config->set_version_id(1);
  config->add_control_site_hashes(control_site_hex);

  SetRemoteTLSDeprecationConfig(config->SerializeAsString());

  EXPECT_TRUE(ShouldSuppressLegacyTLSWarning(control_site));
  EXPECT_FALSE(ShouldSuppressLegacyTLSWarning(non_control_site));

  // And HTTP sites should not count either.
  EXPECT_FALSE(ShouldSuppressLegacyTLSWarning(http_site));
}

// This tests that the binary search in IsTLSDeprecationControlSite() works for
// both a site that is in the control set and a site that is not.
TEST_F(TLSDeprecationConfigTest, ManyControlSites) {
  const struct {
    GURL url;
    std::string hash;
  } kControlSites[] = {
      // These must be in alphanumeric (0-9a-z) order by their hashes.
      {GURL("https://control6.test"),
       "27ce26e09cae16b1bd1914678d82e98326362f4a06b1b3e81c8ea96018be3b08"},
      {GURL("https://control9.test"),
       "2904bff8d79d14e7b90d7cb499ce0bcd619f76818d9512f4c855d2ab738f42e8"},
      {GURL("https://control2.test"),
       "4c37847b9688f807d4853ac9fefca228decbc3bb3851be9f75a8898b5f483435"},
      {GURL("https://control4.test"),
       "57e66a432b7661a426f4e9f470f7f5ff259870931277ac4e00853d3a237895f1"},
      {GURL("https://control0.test"),
       "7653aed3ed0e08c4abd4035e6f24ad1b4705387bffed42c5955abdcabda5b2fc"},
      {GURL("https://control5.test"),
       "8cca9122cb6f459552422aef9536daf9fce3a4648729fe2ab17bc63a5c8cd9d9"},
      {GURL("https://control7.test"),
       "8d9ff576c2e4a834dd75b1a0b2acbc0b3ce89035feb1c1645e96d24e803a08a5"},
      {GURL("https://control1.test"),
       "d4a33fe8bbb13fce2300bd1788b2df1722605f8f30bc1119c8ca3f8aea32e1e5"},
      {GURL("https://control8.test"),
       "e161ddfb174571df3db808e18f8177c9bbfdb545108616bd62434280b5ca2b37"},
      {GURL("https://control3.test"),
       "f8bb464ff13462fde12aac03f9e66f3d37a3b9359992fca01428480bd1418e02"}};
  GURL non_control_site("https://example-not-control.test");

  // Setup proto (as if read from component installer).
  auto config = std::make_unique<LegacyTLSExperimentConfig>();
  config->set_version_id(1);
  for (auto& site : kControlSites) {
    config->add_control_site_hashes(site.hash);
  }
  SetRemoteTLSDeprecationConfig(config->SerializeAsString());

  for (auto& site : kControlSites) {
    EXPECT_TRUE(ShouldSuppressLegacyTLSWarning(site.url));
  }

  EXPECT_FALSE(ShouldSuppressLegacyTLSWarning(non_control_site));
}
