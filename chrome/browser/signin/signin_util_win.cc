// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util_win.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/wincrypt_shim.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin_util {

namespace {

std::unique_ptr<DiceTurnSyncOnHelper::Delegate>*
GetDiceTurnSyncOnHelperDelegateForTestingStorage() {
  static base::NoDestructor<std::unique_ptr<DiceTurnSyncOnHelper::Delegate>>
      delegate;
  return delegate.get();
}

std::string DecryptRefreshToken(const std::string& cipher_text) {
  DATA_BLOB input;
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(cipher_text.data()));
  input.cbData = static_cast<DWORD>(cipher_text.length());
  DATA_BLOB output;
  BOOL result = ::CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                                     CRYPTPROTECT_UI_FORBIDDEN, &output);

  if (!result)
    return std::string();

  std::string refresh_token(reinterpret_cast<char*>(output.pbData),
                            output.cbData);
  ::LocalFree(output.pbData);
  return refresh_token;
}

// Finish the process of import credentials.  This is either called directly
// from ImportCredentialsFromProvider() if a browser window for the profile is
// already available or is delayed until a browser can first be opened.
void FinishImportCredentialsFromProvider(const CoreAccountId& account_id,
                                         Browser* browser,
                                         Profile* profile,
                                         Profile::CreateStatus status) {
  // DiceTurnSyncOnHelper deletes itself once done.
  if (GetDiceTurnSyncOnHelperDelegateForTestingStorage()->get()) {
    new DiceTurnSyncOnHelper(
        profile, signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON,
        signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
        signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT, account_id,
        DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
        std::move(*GetDiceTurnSyncOnHelperDelegateForTestingStorage()),
        base::DoNothing());
  } else {
    if (!browser)
      browser = chrome::FindLastActiveWithProfile(profile);

    new DiceTurnSyncOnHelper(
        profile, browser,
        signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON,
        signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
        signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT, account_id,
        DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  }

  // Mark this profile as having been signed in with the credential provider.
  profile->GetPrefs()->SetBoolean(prefs::kSignedInWithCredentialProvider, true);
}

// Start the process of importing credentials from the credential provider given
// that all the required information is available.  The process depends on
// having a browser window for the profile.  If a browser window exists the
// profile be signed in and sync will be starting up.  If not, the profile will
// be still be signed in but sync will be started once the browser window is
// ready.
void ImportCredentialsFromProvider(Profile* profile,
                                   const base::string16& gaia_id,
                                   const base::string16& email,
                                   const std::string& refresh_token,
                                   bool turn_on_sync) {
  // For debugging purposes, record that the credentials for this profile
  // came from a credential provider.
  AboutSigninInternals* signin_internals =
      AboutSigninInternalsFactory::GetInstance()->GetForProfile(profile);
  signin_internals->OnAuthenticationResultReceived("Credential Provider");

  CoreAccountId account_id =
      IdentityManagerFactory::GetForProfile(profile)
          ->GetAccountsMutator()
          ->AddOrUpdateAccount(base::UTF16ToUTF8(gaia_id),
                               base::UTF16ToUTF8(email), refresh_token,
                               /*is_under_advanced_protection=*/false,
                               signin_metrics::SourceForRefreshTokenOperation::
                                   kMachineLogon_CredentialProvider);

  if (turn_on_sync) {
    Browser* browser = chrome::FindLastActiveWithProfile(profile);
    if (browser) {
      FinishImportCredentialsFromProvider(account_id, browser, profile,
                                          Profile::CREATE_STATUS_CREATED);
    } else {
      // If no active browser exists yet, this profile is in the process of
      // being created.  Wait for the browser to be created before finishing the
      // sign in.  This object deletes itself when done.
      new profiles::BrowserAddedForProfileObserver(
          profile, base::BindRepeating(&FinishImportCredentialsFromProvider,
                                       account_id, nullptr));
    }
  }
}

// Attempt to sign in with a credentials from a system installed credential
// provider if available.  If |auth_gaia_id| is not empty then the system
// credential must be for the same account.  Starts the process to turn on DICE
// only if |turn_on_sync| is true.
bool TrySigninWithCredentialProvider(Profile* profile,
                                     const base::string16& auth_gaia_id,
                                     bool turn_on_sync) {
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, credential_provider::kRegHkcuAccountsPath,
               KEY_READ) != ERROR_SUCCESS) {
    return false;
  }

  base::win::RegistryKeyIterator it(key.Handle(), L"");
  if (!it.Valid() || it.SubkeyCount() == 0)
    return false;

  base::win::RegKey key_account(key.Handle(), it.Name(), KEY_READ | KEY_WRITE);
  if (!key_account.Valid())
    return false;

  base::string16 gaia_id = it.Name();
  if (!auth_gaia_id.empty() && auth_gaia_id != gaia_id)
    return false;

  base::string16 email;
  if (key_account.ReadValue(
          base::UTF8ToUTF16(credential_provider::kKeyEmail).c_str(), &email) !=
      ERROR_SUCCESS) {
    return false;
  }

  // Read the encrypted refresh token.  The data is stored in binary format.
  // No matter what happens, delete the registry entry.

  std::string encrypted_refresh_token;
  DWORD size = 0;
  DWORD type;
  if (key_account.ReadValue(
          base::UTF8ToUTF16(credential_provider::kKeyRefreshToken).c_str(),
          nullptr, &size, &type) != ERROR_SUCCESS) {
    return false;
  }

  encrypted_refresh_token.resize(size);
  bool reauth_attempted = false;
  key_account.ReadValue(
      base::UTF8ToUTF16(credential_provider::kKeyRefreshToken).c_str(),
      const_cast<char*>(encrypted_refresh_token.c_str()), &size, &type);
  if (!gaia_id.empty() && !email.empty() && type == REG_BINARY &&
      !encrypted_refresh_token.empty()) {
    std::string refresh_token = DecryptRefreshToken(encrypted_refresh_token);
    if (!refresh_token.empty()) {
      reauth_attempted = true;
      ImportCredentialsFromProvider(profile, gaia_id, email, refresh_token,
                                    turn_on_sync);
    }
  }

  key_account.DeleteValue(
      base::UTF8ToUTF16(credential_provider::kKeyRefreshToken).c_str());
  return reauth_attempted;
}

}  // namespace

