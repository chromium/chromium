// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_user_files_policy_observer.h"

#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "components/prefs/pref_service.h"

namespace policy::local_user_files {

LocalUserFilesPolicyObserver::LocalUserFilesPolicyObserver(
    PrefService* local_state)
    : local_state_(CHECK_DEREF(local_state)),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  pref_change_registrar_->Init(&local_state_.get());
  const base::RepeatingClosure cb = base::BindRepeating(
      &LocalUserFilesPolicyObserver::OnLocalUserFilesPolicyChanged,
      base::Unretained(this));
  pref_change_registrar_->Add(ash::prefs::kLocalUserFilesAllowed, cb);
  pref_change_registrar_->Add(ash::prefs::kLocalUserFilesMigrationDestination,
                              cb);
}

LocalUserFilesPolicyObserver::~LocalUserFilesPolicyObserver() {
  pref_change_registrar_->RemoveAll();
}

}  // namespace policy::local_user_files
