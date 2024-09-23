// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/update_engine_metrics_provider.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/user_manager/user_manager.h"

void UpdateEngineMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (IsConsumerAutoUpdateToggleEligible()) {
    PrefService* local_state = g_browser_process->local_state();
    UMA_HISTOGRAM_BOOLEAN("UpdateEngine.ConsumerAutoUpdate",
                          local_state && !local_state->GetBoolean(
                                             prefs::kConsumerAutoUpdateToggle));
  }
}

bool UpdateEngineMetricsProvider::IsConsumerAutoUpdateToggleEligible() {
  if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    return false;
  }

  const auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager || !user_manager->IsCurrentUserOwner()) {
    return false;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)
    return false;

  const std::string& gaia_id =
      user_manager->GetActiveUser()->GetAccountId().GetGaiaId();
  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id);
  return account_info.capabilities.can_toggle_auto_updates() ==
         signin::Tribool::kTrue;
}
