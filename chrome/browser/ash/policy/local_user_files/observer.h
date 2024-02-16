// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_LOCAL_USER_FILES_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_LOCAL_USER_FILES_OBSERVER_H_

#include "components/prefs/pref_change_registrar.h"

namespace policy::local_user_files {

// Observer interface for LocalUserFilesEnabled policy changes.
class Observer {
 public:
  Observer();
  virtual ~Observer();

  // Called when the value of the LocalUserFilesEnabled policy changes.
  virtual void OnLocalUserFilesPolicyChanged() {}

 private:
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_LOCAL_USER_FILES_OBSERVER_H_
