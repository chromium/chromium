// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PREF_STATE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PREF_STATE_OBSERVER_H_

#include "base/macros.h"
#include "base/scoped_observation.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

class Profile;

namespace chromeos {

// Class to update NetworkHandler when the PrefService state changes. The
// implementation currently relies on g_browser_process since it holds the
// default PrefService.
class NetworkPrefStateObserver
    : public session_manager::SessionManagerObserver {
 public:
  NetworkPrefStateObserver();
  ~NetworkPrefStateObserver() override;

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;

 private:
  void InitializeNetworkPrefServices(Profile* profile);

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkPrefStateObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PREF_STATE_OBSERVER_H_
