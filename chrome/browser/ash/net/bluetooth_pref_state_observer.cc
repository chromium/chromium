// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/bluetooth_pref_state_observer.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/bluetooth_config/in_process_instance.h"

namespace chromeos {

BluetoothPrefStateObserver::BluetoothPrefStateObserver() {
  CHECK(ash::features::IsBluetoothRevampEnabled());

  // Set CrosBluetoothConfig with device prefs only.
  SetPrefs(/*profile=*/nullptr);
  session_observation_.Observe(session_manager::SessionManager::Get());
}

BluetoothPrefStateObserver::~BluetoothPrefStateObserver() = default;

void BluetoothPrefStateObserver::OnUserProfileLoaded(
    const AccountId& account_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  DCHECK(profile);

  // Only set the prefs for primary users.
  if (!ProfileHelper::IsPrimaryProfile(profile))
    return;

  SetPrefs(profile);
}

void BluetoothPrefStateObserver::SetPrefs(Profile* profile) {
  DCHECK(g_browser_process);
  chromeos::bluetooth_config::SetPrefs(profile ? profile->GetPrefs() : nullptr,
                                       g_browser_process->local_state());
}

}  // namespace chromeos
