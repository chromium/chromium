// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_ASH_DNS_OVER_HTTPS_CONFIG_SOURCE_H_
#define CHROME_BROWSER_ASH_NET_ASH_DNS_OVER_HTTPS_CONFIG_SOURCE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/net/secure_dns_manager.h"
#include "chrome/browser/net/dns_over_https_config_source.h"

namespace ash {

// DnsOverHttpsConfigSource implementation that gets the DNS-over-HTTPS
// configuration from the SecureDnsManager.
class AshDnsOverHttpsConfigSource : public DnsOverHttpsConfigSource,
                                    public SecureDnsManager::Observer {
 public:
  // The instance of `AshDnsOverHttpsConfigSource` should not outlive
  // `secure_dns_manager`.
  AshDnsOverHttpsConfigSource(SecureDnsManager* secure_dns_manager,
                              PrefService* local_state);

  AshDnsOverHttpsConfigSource(const AshDnsOverHttpsConfigSource&) = delete;
  AshDnsOverHttpsConfigSource& operator=(const AshDnsOverHttpsConfigSource&) =
      delete;
  ~AshDnsOverHttpsConfigSource() override;

  // DnsOverHttpsConfigSource:
  std::string GetDnsOverHttpsMode() const override;
  std::string GetDnsOverHttpsTemplates() const override;
  bool IsConfigManaged() const override;
  void SetDohChangeCallback(base::RepeatingClosure callback) override;

  // SecureDnsManager::Observer:
  void OnTemplateUrisChanged(const std::string& template_uris) override;
  void OnModeChanged(const std::string& mode) override;
  void OnSecureDnsManagerShutdown() override;

 private:
  std::string dns_over_https_templates_;
  std::string dns_over_https_mode_;
  raw_ptr<PrefService> local_state_;
  raw_ptr<SecureDnsManager> secure_dns_manager_;
  base::RepeatingClosure on_change_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_ASH_DNS_OVER_HTTPS_CONFIG_SOURCE_H_
