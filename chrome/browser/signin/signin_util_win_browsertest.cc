// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/wincrypt_shim.h"
#include "build/build_config.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util_win.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
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
#include "content/public/test/browser_test.h"

class SigninUIError;

namespace {

class TestTurnSyncOnHelperDelegate : public TurnSyncOnHelper::Delegate {
  ~TestTurnSyncOnHelperDelegate() override {}

  // TurnSyncOnHelper::Delegate:
  void ShowLoginError(const SigninUIError& error) override {}
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      signin::SigninChoiceCallback callback) override {
    std::move(callback).Run(signin::SIGNIN_CHOICE_CONTINUE);
  }
  void ShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      signin::SigninChoiceCallback callback) override {
    std::move(callback).Run(signin::SIGNIN_CHOICE_CONTINUE);
  }
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override {
    std::move(callback).Run(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  }
  void ShowSyncDisabledConfirmation(
      bool is_managed_account,
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override {}
  void ShowSyncSettings() override {}
  void SwitchToProfile(Profile* new_profile) override {}
};

struct SigninUtilWinBrowserTestParams {
  SigninUtilWinBrowserTestParams(bool is_first_run,
                                 const std::wstring& gaia_id,
                                 const std::wstring& email,
                                 const std::string& refresh_token,
                                 bool expect_is_started)
      : is_first_run(is_first_run),
        gaia_id(gaia_id),
        email(email),
        refresh_token(refresh_token),
        expect_is_started(expect_is_started) {}

  bool is_first_run = false;
  std::wstring gaia_id;
  std::wstring email;
  std::string refresh_token;
  bool expect_is_started = false;
};

void AssertSigninStarted(bool expect_is_started, Profile* profile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();

  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());

  ASSERT_NE(entry, nullptr);

  ASSERT_EQ(expect_is_started, entry->IsSignedInWithCredentialProvider());
}

}  // namespace

class BrowserTestHelper {
 public:
  BrowserTestHelper(const std::wstring& gaia_id,
                    const std::wstring& email,
                    const std::string& refresh_token)
      : gaia_id_(gaia_id), email_(email), refresh_token_(refresh_token) {}

 protected:
  void CreateRegKey(base::win::RegKey* key) {
    if (!gaia_id_.empty()) {
      EXPECT_EQ(
          ERROR_SUCCESS,
          key->Create(HKEY_CURRENT_USER,
                      credential_provider::kRegHkcuAccountsPath, KEY_WRITE));
      EXPECT_EQ(ERROR_SUCCESS, key->CreateKey(gaia_id_.c_str(), KEY_WRITE));
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
            base::ASCIIToWide(credential_provider::kKeyRefreshToken).c_str(),
            encrypted_data.c_str(), encrypted_data.length(), REG_BINARY));
    LocalFree(ciphertext.pbData);
  }

  void ExpectRefreshTokenExists(bool exists) {
    base::win::RegKey key;
    EXPECT_EQ(ERROR_SUCCESS,
              key.Open(HKEY_CURRENT_USER,
                       credential_provider::kRegHkcuAccountsPath, KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS, key.OpenKey(gaia_id_.c_str(), KEY_READ));
    EXPECT_EQ(
        exists,
        key.HasValue(
            base::ASCIIToWide(credential_provider::kKeyRefreshToken).c_str()));
  }

 public:
  void SetSigninUtilRegistry() {
    base::win::RegKey key;
    CreateRegKey(&key);

    if (!email_.empty()) {
      EXPECT_TRUE(key.Valid());
      EXPECT_EQ(ERROR_SUCCESS,
                key.WriteValue(
                    base::ASCIIToWide(credential_provider::kKeyEmail).c_str(),
                    email_.c_str()));
    }

    if (!refresh_token_.empty())
      WriteRefreshToken(&key, refresh_token_);
  }

  bool IsPreTest() {
    std::string test_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();
    LOG(INFO) << "PRE_ test_name " << test_name;
    return test_name.find("PRE_") != std::string::npos;
  }

  bool IsPrePreTest() {
    std::string test_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();
    LOG(INFO) << "PRE_PRE_ test_name " << test_name;
    return test_name.find("PRE_PRE_") != std::string::npos;
  }

 private:
  std::wstring gaia_id_;
  std::wstring email_;
  std::string refresh_token_;
};

