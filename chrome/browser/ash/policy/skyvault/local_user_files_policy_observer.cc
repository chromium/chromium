// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_user_files_policy_observer.h"

#include "base/check_is_test.h"
#include "base/functional/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"

namespace policy::local_user_files {

LocalUserFilesPolicyObserver::LocalUserFilesPolicyObserver()
    : pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  if (!g_browser_process->local_state()) {
    // Can be NULL in tests.
    CHECK_IS_TEST();
    return;
  }
  pref_change_registrar_->Init(g_browser_process->local_state());
  const base::RepeatingClosure cb = base::BindRepeating(
      &LocalUserFilesPolicyObserver::OnLocalUserFilesPolicyChanged,
      base::Unretained(this));
  pref_change_registrar_->Add(prefs::kLocalUserFilesAllowed, cb);
  pref_change_registrar_->Add(prefs::kLocalUserFilesMigrationDestination, cb);
}

LocalUserFilesPolicyObserver::~LocalUserFilesPolicyObserver() {
  pref_change_registrar_->RemoveAll();
}

}  // namespace policy::local_user_files
