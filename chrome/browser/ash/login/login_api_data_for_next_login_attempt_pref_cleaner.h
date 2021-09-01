// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOGIN_API_DATA_FOR_NEXT_LOGIN_ATTEMPT_PREF_CLEANER_H_
#define CHROME_BROWSER_ASH_LOGIN_LOGIN_API_DATA_FOR_NEXT_LOGIN_ATTEMPT_PREF_CLEANER_H_

#include "base/scoped_observation.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace chromeos {

// Clears the pref `kLoginExtensionApiDataForNextLoginAttempt` when the session
// becomes active.
class LoginApiDataForNextLoginAttemptPrefCleaner
    : public session_manager::SessionManagerObserver {
 public:
  LoginApiDataForNextLoginAttemptPrefCleaner();

  LoginApiDataForNextLoginAttemptPrefCleaner(
      const LoginApiDataForNextLoginAttemptPrefCleaner&) = delete;
  LoginApiDataForNextLoginAttemptPrefCleaner& operator=(
      const LoginApiDataForNextLoginAttemptPrefCleaner&) = delete;

  ~LoginApiDataForNextLoginAttemptPrefCleaner() override;

 protected:
  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

 private:
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_LOGIN_API_DATA_FOR_NEXT_LOGIN_ATTEMPT_PREF_CLEANER_H_
