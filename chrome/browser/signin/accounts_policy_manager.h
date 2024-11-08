// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNTS_POLICY_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNTS_POLICY_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "base/scoped_observation.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

class Profile;

class AccountsPolicyManager : public KeyedService,
                              public signin::IdentityManager::Observer {
 public:
  explicit AccountsPolicyManager(Profile* profile);
  ~AccountsPolicyManager() override;

  AccountsPolicyManager(const AccountsPolicyManager&) = delete;
  AccountsPolicyManager& operator=(const AccountsPolicyManager&) = delete;

  void Initialize();
  void Shutdown() override;

 private:
  friend class AccountsPolicyManagerTest;

  // Handlers for preference changes.
  void OnSigninAllowedPrefChanged();
  void OnGoogleServicesUsernamePatternChanged();

  // Ensures that the primary account for |profile| is allowed:
  // * If profile does not have any primary account, then this is a no-op.
  // * If |IsUserSignoutAllowedForProfile| is allowed and the primary account
  //   is no longer allowed, then this clears the primary account.
  // * If |IsUserSignoutAllowedForProfile| is not allowed and the primary
  // account
  //   is not longer allowed, then this removes the profile.
  void EnsurePrimaryAccountAllowedForProfile(
      Profile* profile,
      signin_metrics::ProfileSignout clear_primary_account_source);

#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_CHROMEOS)
  class DeleteProfileDialogManager;

  // SHows the delete profile dialog.
  void ShowDeleteProfileDialog(Profile* profile, const std::string& email);

  // Called when the profile was deleted.
  void OnUserConfirmedProfileDeletion(DeleteProfileDialogManager* manager,
                                      base::FilePath profile_path);

  void SetHideUIForTesting(bool hide_ui_for_testing) {
    hide_ui_for_testing_ = hide_ui_for_testing;
  }
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void RemoveUnallowedAccounts();

  // IdentityManager::Observer implementation.
  void OnRefreshTokensLoaded() override;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  raw_ptr<Profile> profile_;

  // Helper object to listen for changes to the signin allowed preference.
  BooleanPrefMember signin_allowed_;

  // Helper object to listen for changes to signin preferences stored in non-
  // profile-specific local prefs (like kGoogleServicesUsernamePattern).
  PrefChangeRegistrar local_state_pref_registrar_;

#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<DeleteProfileDialogManager> delete_profile_dialog_manager_;
  bool hide_ui_for_testing_ = false;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  PrefChangeRegistrar profile_pref_change_registrar_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  base::WeakPtrFactory<AccountsPolicyManager> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNTS_POLICY_MANAGER_H_
