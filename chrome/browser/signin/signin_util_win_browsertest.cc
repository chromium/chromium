// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/wincrypt_shim.h"
#include "build/build_config.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util_win.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

namespace {

class TestDiceTurnSyncOnHelperDelegate : public DiceTurnSyncOnHelper::Delegate {
  ~TestDiceTurnSyncOnHelperDelegate() override {}

  // DiceTurnSyncOnHelper::Delegate:
  void ShowLoginError(const std::string& email,
                      const std::string& error_message) override {}
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override {
    std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
  }
  void ShowEnterpriseAccountConfirmation(
      const std::string& email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override {
    std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
  }
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override {
    std::move(callback).Run(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  }
  void ShowSyncSettings() override {}
  void SwitchToProfile(Profile* new_profile) override {}
};

struct SigninUtilWinBrowserTestParams {
  SigninUtilWinBrowserTestParams(bool is_first_run,
                                 const base::string16& gaia_id,
                                 const base::string16& email,
                                 const std::string& refresh_token,
                                 bool expect_is_started)
      : is_first_run(is_first_run),
        gaia_id(gaia_id),
        email(email),
        refresh_token(refresh_token),
        expect_is_started(expect_is_started) {}

  bool is_first_run = false;
  base::string16 gaia_id;
  base::string16 email;
  std::string refresh_token;
  bool expect_is_started = false;
};

void AssertSigninStarted(bool expect_is_started, Profile* profile) {
  ASSERT_EQ(expect_is_started, profile->GetPrefs()->GetBoolean(
                                   prefs::kSignedInWithCredentialProvider));
}

}  // namespace

class SigninUtilWinBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<SigninUtilWinBrowserTestParams> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(GetParam().is_first_run
                                   ? switches::kForceFirstRun
                                   : switches::kNoFirstRun);
  }

  bool SetUpUserDataDirectory() override {
    registry_override_.OverrideRegistry(HKEY_CURRENT_USER);

    base::win::RegKey key;
    CreateRegKey(&key);

    if (!GetParam().email.empty()) {
      EXPECT_TRUE(key.Valid());
      EXPECT_EQ(ERROR_SUCCESS,
                key.WriteValue(
                    base::ASCIIToUTF16(credential_provider::kKeyEmail).c_str(),
                    GetParam().email.c_str()));
    }

    if (!GetParam().refresh_token.empty())
      WriteRefreshToken(&key, GetParam().refresh_token);

    if (GetParam().expect_is_started) {
      signin_util::SetDiceTurnSyncOnHelperDelegateForTesting(
          std::unique_ptr<DiceTurnSyncOnHelper::Delegate>(
              new TestDiceTurnSyncOnHelperDelegate()));
    }

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

  void CreateRegKey(base::win::RegKey* key) {
    if (!GetParam().gaia_id.empty()) {
      EXPECT_EQ(
          ERROR_SUCCESS,
          key->Create(HKEY_CURRENT_USER,
                      credential_provider::kRegHkcuAccountsPath, KEY_WRITE));
      EXPECT_EQ(ERROR_SUCCESS,
                key->CreateKey(GetParam().gaia_id.c_str(), KEY_WRITE));
    }
  }

  void WriteRefreshToken(base::win::RegKey* key,
                         const std::string& refresh_token) {
    EXPECT_TRUE(key->Valid());
    DATA_BLOB plaintext;
    plaintext.pbData =
        reinterpret_cast<BYTE*>(const_cast<char*>(refresh_token.c_str()));
    plaintext.cbData = static_cast<DWORD>(refresh_token.length());

    DATA_BLOB ciphertext;
    ASSERT_TRUE(::CryptProtectData(&plaintext, L"Gaia refresh token", nullptr,
                                   nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                                   &ciphertext));
    std::string encrypted_data(reinterpret_cast<char*>(ciphertext.pbData),
                               ciphertext.cbData);
    EXPECT_EQ(
        ERROR_SUCCESS,
        key->WriteValue(
            base::ASCIIToUTF16(credential_provider::kKeyRefreshToken).c_str(),
            encrypted_data.c_str(), encrypted_data.length(), REG_BINARY));
    LocalFree(ciphertext.pbData);
  }

  void ExpectRefreshTokenExists(bool exists) {
    base::win::RegKey key;
    EXPECT_EQ(ERROR_SUCCESS,
              key.Open(HKEY_CURRENT_USER,
                       credential_provider::kRegHkcuAccountsPath, KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS, key.OpenKey(GetParam().gaia_id.c_str(), KEY_READ));
    EXPECT_EQ(
        exists,
        key.HasValue(
            base::ASCIIToUTF16(credential_provider::kKeyRefreshToken).c_str()));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
};

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTest, Run) {
  ASSERT_EQ(GetParam().is_first_run, first_run::IsChromeFirstRun());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile =
      profile_manager->GetLastUsedProfile(profile_manager->user_data_dir());
  ASSERT_EQ(profile_manager->GetInitialProfileDir(),
            profile->GetPath().BaseName());

  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  ASSERT_NE(nullptr, browser);

  AssertSigninStarted(GetParam().expect_is_started, profile);

  // If a refresh token was specified and a sign in attempt was expected, make
  // sure the refresh token was removed from the registry.
  if (!GetParam().refresh_token.empty() && GetParam().expect_is_started)
    ExpectRefreshTokenExists(false);
}

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTest, ReauthNoop) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile =
      profile_manager->GetLastUsedProfile(profile_manager->user_data_dir());

