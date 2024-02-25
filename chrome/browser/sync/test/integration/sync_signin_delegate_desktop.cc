// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_signin_delegate_desktop.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

void SyncSigninDelegateDesktop::SigninFake(Profile* profile,
                                           const std::string& username,
                                           signin::ConsentLevel consent_level) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  // Verify HasPrimaryAccount() separately because MakePrimaryAccountAvailable()
  // below DCHECK fails if there is already an authenticated account.
  if (identity_manager->HasPrimaryAccount(consent_level)) {
    DCHECK_EQ(identity_manager->GetPrimaryAccountInfo(consent_level).email,
              username);
    // Don't update the refresh token if we already have one. The reason is
    // that doing so causes Sync (ServerConnectionManager in particular) to
    // mark the current access token as invalid. Since tests typically
    // always hand out the same access token string, any new access token
    // acquired later would also be considered invalid.
    if (!identity_manager->HasPrimaryAccountWithRefreshToken(consent_level)) {
      signin::SetRefreshTokenForPrimaryAccount(identity_manager);
    }
  } else {
    signin::MakePrimaryAccountAvailable(identity_manager, username,
                                        consent_level);
  }
}

bool SyncSigninDelegateDesktop::SigninUI(Profile* profile,
                                         const std::string& username,
                                         const std::string& password,
                                         signin::ConsentLevel consent_level) {
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  DCHECK(browser);
  if (!login_ui_test_utils::SignInWithUI(browser, username, password,
                                         consent_level)) {
    LOG(ERROR) << "Could not sign in to GAIA servers.";
    return false;
  }
  return true;
}

bool SyncSigninDelegateDesktop::ConfirmSyncUI(Profile* profile) {
  if (!login_ui_test_utils::ConfirmSyncConfirmationDialog(
          chrome::FindBrowserWithProfile(profile))) {
    LOG(ERROR) << "Failed to dismiss sync confirmation dialog.";
    return false;
  }
  LoginUIServiceFactory::GetForProfile(profile)->SyncConfirmationUIClosed(
      LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  return true;
}

void SyncSigninDelegateDesktop::SignOutPrimaryAccount(Profile* profile) {
  signin::ClearPrimaryAccount(IdentityManagerFactory::GetForProfile(profile));
}
