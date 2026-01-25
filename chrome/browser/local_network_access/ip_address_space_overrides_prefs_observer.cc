// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_network_access/ip_address_space_overrides_prefs_observer.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/ip_address_space_util.h"

namespace local_network_access {

IPAddressSpaceOverridesPrefsObserver::IPAddressSpaceOverridesPrefsObserver(
    PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);

  // Start listening for pref change notifications.
  //
  // base::Unretained is okay below, because |pref_change_registrar_|'s lifetime
  // is owned by (and shorter than) |this|.
  pref_change_registrar_.Add(
      prefs::kManagedLocalNetworkAccessIpAddressSpaceOverrides,
      base::BindRepeating(&IPAddressSpaceOverridesPrefsObserver::
                              OnChangeInIPAddressSpaceOverridesPref,
                          base::Unretained(this)));

  // Make sure that not only *future* changes of prefs are applied, but that
  // also the *current* state of prefs is applied.
  OnChangeInIPAddressSpaceOverridesPref();
}

void IPAddressSpaceOverridesPrefsObserver::
    OnChangeInIPAddressSpaceOverridesPref() {
  std::vector<std::string> pref_overrides;
  if (pref_change_registrar_.prefs()->HasPrefPath(
          prefs::kManagedLocalNetworkAccessIpAddressSpaceOverrides)) {
    const base::ListValue& pref_list = pref_change_registrar_.prefs()->GetList(
        prefs::kManagedLocalNetworkAccessIpAddressSpaceOverrides);
    for (const auto& pref_value : pref_list) {
      pref_overrides.push_back(pref_value.GetString());
    }
  }

  std::vector<std::string> rejected_overrides;
  std::string pref_value = base::JoinString(pref_overrides, ",");
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      pref_value, &rejected_overrides);

  if (!rejected_overrides.empty()) {
    SYSLOG(ERROR)
        << "The '" << prefs::kManagedLocalNetworkAccessIpAddressSpaceOverrides
        << "' policy contained invalid values (they have been ignored): "
        << base::JoinString(rejected_overrides, ", ");
  }
}

}  // namespace local_network_access
