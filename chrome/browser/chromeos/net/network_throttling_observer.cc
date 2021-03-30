// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_throttling_observer.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

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

  const base::DictionaryValue* throttling_policy =
      local_state_->GetDictionary(prefs::kNetworkThrottlingEnabled);

  // Default is to disable throttling if the policy is not found.
  bool enabled = false;
  uint32_t upload_rate = 0, download_rate = 0;
  if (throttling_policy) {
    int upload_rate_read = 0;
    int download_rate_read = 0;

    throttling_policy->GetBoolean("enabled", &enabled);

    if (throttling_policy->GetInteger("upload_rate_kbits", &upload_rate_read) &&
        upload_rate_read > 0) {
      upload_rate = upload_rate_read;
    }

    if (throttling_policy->GetInteger("download_rate_kbits",
                                      &download_rate_read) &&
        download_rate_read > 0) {
      download_rate = download_rate_read;
    }
  }
  NetworkHandler::Get()->network_state_handler()->SetNetworkThrottlingStatus(
      enabled, upload_rate, download_rate);
}

}  // namespace chromeos
