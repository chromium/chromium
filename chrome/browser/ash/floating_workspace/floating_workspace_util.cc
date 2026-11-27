// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"

#include <initializer_list>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/constants/pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::floating_workspace_util {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(chromeos::prefs::kFloatingWorkspaceV2Enabled,
                                false);
}

bool IsFloatingWorkspaceEnabled(const PrefService& prefs) {
  // TODO(crbug.com/297795546): Temporary check both FloatingWorkspaceEnabled
  // and FloatingWorkspaceV2Enabled policies. The former was originally used to
  // control V2 behavior and there are testers who might still rely on it. The
  // latter will be used in the next round of testing. Before it starts, we let
  // any of the two policies enable the feature.
  for (const auto& pref_name : {policy::policy_prefs::kFloatingWorkspaceEnabled,
                                chromeos::prefs::kFloatingWorkspaceV2Enabled}) {
    const PrefService::Preference* floating_workspace_pref =
        prefs.FindPreference(pref_name);

    CHECK(floating_workspace_pref, base::NotFatalUntil::M161);

    if (floating_workspace_pref->IsManaged() && prefs.GetBoolean(pref_name)) {
      return true;
    }
  }
  return false;
}

bool IsFloatingSsoEnabled(Profile* profile) {
  if (!ash::features::IsFloatingSsoAllowed()) {
    return false;
  }
  ash::floating_sso::FloatingSsoService* floating_sso_service =
      ash::floating_sso::FloatingSsoServiceFactory::GetForProfile(profile);
  if (!floating_sso_service) {
    return false;
  }
  return floating_sso_service->IsFloatingSsoEnabled();
}

bool IsInternetConnected() {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  if (!nsh) {
    return false;
  }
  const NetworkState* state = nsh->DefaultNetwork();
  if (!state) {
    return false;
  }
  return state->IsOnline();
}

bool IsSafeMode() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kSafeMode);
}

bool ShouldHandleRestartRestore(const PrefService& prefs) {
  return IsFloatingWorkspaceEnabled(prefs) && !IsSafeMode();
}

}  // namespace ash::floating_workspace_util
