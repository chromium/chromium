// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"
#include "components/prefs/pref_service.h"

namespace {

ash::LobsterConsentStatus GetConsentStatusFromInteger(int status_value) {
  switch (status_value) {
    case base::to_underlying(
        chromeos::editor_menu::EditorConsentStatus::kUnset):
    case base::to_underlying(
        chromeos::editor_menu::EditorConsentStatus::kPending):
      return ash::LobsterConsentStatus::kUnset;
    case base::to_underlying(
        chromeos::editor_menu::EditorConsentStatus::kApproved):
      return ash::LobsterConsentStatus::kApproved;
    case base::to_underlying(
        chromeos::editor_menu::EditorConsentStatus::kDeclined):
      return ash::LobsterConsentStatus::kDeclined;
    default:
      LOG(ERROR) << "Invalid consent status: " << status_value;
      // For any of the invalid states, treat the consent status as unset.
      return ash::LobsterConsentStatus::kUnset;
  }
}

}  // namespace

LobsterSystemStateProvider::LobsterSystemStateProvider(Profile* profile)
    : profile_(profile) {}

LobsterSystemStateProvider::~LobsterSystemStateProvider() = default;

ash::LobsterSystemState LobsterSystemStateProvider::GetSystemState() {
  // TODO: crbug.com/348280621 - Organizes conditions into classes for
  // readability.
  switch (GetConsentStatusFromInteger(
      profile_->GetPrefs()->GetInteger(ash::prefs::kOrcaConsentStatus))) {
    case ash::LobsterConsentStatus::kUnset:
      return ash::LobsterSystemState(ash::LobsterStatus::kConsentNeeded, {});
    case ash::LobsterConsentStatus::kDeclined:
      return ash::LobsterSystemState(
          ash::LobsterStatus::kBlocked,
          /*failed_checks=*/{ash::LobsterSystemCheck::kInvalidConsent});
    case ash::LobsterConsentStatus::kApproved:
      return ash::LobsterSystemState(
          profile_->GetPrefs()->GetBoolean(ash::prefs::kLobsterEnabled)
              ? ash::LobsterStatus::kEnabled
              : ash::LobsterStatus::kBlocked,
          // TODO: crbug.com/348280621 - Populates the failed checks here with
          // the corresponding failed conditions.
          /*failed_checks=*/{});
  }
}
