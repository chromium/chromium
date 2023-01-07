// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/lacros_data_backward_migration_mode_policy_observer.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"

namespace crosapi {

LacrosDataBackwardMigrationModePolicyObserver::
    LacrosDataBackwardMigrationModePolicyObserver()
    : profile_manager_(g_browser_process->profile_manager()),
      local_state_(g_browser_process->local_state()) {
  DCHECK(profile_manager_);
  DCHECK(local_state_);
  profile_manager_->AddObserver(this);
}

LacrosDataBackwardMigrationModePolicyObserver::
    ~LacrosDataBackwardMigrationModePolicyObserver() {
  profile_manager_->RemoveObserver(this);
}

void LacrosDataBackwardMigrationModePolicyObserver::OnProfileAdded(
    Profile* profile) {
  // Skip handling browser_tests.
  // The main target of this class is to send a message to session_manager
  // to preserve the lacros-backward-data-migration-policy policy. browser_tests
  // is out of scope, and actually there will be multiple initialization of
  // "Primary" Profile in a process, so it won't work as designed.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBrowserTest)) {
    return;
  }
  if (!ash::ProfileHelper::IsPrimaryProfile(profile) ||
      profile->IsOffTheRecord()) {
    return;
  }

  // Start observing local_state since here. Even if we observe the local_state
  // change (and actually it can happen though), we cannot send it to
  // session_manager, because we have to send it with primary profile's feature
  // flags together.
  DCHECK(!primary_profile_);
  primary_profile_ = profile;
  pref_change_registrar_.Init(local_state_);
  pref_change_registrar_.Add(
      prefs::kLacrosDataBackwardMigrationMode,
      base::BindRepeating(
          &LacrosDataBackwardMigrationModePolicyObserver::OnChanged,
          weak_factory_.GetWeakPtr()));
  // And so, the value may be set earlier (on creation of profile), so invoke
  // the callback synchronously here, if necessary.
  if (local_state_->IsManagedPreference(
          prefs::kLacrosDataBackwardMigrationMode))
    OnChanged();
}

void LacrosDataBackwardMigrationModePolicyObserver::OnChanged() {
  // Notify session_manager. The actual value for the
  // kLacrosDataBackwardMigrationMode is calculated in FeatureFlagsUpdate.
  DCHECK(primary_profile_);
  PrefService* prefs = primary_profile_->GetPrefs();
  flags_ui::PrefServiceFlagsStorage flags_storage(prefs);
  ash::about_flags::FeatureFlagsUpdate update(flags_storage, prefs);
  update.UpdateSessionManager();
}

}  // namespace crosapi