class SigninUtilWinBrowserTest
    : public BrowserTestHelper,
      public InProcessBrowserTest,
      public testing::WithParamInterface<SigninUtilWinBrowserTestParams> {
 public:
  SigninUtilWinBrowserTest()
      : BrowserTestHelper(GetParam().gaia_id,
                          GetParam().email,
                          GetParam().refresh_token) {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(GetParam().is_first_run
                                   ? switches::kForceFirstRun
                                   : switches::kNoFirstRun);
  }

  bool SetUpUserDataDirectory() override {
    registry_override_.OverrideRegistry(HKEY_CURRENT_USER);

    signin_util::SetTurnSyncOnHelperDelegateForTesting(
        std::unique_ptr<TurnSyncOnHelper::Delegate>(
            new TestTurnSyncOnHelperDelegate()));

    SetSigninUtilRegistry();

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
};

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTest, Run) {
  ASSERT_EQ(GetParam().is_first_run, first_run::IsChromeFirstRun());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile = profile_manager->GetLastUsedProfile();
  ASSERT_EQ(profile_manager->GetInitialProfileDir(), profile->GetBaseName());

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

  Profile* profile = profile_manager->GetLastUsedProfile();

  // Whether the profile was signed in with the credential provider or not,
  // reauth should be a noop.
  ASSERT_FALSE(signin_util::ReauthWithCredentialProviderIfPossible(profile));
}

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTest, NoReauthAfterSignout) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile = profile_manager->GetLastUsedProfile();

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
    primary_account_mutator->RevokeSyncConsent(
        signin_metrics::ProfileSignout::kForceSignoutAlwaysAllowedForTest);

    // Even with a refresh token available, no reauth happens if the profile
    // is signed out.
    ASSERT_FALSE(signin_util::ReauthWithCredentialProviderIfPossible(profile));
  }
}

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTest, FixReauth) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile = profile_manager->GetLastUsedProfile();

  if (GetParam().expect_is_started) {
    // Write a new refresh token. This time reauth should work.
    base::win::RegKey key;
    CreateRegKey(&key);
    WriteRefreshToken(&key, "lst-new");
    ASSERT_FALSE(signin_util::ReauthWithCredentialProviderIfPossible(profile));

    // Make sure the profile stays signed in, but in an auth error state.
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    signin::UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_manager,
        identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync),
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
                             /*gaia_id=*/std::wstring(),
                             /*email=*/std::wstring(),
                             /*refresh_token=*/std::string(),
                             /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest2,
                         SigninUtilWinBrowserTest,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/true,
                             /*gaia_id=*/std::wstring(),
                             /*email=*/std::wstring(),
                             /*refresh_token=*/std::string(),
                             /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest3,
                         SigninUtilWinBrowserTest,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/true,
                             /*gaia_id=*/L"gaia-123456",
                             /*email=*/std::wstring(),
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

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest6,
                         SigninUtilWinBrowserTest,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/false,
                             /*gaia_id=*/L"gaia-123456",
                             /*email=*/L"foo@gmail.com",
                             /*refresh_token=*/"lst-123456",
                             /*expect_is_started=*/true)));

struct ExistingWinBrowserSigninUtilTestParams : SigninUtilWinBrowserTestParams {
  ExistingWinBrowserSigninUtilTestParams(
      const std::wstring& gaia_id,
      const std::wstring& email,
      const std::string& refresh_token,
      const std::wstring& existing_email,
      bool expect_is_started)
      : SigninUtilWinBrowserTestParams(false,
                                       gaia_id,
                                       email,
                                       refresh_token,
                                       expect_is_started),
        existing_email(existing_email) {}

