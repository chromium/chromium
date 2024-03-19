// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SECURE_DNS_CONFIG_H_
#define CHROME_BROWSER_NET_SECURE_DNS_CONFIG_H_

#include <optional>
#include <string_view>

#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/secure_dns_mode.h"

// Representation of a complete Secure DNS configuration.
class SecureDnsConfig {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.net
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: SecureDnsManagementMode
  // Forced management description types. We will check for the override cases
  // in the order they are listed in the enum.
  enum class ManagementMode {
    // Chrome did not override the secure DNS settings.
    kNoOverride,
    // Secure DNS was disabled due to detection of a managed environment.
    kDisabledManaged,
    // Secure DNS was disabled due to detection of OS-level parental controls.
    kDisabledParentalControls,
  };

  // String representations for net::SecureDnsMode.  Used for both configuration
  // storage and UI state.
  static constexpr char kModeOff[] = "off";
  static constexpr char kModeAutomatic[] = "automatic";
  static constexpr char kModeSecure[] = "secure";

  SecureDnsConfig(net::SecureDnsMode mode,
                  net::DnsOverHttpsConfig doh_config,
                  ManagementMode management_mode);
  // This class is move-only to avoid any accidental copying.
  SecureDnsConfig(SecureDnsConfig&& other);
  SecureDnsConfig& operator=(SecureDnsConfig&& other);
  ~SecureDnsConfig();

  // Identifies the SecureDnsMode corresponding to one of the above names, or
  // returns nullopt if the name is unrecognized.
  static std::optional<net::SecureDnsMode> ParseMode(std::string_view name);
  // Converts a secure DNS mode to one of the above names.
  static const char* ModeToString(net::SecureDnsMode mode);

  net::SecureDnsMode mode() { return mode_; }
  const net::DnsOverHttpsConfig& doh_servers() { return doh_servers_; }
  ManagementMode management_mode() { return management_mode_; }

 private:
  net::SecureDnsMode mode_;
  net::DnsOverHttpsConfig doh_servers_;
  ManagementMode management_mode_;
};

#endif  // CHROME_BROWSER_NET_SECURE_DNS_CONFIG_H_
