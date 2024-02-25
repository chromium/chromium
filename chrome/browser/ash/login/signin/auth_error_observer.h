// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTH_ERROR_OBSERVER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTH_ERROR_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/sync/service/sync_service_observer.h"

class GoogleServiceAuthError;
class Profile;

namespace ash {

// This class is responsible for detecting authentication problems reported
// by sync service and SigninErrorController on a user profile.
class AuthErrorObserver : public KeyedService,
                          public syncer::SyncServiceObserver,
                          public SigninErrorController::Observer {
 public:
  // Whether `profile` should be observed. Currently, this returns true only
  // when `profile` is a user profile of a gaia user or a supervised user.
  static bool ShouldObserve(Profile* profile);

  explicit AuthErrorObserver(Profile* profile);

  AuthErrorObserver(const AuthErrorObserver&) = delete;
  AuthErrorObserver& operator=(const AuthErrorObserver&) = delete;

  ~AuthErrorObserver() override;

  // Starts to observe SyncService and SigninErrorController.
  void StartObserving();

 private:
  friend class AuthErrorObserverFactory;

  // KeyedService implementation.
  void Shutdown() override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

  // Handles an auth error for the Primary / Sync account. `auth_error` can be
  // `GoogleServiceAuthError::AuthErrorNone()`, in which case, it resets and
  // marks the account as valid. Note: `auth_error` must correspond to an error
  // in the Primary / Sync account and not a Secondary Account.
  void HandleAuthError(const GoogleServiceAuthError& auth_error);

  const raw_ptr<Profile> profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTH_ERROR_OBSERVER_H_
