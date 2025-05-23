// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"

#include <initializer_list>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash::floating_workspace_util {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFloatingWorkspaceV2Enabled, false);
}

bool IsFloatingWorkspaceV1Enabled() {
  return features::IsFloatingWorkspaceEnabled();
}

bool IsFloatingWorkspaceV2Enabled() {
  auto* active_user = user_manager::UserManager::Get()->GetActiveUser();
  if (!active_user) {
    return false;
  }
  auto* browser_context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user);
  if (!browser_context) {
    return false;
  }

  return IsFloatingWorkspaceEnabled(
      Profile::FromBrowserContext(browser_context));
}

bool IsFloatingWorkspaceEnabled(const Profile* profile) {
  const PrefService* pref_service = profile->GetPrefs();
  if (!pref_service) {
    return false;
  }

  // TODO(crbug.com/297795546): Temporary check both FloatingWorkspaceEnabled
  // and FloatingWorkspaceV2Enabled policies. The former was originally used to
  // control V2 behavior and there are testers who might still rely on it. The
  // latter will be used in the next round of testing. Before it starts, we let
  // any of the two policies enable the feature.
  for (const auto& pref_name : {policy::policy_prefs::kFloatingWorkspaceEnabled,
                                prefs::kFloatingWorkspaceV2Enabled}) {
    const PrefService::Preference* floating_workspace_pref =
        pref_service->FindPreference(pref_name);

    DCHECK(floating_workspace_pref);

    if (floating_workspace_pref->IsManaged() &&
        pref_service->GetBoolean(pref_name)) {
      return true;
    }
  }

  // TODO(crbug.com/297795546): Remove external ash feature flag.
  return features::IsFloatingWorkspaceV2Enabled();
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

bool ShouldHandleRestartRestore() {
  return IsFloatingWorkspaceV2Enabled() && !IsSafeMode();
}

}  // namespace ash::floating_workspace_util
