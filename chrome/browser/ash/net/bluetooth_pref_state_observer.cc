// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/bluetooth_pref_state_observer.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/bluetooth_config/in_process_instance.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

BluetoothPrefStateObserver::BluetoothPrefStateObserver() {
  // Set CrosBluetoothConfig with device prefs only.
  SetPrefs(/*profile=*/nullptr);
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
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  DCHECK(profile);

  // Only set the prefs for primary users.
  if (!ProfileHelper::IsPrimaryProfile(profile)) {
    BLUETOOTH_LOG(EVENT)
        << "User profile loaded, but user is not primary. Not setting "
        << "CrosBluetoothConfig with profile prefs service";
    return;
  }

  BLUETOOTH_LOG(EVENT) << "Primary profile loaded, setting CrosBluetoothConfig "
                       << "with profile prefs service";
  SetPrefs(profile);
  session_observation_.Reset();
}

void BluetoothPrefStateObserver::SetPrefs(Profile* profile) {
  DCHECK(g_browser_process);
  bluetooth_config::SetPrefs(profile ? profile->GetPrefs() : nullptr,
                             g_browser_process->local_state());
}

}  // namespace ash
