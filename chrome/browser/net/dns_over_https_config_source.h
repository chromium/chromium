// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_OVER_HTTPS_CONFIG_SOURCE_H_
#define CHROME_BROWSER_NET_DNS_OVER_HTTPS_CONFIG_SOURCE_H_

#include <string>

#include "base/functional/callback_forward.h"

// Interface definition for specifying sources (e.g. preferences) for the
// DNS-over-HTTPS configuration.
class DnsOverHttpsConfigSource {
 public:
  virtual ~DnsOverHttpsConfigSource() = default;

  // Returns the DNS-over-HTTPS mode.
  virtual std::string GetDnsOverHttpsMode() const = 0;

  // Returns the DNS-over-HTTPS template URIs.
  virtual std::string GetDnsOverHttpsTemplates() const = 0;

  // Returns true is the DNS-over-HTTPS configuration is managed by enterprise
  // policy.
  virtual bool IsConfigManaged() const = 0;

  // Adds an observer that will be called when the DNS-over-HTTPS settings
  // change.
  virtual void SetDohChangeCallback(base::RepeatingClosure callback) = 0;
};

#endif  // CHROME_BROWSER_NET_DNS_OVER_HTTPS_CONFIG_SOURCE_H_
