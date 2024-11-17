// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

LobsterSystemStateProvider::LobsterSystemStateProvider(Profile* profile)
    : profile_(profile) {}

LobsterSystemStateProvider::~LobsterSystemStateProvider() = default;

ash::LobsterSystemState LobsterSystemStateProvider::GetSystemState() {
  // TODO: crbug.com/348280621 - Organizes conditions into classes for
  // readability.
  return ash::LobsterSystemState(
      profile_->GetPrefs()->GetBoolean(ash::prefs::kLobsterEnabled)
          ? ash::LobsterStatus::kEnabled
          : ash::LobsterStatus::kBlocked,
      // TODO: crbug.com/348280621 - Populates the failed checks here with the
      // corresponding failed conditions.
      {});
}
