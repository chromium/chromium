// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_USER_FILES_POLICY_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_USER_FILES_POLICY_OBSERVER_H_

#include "base/memory/raw_ref.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace policy::local_user_files {

// LocalUserFilesAllowed and LocalUserFilesMigrationDestination policies
// observer interface.
class LocalUserFilesPolicyObserver {
 public:
  // `local_state` must be non-null and outlive `this`.
  explicit LocalUserFilesPolicyObserver(PrefService* local_state);
  LocalUserFilesPolicyObserver(const LocalUserFilesPolicyObserver&) = delete;
  LocalUserFilesPolicyObserver& operator=(const LocalUserFilesPolicyObserver&) =
      delete;
  virtual ~LocalUserFilesPolicyObserver();

  // Called when the value of the observed policy changes.
  virtual void OnLocalUserFilesPolicyChanged() {}

 protected:
  const raw_ref<PrefService> local_state_;

 private:
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_USER_FILES_POLICY_OBSERVER_H_