void SetDiceTurnSyncOnHelperDelegateForTesting(
    std::unique_ptr<DiceTurnSyncOnHelper::Delegate> delegate) {
  GetDiceTurnSyncOnHelperDelegateForTestingStorage()->swap(delegate);
}

void SigninWithCredentialProviderIfPossible(Profile* profile) {
  // Check to see if auto signin information is available.  Only applies if:
  //
  //  - This is first run.
  //  - Opening the initial profile.
  //  - Not already signed in.
  if (!(first_run::IsChromeFirstRun() &&
        g_browser_process->profile_manager()->GetInitialProfileDir() ==
            profile->GetPath().BaseName() &&
        !IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount())) {
    return;
  }

  TrySigninWithCredentialProvider(profile, base::string16(), true);
}

bool ReauthWithCredentialProviderIfPossible(Profile* profile) {
  // Check to see if auto signin information is available.  Only applies if:
  //
  //  - The profile is marked as having been signed in with a system credential.
  //  - The profile is already signed in.
  //  - The profile is in an auth error state.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!(profile->GetPrefs()->GetBoolean(
            prefs::kSignedInWithCredentialProvider) &&
        identity_manager->HasPrimaryAccount() &&
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            identity_manager->GetPrimaryAccountId()))) {
    return false;
  }

  base::string16 gaia_id =
      base::UTF8ToUTF16(identity_manager->GetPrimaryAccountInfo().gaia.c_str());
  return TrySigninWithCredentialProvider(profile, gaia_id, false);
}

}  // namespace signin_util
