// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_
#define CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

// This service kicks off a download of the user's name and profile picture.
// The results are saved in the profile info cache.
// It also manages the lifecycle of the signin accounts prefs.
class GAIAInfoUpdateService : public KeyedService,
                              public signin::IdentityManager::Observer {
 public:
  GAIAInfoUpdateService(signin::IdentityManager* identity_manager,
                        ProfileAttributesStorage* profile_attributes_storage,
                        PrefService& pref_service,
                        const base::FilePath& profile_path);

  GAIAInfoUpdateService(const GAIAInfoUpdateService&) = delete;
  GAIAInfoUpdateService& operator=(const GAIAInfoUpdateService&) = delete;

  ~GAIAInfoUpdateService() override;

  // Updates the GAIA info for the profile associated with this instance.
  virtual void UpdatePrimaryAccount();

  // Overridden from KeyedService:
  void Shutdown() override;

 private:
  void ClearProfileEntry();
  bool ShouldUpdatePrimaryAccount();
  void UpdatePrimaryAccount(const AccountInfo& info);

  void UpdateAnyAccount(const AccountInfo& info);

  // Overridden from signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<ProfileAttributesStorage> profile_attributes_storage_;
  raw_ref<PrefService> pref_service_;
  const base::FilePath profile_path_;
  // TODO(msalama): remove when |SigninProfileAttributesUpdater| is folded into
  // |GAIAInfoUpdateService|.
  std::string gaia_id_of_profile_attribute_entry_;
};

#endif  // CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_
