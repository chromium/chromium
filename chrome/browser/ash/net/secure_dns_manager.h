// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_SECURE_DNS_MANAGER_H_
#define CHROME_BROWSER_ASH_NET_SECURE_DNS_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace net {

// Responds to changes in the SecureDNS preferences and generates and updates
// the corresponding shill property which can then be used by downstream
// services.
class SecureDnsManager {
 public:
  explicit SecureDnsManager(PrefService* pref_service);
  SecureDnsManager(const SecureDnsManager&) = delete;
  SecureDnsManager& operator=(const SecureDnsManager&) = delete;
  ~SecureDnsManager();

 private:
  // Retrieves the list of secure DNS providers, preprocesses and caches it for
  // later use. This is safe since the list is embedded in code and will not
  // change at runtime.
  void LoadProviders();

  // Computes a collection of secure DNS providers to use based on the |mode|
  // and |templates| prefs applied to |local_doh_providers_|.
  base::Value GetProviders(const std::string& mode,
                           const std::string& templates);

  // Callback for the registrar. Evaluates the current settings and publishes
  // the result to shill.
  void OnPrefChanged();

  PrefChangeRegistrar registrar_;

  // Maps secure DNS provider URL templates to their corresponding standard DNS
  // name servers. Providers that are either disabled or not applicable for the
  // country have been pre-filtered.
  base::flat_map<std::string, std::string> local_doh_providers_;
};

}  // namespace net

#endif  // CHROME_BROWSER_ASH_NET_SECURE_DNS_MANAGER_H_