  std::wstring existing_email;
};

class ExistingWinBrowserSigninUtilTest
    : public BrowserTestHelper,
      public InProcessBrowserTest,
      public testing::WithParamInterface<
          ExistingWinBrowserSigninUtilTestParams> {
 public:
  ExistingWinBrowserSigninUtilTest()
      : BrowserTestHelper(GetParam().gaia_id,
                          GetParam().email,
                          GetParam().refresh_token) {}

 protected:
  bool SetUpUserDataDirectory() override {
    registry_override_.OverrideRegistry(HKEY_CURRENT_USER);

    signin_util::SetTurnSyncOnHelperDelegateForTesting(
        std::unique_ptr<TurnSyncOnHelper::Delegate>(
            new TestTurnSyncOnHelperDelegate()));
    if (!IsPreTest())
      SetSigninUtilRegistry();

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
};

IN_PROC_BROWSER_TEST_P(ExistingWinBrowserSigninUtilTest,
                       PRE_ExistingWinBrowser) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetLastUsedProfile();

  ASSERT_EQ(profile_manager->GetInitialProfileDir(), profile->GetBaseName());

  if (!GetParam().existing_email.empty()) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

    ASSERT_TRUE(identity_manager);

    signin::MakePrimaryAccountAvailable(
        identity_manager, base::WideToUTF8(GetParam().existing_email),
        signin::ConsentLevel::kSync);

    ASSERT_TRUE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  }
}

IN_PROC_BROWSER_TEST_P(ExistingWinBrowserSigninUtilTest, ExistingWinBrowser) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile = profile_manager->GetLastUsedProfile();
  ASSERT_EQ(profile_manager->GetInitialProfileDir(), profile->GetBaseName());

  AssertSigninStarted(GetParam().expect_is_started, profile);

  // If a refresh token was specified and a sign in attempt was expected, make
  // sure the refresh token was removed from the registry.
  if (!GetParam().refresh_token.empty() && GetParam().expect_is_started)
    ExpectRefreshTokenExists(false);
}

INSTANTIATE_TEST_SUITE_P(AllowSubsequentRun,
                         ExistingWinBrowserSigninUtilTest,
                         testing::Values(ExistingWinBrowserSigninUtilTestParams(
                             /*gaia_id=*/L"gaia-123456",
                             /*email=*/L"foo@gmail.com",
                             /*refresh_token=*/"lst-123456",
                             /*existing_email=*/std::wstring(),
                             /*expect_is_started=*/true)));

