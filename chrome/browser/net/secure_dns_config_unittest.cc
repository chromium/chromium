// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_config.h"

#include "chrome/browser/net/secure_dns_util.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SecureDnsConfig, ParseMode) {
  EXPECT_EQ(net::SecureDnsMode::kOff,
            SecureDnsConfig::ParseMode("off").value());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic,
            SecureDnsConfig::ParseMode("automatic").value());
  EXPECT_EQ(net::SecureDnsMode::kSecure,
            SecureDnsConfig::ParseMode("secure").value());

  EXPECT_FALSE(SecureDnsConfig::ParseMode("foo").has_value());
  EXPECT_FALSE(SecureDnsConfig::ParseMode("").has_value());
}

TEST(SecureDnsConfig, ModeToString) {
  EXPECT_EQ(std::string("off"),
            SecureDnsConfig::ModeToString(net::SecureDnsMode::kOff));
  EXPECT_EQ(std::string("automatic"),
            SecureDnsConfig::ModeToString(net::SecureDnsMode::kAutomatic));
  EXPECT_EQ(std::string("secure"),
            SecureDnsConfig::ModeToString(net::SecureDnsMode::kSecure));
}

TEST(SecureDnsConfig, Constructor) {
  net::DnsOverHttpsConfig doh_config = *net::DnsOverHttpsConfig::FromString(
      "https://template1 https://template2/{?dns}");
  std::vector<net::IPEndPoint> fallback_doh_nameservers = {net::IPEndPoint(
      net::IPAddress(1, 2, 3, 4), net::dns_protocol::kDefaultPort)};
  SecureDnsConfig config(
      net::SecureDnsMode::kSecure, doh_config,
      SecureDnsConfig::ManagementMode::kDisabledParentalControls,
      /*fallback_doh_nameservers=*/fallback_doh_nameservers);
  EXPECT_EQ(net::SecureDnsMode::kSecure, config.mode());
  EXPECT_EQ(doh_config, config.doh_servers());
  EXPECT_EQ(SecureDnsConfig::ManagementMode::kDisabledParentalControls,
            config.management_mode());
  EXPECT_EQ(fallback_doh_nameservers, config.fallback_doh_nameservers());
}
