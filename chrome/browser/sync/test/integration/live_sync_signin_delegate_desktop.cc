// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/live_sync_signin_delegate_desktop.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

LiveSyncSigninDelegateDesktop::LiveSyncSigninDelegateDesktop(Profile* profile)
    : profile_(CHECK_DEREF(profile).GetWeakPtr()) {}

LiveSyncSigninDelegateDesktop::~LiveSyncSigninDelegateDesktop() = default;

bool LiveSyncSigninDelegateDesktop::SignIn(const std::string& username,
                                           const std::string& password,
                                           signin::ConsentLevel consent_level) {
  Browser* browser = chrome::FindBrowserWithProfile(profile_.get());
  DCHECK(browser);
  if (!login_ui_test_utils::SignInWithUI(browser, username, password,
                                         consent_level)) {
    LOG(ERROR) << "Could not sign in to GAIA servers.";
    return false;
  }
  return true;
}

bool LiveSyncSigninDelegateDesktop::ConfirmSync() {
  if (!login_ui_test_utils::ConfirmSyncConfirmationDialog(
          chrome::FindBrowserWithProfile(profile_.get()))) {
    LOG(ERROR) << "Failed to dismiss sync confirmation dialog.";
    return false;
  }
  LoginUIServiceFactory::GetForProfile(profile_.get())
      ->SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  return true;
}

void LiveSyncSigninDelegateDesktop::SignOut() {
  signin::ClearPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_.get()));
}

GaiaId LiveSyncSigninDelegateDesktop::GetGaiaIdForUsername(
    const std::string& username) {
  NOTREACHED() << "Not supported";
}
