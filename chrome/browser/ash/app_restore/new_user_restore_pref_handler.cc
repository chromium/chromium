// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/new_user_restore_pref_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"

namespace ash::full_restore {

NewUserRestorePrefHandler::NewUserRestorePrefHandler(Profile* profile)
    : profile_(profile) {
  SetDefaultRestorePrefIfNecessary(profile_->GetPrefs());

  auto* pref_service = PrefServiceSyncableFromProfile(profile_);
  syncable_pref_observer_.Observe(pref_service);
  pref_service->AddSyncedPrefObserver(prefs::kRestoreAppsAndPagesPrefName,
                                      this);

  local_restore_pref_ = std::make_unique<IntegerPrefMember>();

  // base::Unretained() is safe because this class owns |local_restore_pref_|.
  local_restore_pref_->Init(
      prefs::kRestoreAppsAndPagesPrefName, profile_->GetPrefs(),
      base::BindRepeating(&NewUserRestorePrefHandler::OnPreferenceChanged,
                          base::Unretained(this)));
}

NewUserRestorePrefHandler::~NewUserRestorePrefHandler() {
  if (!is_restore_pref_synced_) {
    PrefServiceSyncableFromProfile(profile_)->RemoveSyncedPrefObserver(
        prefs::kRestoreAppsAndPagesPrefName, this);
  }
}

void NewUserRestorePrefHandler::OnStartedSyncing(std::string_view path) {
  is_restore_pref_synced_ = true;
  PrefServiceSyncableFromProfile(profile_)->RemoveSyncedPrefObserver(
      prefs::kRestoreAppsAndPagesPrefName, this);
}

void NewUserRestorePrefHandler::OnIsSyncingChanged() {
  // Wait until the initial sync happens.
  auto* pref_service = PrefServiceSyncableFromProfile(profile_);
  if (!pref_service->AreOsPrefsSyncing())
    return;

  // OnIsSyncingChanged could be called multiple times. We only check and modify
  // the restore pref for the first sync.
  DCHECK(syncable_pref_observer_.IsObserving());
  syncable_pref_observer_.Reset();

  // If `prefs::kRestoreAppsAndPagesPrefName` is modified before the first
  // sync, that means `prefs::kRestoreAppsAndPagesPrefName` is modified
  // from sync, or the user has set `prefs::kRestoreAppsAndPagesPrefName`.
  // Then we should keep it, and not update it.
  if (is_restore_pref_changed_ || is_restore_pref_synced_)
    return;

  // If `prefs::kRestoreAppsAndPagesPrefName` is not modified and still the
  // default setting done by SetDefaultRestorePrefIfNecessary, update based on
  // the synced browser restore settings.
  UpdateRestorePrefIfNecessary(profile_->GetPrefs());
}

void NewUserRestorePrefHandler::OnPreferenceChanged(
    const std::string& pref_name) {
  is_restore_pref_changed_ = true;
  local_restore_pref_.reset();
}

}  // namespace ash::full_restore
