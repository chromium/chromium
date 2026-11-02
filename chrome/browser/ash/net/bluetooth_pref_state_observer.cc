// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/bluetooth_pref_state_observer.h"

#include "base/check.h"
#include "chromeos/ash/services/bluetooth_config/in_process_instance.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

BluetoothPrefStateObserver::BluetoothPrefStateObserver(PrefService& local_state)
    : local_state_(local_state) {
  // Set CrosBluetoothConfig with device prefs only.
  bluetooth_config::SetPrefs(/*logged_in_profile_prefs=*/nullptr, &local_state);
  session_observation_.Observe(session_manager::SessionManager::Get());
}

BluetoothPrefStateObserver::~BluetoothPrefStateObserver() {
  // Reset the pref services. We are explicitly setting device_prefs to null
  // instead of calling SetPrefs(nullptr) to ensure g_browser_process's
  // local_state isn't used, in case it's in a bad state. See
  // https://crbug.com/1261338.
  bluetooth_config::SetPrefs(/*logged_in_profile_prefs=*/nullptr,
                             /*device_prefs=*/nullptr);
}

void BluetoothPrefStateObserver::OnUserProfileLoaded(
    const AccountId& account_id) {
  // Only set the prefs for primary users.
  const session_manager::Session* const primary_session =
      session_manager::SessionManager::Get()->GetPrimarySession();
  if (!primary_session || primary_session->account_id() != account_id) {
    BLUETOOTH_LOG(EVENT)
        << "User profile loaded, but user is not primary. Not setting "
        << "CrosBluetoothConfig with profile prefs service";
    return;
  }

  // The Profile has been created by now, so its prefs are attached to the User.
  PrefService* const profile_prefs = user_manager::UserManager::Get()
                                         ->FindUserAndModify(account_id)
                                         ->GetProfilePrefs();
  CHECK(profile_prefs);

  BLUETOOTH_LOG(EVENT) << "Primary profile loaded, setting CrosBluetoothConfig "
                       << "with profile prefs service";
  bluetooth_config::SetPrefs(profile_prefs, &local_state_.get());
  session_observation_.Reset();
}

}  // namespace ash
