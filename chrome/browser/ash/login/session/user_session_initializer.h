// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SESSION_USER_SESSION_INITIALIZER_H_
#define CHROME_BROWSER_ASH_LOGIN_SESSION_USER_SESSION_INITIALIZER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user.h"

class Profile;

namespace user_manager {
class User;
}

namespace ash {

class UserSessionInitializer : public session_manager::SessionManagerObserver {
 public:
  // Parameters to use when initializing the RLZ library.  These fields need
  // to be retrieved from a blocking task and this structure is used to pass
  // the data.
  struct RlzInitParams {
    // Set to true if RLZ is disabled.
    bool disabled;

    // The elapsed time since the device went through the OOBE.  This can
    // be a very long time.
    base::TimeDelta time_since_oobe_completion;
  };

  UserSessionInitializer();
  UserSessionInitializer(const UserSessionInitializer&) = delete;
  UserSessionInitializer& operator=(const UserSessionInitializer&) = delete;
  ~UserSessionInitializer() override;

  // Returns UserSessionInitializer instance.
  static UserSessionInitializer* Get();

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;
  void OnUserSessionStarted(bool is_primary_user) override;
  void OnUserSessionStartUpTaskCompleted() override;

  // Called before a session begins loading.
  void PreStartSession(bool is_primary_session);

  // Initialize child user profile services that depend on the policy.
  void InitializeChildUserServices(Profile* profile);

  void set_init_rlz_impl_closure_for_testing(base::OnceClosure closure) {
    init_rlz_impl_closure_for_testing_ = std::move(closure);
  }

  bool get_inited_for_testing() { return inited_for_testing_; }

 private:
  // Initialize RLZ.
  void InitRlz(Profile* profile);

  // Get the NSS cert database for the user represented with `profile`
  // and start certificate loader with it.
  void InitializeCerts(Profile* profile);

  // Starts loading CRL set.
  void InitializeCRLSetFetcher();

  // Initialize all services that need the primary profile.
  void InitializePrimaryProfileServices(Profile* profile,
                                        const user_manager::User* user);

  // Initialize a `ScalableIph` service for a profile.
  void InitializeScalableIph(Profile* profile);

  // Initializes RLZ. If `disabled` is true, RLZ pings are disabled.
  void InitRlzImpl(Profile* profile, const RlzInitParams& params);

  raw_ptr<Profile, DanglingUntriaged> primary_profile_ = nullptr;

  bool inited_for_testing_ = false;
  base::OnceClosure init_rlz_impl_closure_for_testing_;

  base::WeakPtrFactory<UserSessionInitializer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SESSION_USER_SESSION_INITIALIZER_H_
