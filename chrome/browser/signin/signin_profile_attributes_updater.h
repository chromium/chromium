// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class ProfileAttributesStorage;

// This class listens to various signin events and updates the signin-related
// fields of ProfileAttributes.
class SigninProfileAttributesUpdater
    : public KeyedService,
      public SigninErrorController::Observer,
      public signin::IdentityManager::Observer {
 public:
  SigninProfileAttributesUpdater(
      signin::IdentityManager* identity_manager,
      SigninErrorController* signin_error_controller,
      ProfileAttributesStorage* profile_attributes_storage,
      const base::FilePath& profile_path,
      PrefService* prefs);

  ~SigninProfileAttributesUpdater() override;

 private:
  // KeyedService:
  void Shutdown() override;

  // Updates the profile attributes on signin and signout events.
  void UpdateProfileAttributes();

  // SigninErrorController::Observer:
  void OnErrorChanged() override;

  // IdentityManager::Observer:
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;
  void OnUnconsentedPrimaryAccountChanged(
      const CoreAccountInfo& unconsented_primary_account_info) override;

  signin::IdentityManager* identity_manager_;
  SigninErrorController* signin_error_controller_;
  ProfileAttributesStorage* profile_attributes_storage_;
  const base::FilePath profile_path_;
  PrefService* prefs_;
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_{this};
  ScopedObserver<SigninErrorController, SigninErrorController::Observer>
      signin_error_controller_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(SigninProfileAttributesUpdater);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_H_
