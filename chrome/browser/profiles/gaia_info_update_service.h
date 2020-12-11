// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_
#define CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

// This service kicks off a download of the user's name and profile picture.
// The results are saved in the profile info cache.
class GAIAInfoUpdateService : public KeyedService,
                              public signin::IdentityManager::Observer {
 public:
  GAIAInfoUpdateService(signin::IdentityManager* identity_manager,
                        ProfileAttributesStorage* profile_attributes_storage,
                        const base::FilePath& profile_path,
                        PrefService* prefs);

  ~GAIAInfoUpdateService() override;

  // Updates the GAIA info for the profile associated with this instance.
  virtual void UpdatePrimaryAccount();

  // Checks if downloading GAIA info for the given profile is allowed.
  static bool ShouldUseGAIAProfileInfo(Profile* profile);

  // Overridden from KeyedService:
  void Shutdown() override;

 private:
  void ClearProfileEntry();
  bool ShouldUpdatePrimaryAccount();
  void UpdatePrimaryAccount(const AccountInfo& info);

  void UpdateAnyAccount(const AccountInfo& info);

  // Overridden from signin::IdentityManager::Observer:
  void OnUnconsentedPrimaryAccountChanged(
      const CoreAccountInfo& unconsented_primary_account_info) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  signin::IdentityManager* identity_manager_;
  ProfileAttributesStorage* profile_attributes_storage_;
  const base::FilePath profile_path_;
  PrefService* profile_prefs_;
  // TODO(msalama): remove when |SigninProfileAttributesUpdater| is folded into
  // |GAIAInfoUpdateService|.
  std::string gaia_id_of_profile_attribute_entry_;

  DISALLOW_COPY_AND_ASSIGN(GAIAInfoUpdateService);
};

#endif  // CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_
