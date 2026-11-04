// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/update_engine_metrics_provider.h"

#include <map>

#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/signin/identity_manager_provider.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_id.h"

UpdateEngineMetricsProvider::UpdateEngineMetricsProvider(
    session_manager::SessionManager* session_manager)
    : session_manager_(session_manager) {
  if (session_manager_) {
    session_manager_observation_.Observe(session_manager_);
  } else {
    CHECK_IS_TEST();
  }
}

UpdateEngineMetricsProvider::~UpdateEngineMetricsProvider() = default;

void UpdateEngineMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (IsConsumerAutoUpdateToggleEligible()) {
    PrefService* local_state = g_browser_process->local_state();
    UMA_HISTOGRAM_BOOLEAN(
        "UpdateEngine.ConsumerAutoUpdate",
        local_state &&
            !local_state->GetBoolean(ash::prefs::kConsumerAutoUpdateToggle));
  }
}

void UpdateEngineMetricsProvider::OnUserProfileLoaded(
    const AccountId& account_id) {
  if (signin::IdentityManager* identity_manager =
          ash::IdentityManagerProvider::Get().Find(account_id)) {
    auto [_, inserted] =
        identity_managers_.emplace(account_id, identity_manager);
    CHECK(inserted);
    identity_manager_observations_.AddObservation(identity_manager);
  }
}

void UpdateEngineMetricsProvider::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observations_.RemoveObservation(identity_manager);
  std::erase_if(identity_managers_, [identity_manager](const auto& entry) {
    return entry.second == identity_manager;
  });
}

bool UpdateEngineMetricsProvider::IsConsumerAutoUpdateToggleEligible() {
  if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    return false;
  }

  const auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager || !user_manager->IsCurrentUserOwner()) {
    return false;
  }

  if (!session_manager_) {
    CHECK_IS_TEST();
    return false;
  }

  // TODO(crbug.com/278643115): Take the account_id from the callers.
  const session_manager::Session* active_session =
      session_manager_->GetActiveSession();
  CHECK(active_session);
  const AccountId& account_id = active_session->account_id();
  auto it = identity_managers_.find(account_id);
  if (it == identity_managers_.end()) {
    return false;
  }

  const AccountInfo account_info =
      it->second->FindExtendedAccountInfoByGaiaId(account_id.GetGaiaId());
  return account_info.GetAccountCapabilities().can_toggle_auto_updates() ==
         signin::Tribool::kTrue;
}
