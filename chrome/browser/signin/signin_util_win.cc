// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util_win.h"

#include <windows.h>

#include <wincrypt.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/features.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"

namespace signin_util {

namespace {

constexpr signin_metrics::AccessPoint kCredentialsProviderAccessPointWin =
    signin_metrics::AccessPoint::kMachineLogon;

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

// Shows the history sync promo. Called after a browser window is available.
void ShowHistorySyncPromo(const CoreAccountId& account_id,
                          Profile* profile,
                          BrowserWindowInterface* browser) {
  if (!browser) {
    // Chrome failed to open a browser.
    base::debug::DumpWithoutCrashing();
    return;
  }
  CHECK_EQ(browser->GetProfile(), profile);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  HistorySyncOptinService* history_sync_optin_service =
      HistorySyncOptinServiceFactory::GetForProfile(profile);
  CHECK(history_sync_optin_service);

  AccountInfo extended_account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id);

  history_sync_optin_service->StartHistorySyncOptinFlow(
      extended_account_info,
      std::make_unique<HistorySyncOptinServiceDefaultDelegate>(),
      kCredentialsProviderAccessPointWin);
}

// Helper to run |callback| either immediately if a browser exists for
// |profile|, or schedules it to run once a browser is added.
void RunOnBrowserReady(
    Profile* profile,
    base::OnceCallback<void(BrowserWindowInterface*)> callback) {
  BrowserWindowInterface* browser =
      ProfileBrowserCollection::GetForProfile(profile)->GetLastActiveBrowser();
  if (browser) {
    std::move(callback).Run(browser);
  } else {
    // If no active browser exists yet, this profile is in the process of
    // being created. Wait for the browser to be created.
    // BrowserAddedForProfileObserver deletes itself when done.
    new profiles::BrowserAddedForProfileObserver(profile, std::move(callback));
  }
}

// Start the process of importing credentials from the credential provider given
// that all the required information is available. Signs in the profile, unless
// it is performing a reauth.
void ImportCredentialsFromProvider(Profile* profile,
                                   const std::wstring& gaia_id,
                                   const std::wstring& email,
                                   const std::string& refresh_token,
                                   bool is_reauth) {
  // For debugging purposes, record that the credentials for this profile
  // came from a credential provider.
  AboutSigninInternals* signin_internals =
      AboutSigninInternalsFactory::GetInstance()->GetForProfile(profile);
  signin_internals->OnAuthenticationResultReceived("Credential Provider");

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountId account_id =
      identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
          GaiaId(base::WideToUTF8(gaia_id)), base::WideToUTF8(email),
          refresh_token,
          /*is_under_advanced_protection=*/false,
          kCredentialsProviderAccessPointWin,
          signin_metrics::SourceForRefreshTokenOperation::
              kMachineLogon_CredentialProvider);

  if (!is_reauth) {
    identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
        account_id, signin::ConsentLevel::kSignin,
        kCredentialsProviderAccessPointWin);

    if (base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
      // Show the History Sync promo once a browser window is available.
      RunOnBrowserReady(
          profile, base::BindOnce(&ShowHistorySyncPromo, account_id, profile));
    }
  }

  // Mark this profile as having been signed in with the credential provider.
  profile->GetPrefs()->SetBoolean(prefs::kSignedInWithCredentialProvider, true);
}

// Extracts the |cred_provider_gaia_id| and |cred_provider_email| for the user
// signed in throuhg credential provider.
void ExtractCredentialProviderUser(std::wstring* cred_provider_gaia_id,
                                   std::wstring* cred_provider_email) {
  CHECK(cred_provider_gaia_id, base::NotFatalUntil::M162);
  CHECK(cred_provider_email, base::NotFatalUntil::M162);

  cred_provider_gaia_id->clear();
  cred_provider_email->clear();

  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, credential_provider::kRegHkcuAccountsPath,
               KEY_READ) != ERROR_SUCCESS) {
    return;
  }

  base::win::RegistryKeyIterator it(key.Handle(), L"");
  if (!it.Valid() || it.SubkeyCount() != 1)
    return;

  base::win::RegKey key_account(key.Handle(), it.Name(), KEY_QUERY_VALUE);
  if (!key_account.Valid())
    return;

  std::wstring email;
  if (key_account.ReadValue(
          base::UTF8ToWide(credential_provider::kKeyEmail).c_str(), &email) !=
      ERROR_SUCCESS) {
    return;
  }

  *cred_provider_gaia_id = it.Name();
  *cred_provider_email = email;
}

