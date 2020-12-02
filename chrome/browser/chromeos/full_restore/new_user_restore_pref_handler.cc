// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/new_user_restore_pref_handler.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/full_restore/full_restore_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"

namespace chromeos {
namespace full_restore {

NewUserRestorePrefHanlder::NewUserRestorePrefHanlder(Profile* profile)
    : profile_(profile) {
  SetDefaultRestorePrefIfNecessary(profile_->GetPrefs());
  syncable_pref_observer_.Observe(PrefServiceSyncableFromProfile(profile));

  local_restore_pref_ = std::make_unique<IntegerPrefMember>();

  // base::Unretained() is safe because this class owns |local_restore_pref_|.
  local_restore_pref_->Init(
      kRestoreAppsAndPagesPrefName, profile_->GetPrefs(),
      base::Bind(&NewUserRestorePrefHanlder::OnPreferenceChanged,
                 base::Unretained(this)));
}

NewUserRestorePrefHanlder::~NewUserRestorePrefHanlder() = default;

void NewUserRestorePrefHanlder::OnIsSyncingChanged() {
  // Wait until the initial sync happens.
  auto* pref_service = PrefServiceSyncableFromProfile(profile_);
  bool is_syncing = chromeos::features::IsSplitSettingsSyncEnabled()
                        ? pref_service->AreOsPrefsSyncing()
                        : pref_service->IsSyncing();
  if (!is_syncing)
    return;

  // OnIsSyncingChanged could be called multiple times. We only check and modify
  // the restore pref for the first sync.
  syncable_pref_observer_.RemoveObservation();

  // If |kRestoreAppsAndPagesPrefName| is modified before the first sync, that
  // means |kRestoreAppsAndPagesPrefName| is modifyed from sync, or the user
  // has set |kRestoreAppsAndPagesPrefName|. Then we should keep it, and not
  // update it.
  if (is_restore_pref_changed_)
    return;

  // If |kRestoreAppsAndPagesPrefName| is not modified and still the default
  // setting done by SetDefaultRestorePrefIfNecessary, update based on the
  // synced browser restore settings.
  UpdateRestorePrefIfNecessary(profile_->GetPrefs());
}

void NewUserRestorePrefHanlder::OnPreferenceChanged(
    const std::string& pref_name) {
  is_restore_pref_changed_ = true;
  local_restore_pref_.reset();
}

}  // namespace full_restore
}  // namespace chromeos
