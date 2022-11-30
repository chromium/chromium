// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_BLUETOOTH_PREF_STATE_OBSERVER_H_
#define CHROME_BROWSER_ASH_NET_BLUETOOTH_PREF_STATE_OBSERVER_H_

#include "base/scoped_observation.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

class Profile;

namespace ash {

// Class to update CrosBluetoothConfig when the PrefService state changes. The
// implementation currently relies on g_browser_process since it holds the
// default PrefService.
class BluetoothPrefStateObserver
    : public session_manager::SessionManagerObserver {
 public:
  BluetoothPrefStateObserver();

  BluetoothPrefStateObserver(const BluetoothPrefStateObserver&) = delete;
  BluetoothPrefStateObserver& operator=(const BluetoothPrefStateObserver&) =
      delete;

  ~BluetoothPrefStateObserver() override;

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;

 private:
  void SetPrefs(Profile* profile);

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_BLUETOOTH_PREF_STATE_OBSERVER_H_