  // Whether the profile was signed in with the credential provider or not,
  // reauth should be a noop.
  ASSERT_FALSE(signin_util::ReauthWithCredentialProviderIfPossible(profile));
}

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTest, NoReauthAfterSignout) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile =
      profile_manager->GetLastUsedProfile(profile_manager->user_data_dir());

  if (GetParam().expect_is_started) {
    // Write a new refresh token.
    base::win::RegKey key;
    CreateRegKey(&key);
    WriteRefreshToken(&key, "lst-new");
    ASSERT_FALSE(signin_util::ReauthWithCredentialProviderIfPossible(profile));

    // Sign user out of browser.
    auto* primary_account_mutator =
        IdentityManagerFactory::GetForProfile(profile)
            ->GetPrimaryAccountMutator();
    primary_account_mutator->ClearPrimaryAccount(
        signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
        signin_metrics::FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST,
        signin_metrics::SignoutDelete::DELETED);

    // Even with a refresh token available, no reauth happens if the profile
    // is signed out.
    ASSERT_FALSE(signin_util::ReauthWithCredentialProviderIfPossible(profile));
  }
}

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTest, FixReauth) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile =
      profile_manager->GetLastUsedProfile(profile_manager->user_data_dir());

  if (GetParam().expect_is_started) {
    // Write a new refresh token. This time reauth should work.
    base::win::RegKey key;
    CreateRegKey(&key);
    WriteRefreshToken(&key, "lst-new");
    ASSERT_FALSE(signin_util::ReauthWithCredentialProviderIfPossible(profile));

    // Make sure the profile stays signed in, but in an auth error state.
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    signin::UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_manager, identity_manager->GetPrimaryAccountId(),
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_SERVER));

    // If the profile remains signed in but is in an auth error state,
    // reauth should happen.
    ASSERT_TRUE(signin_util::ReauthWithCredentialProviderIfPossible(profile));
  }
}

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest1,
                         SigninUtilWinBrowserTest,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/false,
                             /*gaia_id=*/base::string16(),
                             /*email=*/base::string16(),
                             /*refresh_token=*/std::string(),
                             /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest2,
                         SigninUtilWinBrowserTest,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/true,
                             /*gaia_id=*/base::string16(),
                             /*email=*/base::string16(),
                             /*refresh_token=*/std::string(),
                             /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest3,
                         SigninUtilWinBrowserTest,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/true,
                             /*gaia_id=*/L"gaia-123456",
                             /*email=*/base::string16(),
                             /*refresh_token=*/std::string(),
                             /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest4,
                         SigninUtilWinBrowserTest,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/true,
                             /*gaia_id=*/L"gaia-123456",
                             /*email=*/L"foo@gmail.com",
                             /*refresh_token=*/std::string(),
                             /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest5,
                         SigninUtilWinBrowserTest,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/true,
                             /*gaia_id=*/L"gaia-123456",
                             /*email=*/L"foo@gmail.com",
                             /*refresh_token=*/"lst-123456",
                             /*expect_is_started=*/true)));
