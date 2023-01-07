// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_throttling_observer.h"

#include <algorithm>
#include <string>

#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

NetworkThrottlingObserver::NetworkThrottlingObserver(PrefService* local_state)
    : local_state_(local_state) {
  pref_change_registrar_.Init(local_state_);

  auto throttle_callback = base::BindRepeating(
      &NetworkThrottlingObserver::OnPreferenceChanged, base::Unretained(this));

  pref_change_registrar_.Add(prefs::kNetworkThrottlingEnabled,
                             throttle_callback);
}

NetworkThrottlingObserver::~NetworkThrottlingObserver() {}

void NetworkThrottlingObserver::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNetworkThrottlingEnabled);
}

void NetworkThrottlingObserver::OnPreferenceChanged(
    const std::string& pref_name) {
  DCHECK(pref_name == prefs::kNetworkThrottlingEnabled);

  const base::Value::Dict& throttling_policy =
      local_state_->GetDict(prefs::kNetworkThrottlingEnabled);

  // Default is to disable throttling if the policy is not found.
  const bool enabled = throttling_policy.FindBool("enabled").value_or(false);
  const uint32_t upload_rate =
      std::max(0, throttling_policy.FindInt("upload_rate_kbits").value_or(0));
  const uint32_t download_rate =
      std::max(0, throttling_policy.FindInt("download_rate_kbits").value_or(0));

  NetworkHandler::Get()->network_state_handler()->SetNetworkThrottlingStatus(
      enabled, upload_rate, download_rate);
}

}  // namespace ash
