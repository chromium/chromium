// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_NETWORK_ACCESS_IP_ADDRESS_SPACE_OVERRIDES_PREFS_OBSERVER_H_
#define CHROME_BROWSER_LOCAL_NETWORK_ACCESS_IP_ADDRESS_SPACE_OVERRIDES_PREFS_OBSERVER_H_

#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace local_network_access {

// Helper for translating
// prefs::kManagedLocalNetworkAccessIPAddressSpaceOverrides (connected to the
// enterprise policy LocalNetworkAccessIPAddressSpaceOverrides) into
// network::IPAddressSpaceOverrides
class IPAddressSpaceOverridesPrefsObserver {
 public:
  explicit IPAddressSpaceOverridesPrefsObserver(PrefService* pref_service);

  IPAddressSpaceOverridesPrefsObserver(
      const IPAddressSpaceOverridesPrefsObserver&) = delete;
  IPAddressSpaceOverridesPrefsObserver& operator=(
      const IPAddressSpaceOverridesPrefsObserver&) = delete;

 private:
  void OnChangeInIPAddressSpaceOverridesPref();

  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace local_network_access

#endif  // CHROME_BROWSER_LOCAL_NETWORK_ACCESS_IP_ADDRESS_SPACE_OVERRIDES_PREFS_OBSERVER_H_