// Attempt to sign in with a credentials from a system installed credential
// provider if available.  If `auth_gaia_id` is not empty then the system
// credential must be for the same account.  Starts the process to turn on DICE
// only if `is_reauth` is false.
bool TrySigninWithCredentialProvider(Profile* profile,
                                     const GaiaId& auth_gaia_id,
                                     bool is_reauth) {
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

  std::wstring gaia_id = it.Name();
  if (!auth_gaia_id.empty() &&
      base::UTF8ToWide(auth_gaia_id.ToString()) != gaia_id) {
    return false;
  }

  std::wstring email;
  if (key_account.ReadValue(
          base::UTF8ToWide(credential_provider::kKeyEmail).c_str(), &email) !=
      ERROR_SUCCESS) {
    return false;
  }

  // Read the encrypted refresh token.  The data is stored in binary format.
  // No matter what happens, delete the registry entry.

  std::string encrypted_refresh_token;
  DWORD size = 0;
  DWORD type;
  if (key_account.ReadValue(
          base::UTF8ToWide(credential_provider::kKeyRefreshToken).c_str(),
          nullptr, &size, &type) != ERROR_SUCCESS) {
    return false;
  }

  encrypted_refresh_token.resize(size);
  bool reauth_attempted = false;
  key_account.ReadValue(
      base::UTF8ToWide(credential_provider::kKeyRefreshToken).c_str(),
      const_cast<char*>(encrypted_refresh_token.c_str()), &size, &type);
  if (!gaia_id.empty() && !email.empty() && type == REG_BINARY &&
      !encrypted_refresh_token.empty()) {
    std::string refresh_token = DecryptRefreshToken(encrypted_refresh_token);
    if (!refresh_token.empty()) {
      reauth_attempted = true;
      ImportCredentialsFromProvider(profile, gaia_id, email, refresh_token,
                                    is_reauth);
    }
  }

  key_account.DeleteValue(
      base::UTF8ToWide(credential_provider::kKeyRefreshToken).c_str());
  return reauth_attempted;
}

}  // namespace

// Credential provider needs to stick to profile it previously used to import
// credentials. Thus, if there is another profile that was previously signed in
// with credential provider regardless of whether user signed in or out,
// credential provider shouldn't attempt to import credentials into current
// profile.
bool IsGCPWUsedInOtherProfile(Profile* profile) {
  CHECK(profile, base::NotFatalUntil::M162);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    std::vector<ProfileAttributesEntry*> entries =
        profile_manager->GetProfileAttributesStorage()
            .GetAllProfilesAttributes();

    for (const ProfileAttributesEntry* entry : entries) {
      if (entry->GetPath() == profile->GetPath())
        continue;

      if (entry->IsSignedInWithCredentialProvider())
        return true;
    }
  }

  return false;
}

void SigninWithCredentialProviderIfPossible(Profile* profile) {
  // This flow is used for first time signin through credential provider. Any
  // subsequent signin for the credential provider user needs to go through
  // reauth flow.
  if (profile->GetPrefs()->GetBoolean(prefs::kSignedInWithCredentialProvider))
    return;

  std::wstring cred_provider_gaia_id;
  std::wstring cred_provider_email;

  ExtractCredentialProviderUser(&cred_provider_gaia_id, &cred_provider_email);
  if (cred_provider_gaia_id.empty() || cred_provider_email.empty())
    return;

  // Chrome doesn't allow signing into current profile if the same user is
  // signed in another profile.
  if (!CanOfferSignin(profile, GaiaId(base::WideToUTF8(cred_provider_gaia_id)),
                      base::WideToUTF8(cred_provider_email),
                      /*allow_account_from_other_profile=*/false)
           .IsOk() ||
      IsGCPWUsedInOtherProfile(profile)) {
    return;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  GaiaId gaia_id;
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    gaia_id =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .gaia;
  }

  TrySigninWithCredentialProvider(profile, gaia_id,
                                  /*is_reauth=*/!gaia_id.empty());
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
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin)))) {
    return false;
  }

  const GaiaId gaia_id =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  return TrySigninWithCredentialProvider(profile, gaia_id, /*is_reauth=*/true);
}

}  // namespace signin_util
