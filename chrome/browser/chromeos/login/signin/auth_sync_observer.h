// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_AUTH_SYNC_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_AUTH_SYNC_OBSERVER_H_

#include <string>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/sync/driver/sync_service_observer.h"

class GoogleServiceAuthError;
class Profile;

namespace chromeos {

// This class is responsible for detecting authentication problems reported
// by sync service and SigninErrorController on a user profile.
class AuthSyncObserver : public KeyedService,
                         public syncer::SyncServiceObserver,
                         public SigninErrorController::Observer {
 public:
  // Whether |profile| should be observed. Currently, this returns true only
  // when |profile| is a user profile of a gaia user or a supervised user.
  static bool ShouldObserve(Profile* profile);

  explicit AuthSyncObserver(Profile* profile);
  ~AuthSyncObserver() override;

  // Starts to observe SyncService and SigninErrorController.
  void StartObserving();

 private:
  friend class AuthSyncObserverFactory;

  // KeyedService implementation.
  void Shutdown() override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

  // Handles an auth error for the Primary / Sync account. |auth_error| can be
  // |GoogleServiceAuthError::AuthErrorNone()|, in which case, it resets and
  // marks the account as valid. Note: |auth_error| must correspond to an error
  // in the Primary / Sync account and not a Secondary Account.
  void HandleAuthError(const GoogleServiceAuthError& auth_error);

  // Called on attempt to restore supervised user token.
  void OnSupervisedTokenLoaded(const std::string& token);

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(AuthSyncObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_AUTH_SYNC_OBSERVER_H_
