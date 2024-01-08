// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_THROTTLING_OBSERVER_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_THROTTLING_OBSERVER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;

namespace ash {

// NetworkThrottlingObserver is a singleton, owned by
// `ChromeBrowserMainPartsAsh`.
// This class is responsible for propagating network bandwidth throttling policy
// changes (prefs::kNetworkThrottlingEnabled) in Chrome down to Shill which
// implements by calling 'tc' in the kernel.
class NetworkThrottlingObserver {
 public:
  explicit NetworkThrottlingObserver(PrefService* local_state);

  NetworkThrottlingObserver(const NetworkThrottlingObserver&) = delete;
  NetworkThrottlingObserver& operator=(const NetworkThrottlingObserver&) =
      delete;

  ~NetworkThrottlingObserver();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Callback used when prefs::kNetworkThrottlingEnabled changes
  void OnPreferenceChanged(const std::string& pref_name);

  raw_ptr<PrefService> local_state_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_THROTTLING_OBSERVER_H_
