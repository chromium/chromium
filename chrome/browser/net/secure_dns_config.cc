// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_config.h"

// static
constexpr char SecureDnsConfig::kModeOff[];
constexpr char SecureDnsConfig::kModeAutomatic[];
constexpr char SecureDnsConfig::kModeSecure[];

SecureDnsConfig::SecureDnsConfig(
    net::SecureDnsMode mode,
    std::vector<net::DnsOverHttpsServerConfig> servers,
    ManagementMode management_mode)
    : mode_(mode),
      servers_(std::move(servers)),
      management_mode_(management_mode) {}
SecureDnsConfig::SecureDnsConfig(SecureDnsConfig&& other) = default;
SecureDnsConfig& SecureDnsConfig::operator=(SecureDnsConfig&& other) = default;
SecureDnsConfig::~SecureDnsConfig() = default;

// static
base::Optional<net::SecureDnsMode> SecureDnsConfig::ParseMode(
    base::StringPiece name) {
  if (name == kModeSecure) {
    return net::SecureDnsMode::kSecure;
  } else if (name == kModeAutomatic) {
    return net::SecureDnsMode::kAutomatic;
  } else if (name == kModeOff) {
    return net::SecureDnsMode::kOff;
  }
  return base::nullopt;
}

// static
const char* SecureDnsConfig::ModeToString(net::SecureDnsMode mode) {
  switch (mode) {
    case net::SecureDnsMode::kSecure:
      return kModeSecure;
    case net::SecureDnsMode::kAutomatic:
      return kModeAutomatic;
    case net::SecureDnsMode::kOff:
      return kModeOff;
  }
}