INSTANTIATE_TEST_SUITE_P(OnlyAllowProfileWithNoPrimaryAccount,
                         ExistingWinBrowserSigninUtilTest,
                         testing::Values(ExistingWinBrowserSigninUtilTestParams(
                             /*gaia_id=*/L"gaia_id_for_foo_gmail.com",
                             /*email=*/L"foo@gmail.com",
                             /*refresh_token=*/"lst-123456",
                             /*existing_email=*/L"bar@gmail.com",
                             /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(AllowProfileWithPrimaryAccount_DifferentUser,
                         ExistingWinBrowserSigninUtilTest,
                         testing::Values(ExistingWinBrowserSigninUtilTestParams(
                             /*gaia_id=*/L"gaia_id_for_foo_gmail.com",
                             /*email=*/L"foo@gmail.com",
                             /*refresh_token=*/"lst-123456",
                             /*existing_email=*/L"bar@gmail.com",
                             /*expect_is_started=*/false)));


INSTANTIATE_TEST_SUITE_P(AllowProfileWithPrimaryAccount_SameUser,
                         ExistingWinBrowserSigninUtilTest,
                         testing::Values(ExistingWinBrowserSigninUtilTestParams(
                             /*gaia_id=*/L"gaia_id_for_foo_gmail.com",
                             /*email=*/L"foo@gmail.com",
                             /*refresh_token=*/"lst-123456",
                             /*existing_email=*/L"foo@gmail.com",
                             /*expect_is_started=*/true)));

void CreateAndSwitchToProfile(const std::string& basepath) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  base::FilePath path = profile_manager->user_data_dir().AppendASCII(basepath);
  profiles::testing::CreateProfileSync(profile_manager, path);

  profiles::SwitchToProfile(path, false);
}

struct ExistingWinBrowserProfilesSigninUtilTestParams {
  ExistingWinBrowserProfilesSigninUtilTestParams(
      const std::wstring& email_in_other_profile,
      bool cred_provider_used_other_profile,
      const std::wstring& current_profile,
      const std::wstring& email_in_current_profile,
      bool expect_is_started)
      : email_in_other_profile(email_in_other_profile),
        cred_provider_used_other_profile(cred_provider_used_other_profile),
        current_profile(current_profile),
        email_in_current_profile(email_in_current_profile),
        expect_is_started(expect_is_started) {}

  std::wstring email_in_other_profile;
  bool cred_provider_used_other_profile;
  std::wstring current_profile;
  std::wstring email_in_current_profile;
  bool expect_is_started;
};

class ExistingWinBrowserProfilesSigninUtilTest
    : public BrowserTestHelper,
      public InProcessBrowserTest,
      public testing::WithParamInterface<
          ExistingWinBrowserProfilesSigninUtilTestParams> {
 public:
  ExistingWinBrowserProfilesSigninUtilTest()
      : BrowserTestHelper(L"gaia_id_for_foo_gmail.com",
                          L"foo@gmail.com",
                          "lst-123456") {}

 protected:
  bool SetUpUserDataDirectory() override {
    registry_override_.OverrideRegistry(HKEY_CURRENT_USER);

    signin_util::SetTurnSyncOnHelperDelegateForTesting(
        std::unique_ptr<TurnSyncOnHelper::Delegate>(
            new TestTurnSyncOnHelperDelegate()));
    if (!IsPreTest()) {
      SetSigninUtilRegistry();
    } else if (IsPrePreTest() && GetParam().cred_provider_used_other_profile) {
      BrowserTestHelper(L"gaia_id_for_bar_gmail.com", L"bar@gmail.com",
                        "lst-123456")
          .SetSigninUtilRegistry();
    }

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  registry_util::RegistryOverrideManager registry_override_;
};

// In PRE_PRE_Run, browser starts for the first time with the initial profile
// dir. If needed by the test, this step can set |email_in_other_profile| as the
// primary account in the profile or it can sign in with credential provider,
// but before this step ends, |current_profile| is created and browser switches
// to that profile just to prepare the browser for the next step.
IN_PROC_BROWSER_TEST_P(ExistingWinBrowserProfilesSigninUtilTest, PRE_PRE_Run) {
  g_browser_process->local_state()->SetBoolean(
      prefs::kBrowserShowProfilePickerOnStartup, false);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  Profile* profile = profile_manager->GetLastUsedProfile();

  ASSERT_EQ(profile_manager->GetInitialProfileDir(), profile->GetBaseName());

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  ASSERT_TRUE(identity_manager);
  ASSERT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync) ==
      GetParam().cred_provider_used_other_profile);

  if (!GetParam().cred_provider_used_other_profile &&
      !GetParam().email_in_other_profile.empty()) {
    signin::MakePrimaryAccountAvailable(
        identity_manager, base::WideToUTF8(GetParam().email_in_other_profile),
        signin::ConsentLevel::kSync);

    ASSERT_TRUE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  }

  CreateAndSwitchToProfile(base::WideToUTF8(GetParam().current_profile));
}

// Browser starts with the |current_profile| profile created in the previous
// step. If needed by the test, this step can set |email_in_current_profile| as
// the primary account in the profile.
IN_PROC_BROWSER_TEST_P(ExistingWinBrowserProfilesSigninUtilTest, PRE_Run) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetLastUsedProfile();

  ASSERT_EQ(GetParam().current_profile, profile->GetBaseName().value());

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  ASSERT_TRUE(identity_manager);
  ASSERT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));

  if (!GetParam().email_in_current_profile.empty()) {
    signin::MakePrimaryAccountAvailable(
        identity_manager, base::WideToUTF8(GetParam().email_in_current_profile),
        signin::ConsentLevel::kSync);

    ASSERT_TRUE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  }
}

