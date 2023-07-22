// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/early_prefs/early_prefs_export_service.h"

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

EarlyPrefsExportService::EarlyPrefsExportService(const base::FilePath& root_dir,
                                                 PrefService* user_prefs)
    : prefs_(user_prefs) {
  writer_ = std::make_unique<EarlyPrefsWriter>(
      root_dir, base::ThreadPool::CreateSequencedTaskRunner(
                    {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                     base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  // Watch for pref updates from webui settings and policy.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs_);

  // We need to check if about://flags differ for user, so that
  // we can restart session with correct flags before loading
  // profile.
  StoreAndTrackPref(flags_ui::prefs::kAboutFlagsEntries);
  // Some policies need to be enforced early in the login flow
  // to limit lifetime of authenticated authsession.
  StoreAndTrackPref(ash::prefs::kRecoveryFactorBehavior);
}

EarlyPrefsExportService::~EarlyPrefsExportService() = default;

void EarlyPrefsExportService::StoreAndTrackPref(const std::string& pref_name) {
  EarlyPrefsExportService::OnPrefChanged(pref_name);
  pref_change_registrar_->Add(
      pref_name, base::BindRepeating(&EarlyPrefsExportService::OnPrefChanged,
                                     base::Unretained(this)));
}

void EarlyPrefsExportService::OnPrefChanged(const std::string& pref_name) {
  auto* pref = prefs_->FindPreference(pref_name);
  if (!pref || pref->IsDefaultValue()) {
    writer_->ResetPref(pref_name);
    return;
  }
  if (pref->IsManaged()) {
    writer_->StorePolicy(pref_name, *pref->GetValue(), pref->IsRecommended());
  } else {
    writer_->StoreUserPref(pref_name, *pref->GetValue());
  }
}

void EarlyPrefsExportService::Shutdown() {
  writer_->CommitPendingWrites();
}

}  // namespace ash
