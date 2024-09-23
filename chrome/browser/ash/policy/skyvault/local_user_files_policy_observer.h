// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_USER_FILES_POLICY_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_USER_FILES_POLICY_OBSERVER_H_

#include "components/prefs/pref_change_registrar.h"

namespace policy::local_user_files {

// LocalUserFilesAllowed and LocalUserFilesMigrationDestination policies
// observer interface.
class LocalUserFilesPolicyObserver {
 public:
  LocalUserFilesPolicyObserver();
  virtual ~LocalUserFilesPolicyObserver();

  // Called when the value of the observed policy changes.
  virtual void OnLocalUserFilesPolicyChanged() {}

 private:
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_USER_FILES_POLICY_OBSERVER_H_
