// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_PREF_STATE_OBSERVER_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_PREF_STATE_OBSERVER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

class PrefService;

namespace ash {

// Class to update NetworkHandler when the PrefService state changes.
class NetworkPrefStateObserver
    : public session_manager::SessionManagerObserver {
 public:
  explicit NetworkPrefStateObserver(PrefService& local_state);

  NetworkPrefStateObserver(const NetworkPrefStateObserver&) = delete;
  NetworkPrefStateObserver& operator=(const NetworkPrefStateObserver&) = delete;

  ~NetworkPrefStateObserver() override;

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;

 private:
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  const raw_ref<PrefService> local_state_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_PREF_STATE_OBSERVER_H_
