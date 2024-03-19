// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_config.h"

#include <string_view>

// static
constexpr char SecureDnsConfig::kModeOff[];
constexpr char SecureDnsConfig::kModeAutomatic[];
constexpr char SecureDnsConfig::kModeSecure[];

SecureDnsConfig::SecureDnsConfig(net::SecureDnsMode mode,
                                 net::DnsOverHttpsConfig doh_servers,
                                 ManagementMode management_mode)
    : mode_(mode),
      doh_servers_(std::move(doh_servers)),
      management_mode_(management_mode) {}
SecureDnsConfig::SecureDnsConfig(SecureDnsConfig&& other) = default;
SecureDnsConfig& SecureDnsConfig::operator=(SecureDnsConfig&& other) = default;
SecureDnsConfig::~SecureDnsConfig() = default;

// static
std::optional<net::SecureDnsMode> SecureDnsConfig::ParseMode(
    std::string_view name) {
  if (name == kModeSecure) {
    return net::SecureDnsMode::kSecure;
  } else if (name == kModeAutomatic) {
    return net::SecureDnsMode::kAutomatic;
  } else if (name == kModeOff) {
    return net::SecureDnsMode::kOff;
  }
  return std::nullopt;
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
