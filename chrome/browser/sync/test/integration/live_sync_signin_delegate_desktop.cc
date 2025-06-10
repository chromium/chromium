// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/live_sync_signin_delegate_desktop.h"

#include "base/check_deref.h"
#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

namespace {

// Command-line switches used for live tests.
constexpr char kSyncUserForTest[] = "sync-user-for-test";
constexpr char kSyncPasswordForTest[] = "sync-password-for-test";

}  // namespace

LiveSyncSigninDelegateDesktop::LiveSyncSigninDelegateDesktop(Profile* profile)
    : profile_(CHECK_DEREF(profile).GetWeakPtr()) {}

LiveSyncSigninDelegateDesktop::~LiveSyncSigninDelegateDesktop() = default;

bool LiveSyncSigninDelegateDesktop::SignIn(SyncTestAccount account,
                                           signin::ConsentLevel consent_level) {
  if (account != SyncTestAccount::kDefaultAccount) {
    LOG(ERROR) << "Live tests only support one account";
    return false;
  }

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  const std::string username = cl->GetSwitchValueASCII(kSyncUserForTest);
  const std::string password = cl->GetSwitchValueASCII(kSyncPasswordForTest);
  if (username.empty() || password.empty()) {
    LOG(ERROR) << "Cannot run live sync tests without GAIA credentials.";
    return false;
  }

  // Switch to `profile_` to ensure a browser exists.
  profiles::testing::SwitchToProfileSync(profile_->GetPath(),
                                         /*always_create=*/true);

  Browser* browser = chrome::FindBrowserWithProfile(profile_.get());
  if (!browser) {
    LOG(ERROR) << "Failed to open browser to sign in.";
    return false;
  }

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

GaiaId LiveSyncSigninDelegateDesktop::GetGaiaIdForAccount(
    SyncTestAccount account) {
  // If you run into the need for this functionality in live tests, a new flag
  // would need to be passed with the gaia ID.
  NOTREACHED() << "Not supported in live tests";
}

std::string LiveSyncSigninDelegateDesktop::GetEmailForAccount(
    SyncTestAccount account) {
  CHECK_EQ(account, SyncTestAccount::kDefaultAccount)
      << "Only one account is supported in live tests";

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  return cl->GetSwitchValueASCII(kSyncUserForTest);
}