// Before this step runs, refresh token is written into fake registry. Browser
// starts with the |current_profile| profile. Depending on the test case,
// profile may have a primary account. Similarly the other profile(initial
// profile in this case) may have a primary account as well.
IN_PROC_BROWSER_TEST_P(ExistingWinBrowserProfilesSigninUtilTest, Run) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetLastUsedProfile();

  ASSERT_EQ(GetParam().current_profile, profile->GetBaseName().value());
  AssertSigninStarted(GetParam().expect_is_started, profile);
}

INSTANTIATE_TEST_SUITE_P(
    AllowCurrentProfile_NoUserSignedIn,
    ExistingWinBrowserProfilesSigninUtilTest,
    testing::Values(ExistingWinBrowserProfilesSigninUtilTestParams(
        /*email_in_other_profile*/ L"",
        /*cred_provider_used_other_profile*/ false,
        /*current_profile*/ L"profile1",
        /*email_in_current_profile=*/L"",
        /*expect_is_started=*/true)));

INSTANTIATE_TEST_SUITE_P(
    AllowCurrentProfile_SameUserSignedIn,
    ExistingWinBrowserProfilesSigninUtilTest,
    testing::Values(ExistingWinBrowserProfilesSigninUtilTestParams(
        /*email_in_other_profile*/ L"",
        /*cred_provider_used_other_profile*/ false,
        /*current_profile*/ L"profile1",
        /*email_in_current_profile=*/L"foo@gmail.com",
        /*expect_is_started=*/true)));

INSTANTIATE_TEST_SUITE_P(
    DisallowCurrentProfile_DifferentUserSignedIn,
    ExistingWinBrowserProfilesSigninUtilTest,
    testing::Values(ExistingWinBrowserProfilesSigninUtilTestParams(
        /*email_in_other_profile*/ L"",
        /*cred_provider_used_other_profile*/ false,
        /*current_profile*/ L"profile1",
        /*email_in_current_profile=*/L"bar@gmail.com",
        /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(
    DisallowCurrentProfile_SameUserSignedInDefaultProfile,
    ExistingWinBrowserProfilesSigninUtilTest,
    testing::Values(ExistingWinBrowserProfilesSigninUtilTestParams(
        /*email_in_other_profile*/ L"foo@gmail.com",
        /*cred_provider_used_other_profile*/ false,
        /*current_profile*/ L"profile1",
        /*email_in_current_profile=*/L"",
        /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(
    AllowCurrentProfile_DifferentUserSignedInDefaultProfile,
    ExistingWinBrowserProfilesSigninUtilTest,
    testing::Values(ExistingWinBrowserProfilesSigninUtilTestParams(
        /*email_in_other_profile*/ L"bar@gmail.com",
        /*cred_provider_used_other_profile*/ false,
        /*current_profile*/ L"profile1",
        /*email_in_current_profile=*/L"",
        /*expect_is_started=*/true)));

INSTANTIATE_TEST_SUITE_P(
    DisallowCurrentProfile_CredProviderUsedDefaultProfile,
    ExistingWinBrowserProfilesSigninUtilTest,
    testing::Values(ExistingWinBrowserProfilesSigninUtilTestParams(
        /*email_in_other_profile*/ L"",
        /*cred_provider_used_other_profile*/ true,
        /*current_profile*/ L"profile1",
        /*email_in_current_profile=*/L"",
        /*expect_is_started=*/false)));
