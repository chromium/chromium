// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util_win.h"

#include <stddef.h>

#include <memory>

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
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"

class SigninUIError;

namespace {

// Automatically accepts enterprise confirmation screens and turns sync on.
class TestTurnSyncOnHelperDelegate : public TurnSyncOnHelper::Delegate {
  ~TestTurnSyncOnHelperDelegate() override = default;

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

class SigninUtilWinBrowserTestBase : public BrowserTestHelper,
                                     public InProcessBrowserTest {
 public:
  explicit SigninUtilWinBrowserTestBase(SigninUtilWinBrowserTestParams params)
      : BrowserTestHelper(params.gaia_id, params.email, params.refresh_token),
        params_(params) {}

  const SigninUtilWinBrowserTestParams& GetTestParam() { return params_; }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(params_.is_first_run ? switches::kForceFirstRun
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

  SigninUtilWinBrowserTestParams params_;

  base::test::ScopedFeatureList feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
};

// `GetTestParam()` and `GetParam()` are equivalent in this test suite.
class SigninUtilWinBrowserTestWithParams
    : public SigninUtilWinBrowserTestBase,
      public testing::WithParamInterface<SigninUtilWinBrowserTestParams> {
 public:
  SigninUtilWinBrowserTestWithParams()
      : SigninUtilWinBrowserTestBase(GetParam()) {}
};

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTestWithParams, Run) {
  ASSERT_EQ(GetParam().is_first_run, first_run::IsChromeFirstRun());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile = profile_manager->GetLastUsedProfile();
  ASSERT_EQ(ProfileManager::GetInitialProfileDir(), profile->GetBaseName());

  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  ASSERT_NE(nullptr, browser);

  AssertSigninStarted(GetParam().expect_is_started, profile);

  // If a refresh token was specified and a sign in attempt was expected, make
  // sure the refresh token was removed from the registry.
  if (!GetParam().refresh_token.empty() && GetParam().expect_is_started) {
    ExpectRefreshTokenExists(false);
  }
}

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTestWithParams, ReauthNoop) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile = profile_manager->GetLastUsedProfile();

  // Whether the profile was signed in with the credential provider or not,
  // reauth should be a noop.
  ASSERT_FALSE(signin_util::ReauthWithCredentialProviderIfPossible(profile));
}

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTestWithParams,
                       NoReauthAfterSignout) {
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
    primary_account_mutator->ClearPrimaryAccount(
        signin_metrics::ProfileSignout::kForceSignoutAlwaysAllowedForTest);

    // Even with a refresh token available, no reauth happens if the profile
    // is signed out.
    ASSERT_FALSE(signin_util::ReauthWithCredentialProviderIfPossible(profile));
  }
}

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTestWithParams, FixReauth) {
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
        identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_SERVER));

    // If the profile remains signed in but is in an auth error state,
    // reauth should happen.
    ASSERT_TRUE(signin_util::ReauthWithCredentialProviderIfPossible(profile));
  }
}

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest1,
                         SigninUtilWinBrowserTestWithParams,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/false,
                             /*gaia_id=*/std::wstring(),
                             /*email=*/std::wstring(),
                             /*refresh_token=*/std::string(),
                             /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest2,
                         SigninUtilWinBrowserTestWithParams,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/true,
                             /*gaia_id=*/std::wstring(),
                             /*email=*/std::wstring(),
                             /*refresh_token=*/std::string(),
                             /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest3,
                         SigninUtilWinBrowserTestWithParams,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/true,
                             /*gaia_id=*/L"gaia-123456",
                             /*email=*/std::wstring(),
                             /*refresh_token=*/std::string(),
                             /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest4,
                         SigninUtilWinBrowserTestWithParams,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/true,
                             /*gaia_id=*/L"gaia-123456",
                             /*email=*/L"foo@gmail.com",
                             /*refresh_token=*/std::string(),
                             /*expect_is_started=*/false)));

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest5,
                         SigninUtilWinBrowserTestWithParams,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/true,
                             /*gaia_id=*/L"gaia-123456",
                             /*email=*/L"foo@gmail.com",
                             /*refresh_token=*/"lst-123456",
                             /*expect_is_started=*/true)));

INSTANTIATE_TEST_SUITE_P(SigninUtilWinBrowserTest6,
                         SigninUtilWinBrowserTestWithParams,
                         testing::Values(SigninUtilWinBrowserTestParams(
                             /*is_first_run=*/false,
                             /*gaia_id=*/L"gaia-123456",
                             /*email=*/L"foo@gmail.com",
                             /*refresh_token=*/"lst-123456",
                             /*expect_is_started=*/true)));

// The test suite prepares a setup that is closer to the production setup when
// running GCPW. It also allows to control the different steps triggers, so we
// can properly check the states in between.
//
// More specifically in FRE, the test suite ensures this order of events:
// - Test setup with FRE mode and without attempting to create an explicit
// browser. GCPW logic to retrieve the tokens from the registry runs before
// attempting to open a browser. Relies on test keep alives to keep the browser
// process alive.
// - Enforces the creation of the `FirstRunService` on profile creation - which
// test setup does not ensure automatically.
// - Profile is created and reads the account information from the registry.
// - Check the state before opening first browser (that will attempt to run the
// FRE)
// - Can attempt to open the first browser simulating the remainder of flow in
// the test description.
//
// TODO(crbug.com/396696524): Investigate if there is a need to migrate all
// tests in this file that expects to run with FRE mode in order to support the
// proper flow setup. The other setups create an initial browser, do not enforce
// the FRE properly and shows the sync dialog (that signs the user indireclty)
// in the existing browser and accepts sync by default.
class SigninUtilWinNoStartingWindowBrowserTest
    : public SigninUtilWinBrowserTestBase {
 public:
  SigninUtilWinNoStartingWindowBrowserTest()
      : SigninUtilWinBrowserTestBase(SigninUtilWinBrowserTestParams(
            /*is_first_run=*/true,
            /*gaia_id=*/L"gaia-123456",
            /*email=*/L"foo@gmail.com",
            /*refresh_token=*/"lst-123456",
            /*expect_is_started=*/true)),
        keep_alive_(std::make_unique<ScopedKeepAlive>(
            KeepAliveOrigin::APP_CONTROLLER,
            KeepAliveRestartOption::DISABLED)) {}

  bool IsFirstRunFinished() {
    return g_browser_process->local_state()->GetBoolean(
        prefs::kFirstRunFinished);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SigninUtilWinBrowserTestBase::SetUpCommandLine(command_line);
    // No startup windows allows to properly simulate the GCPW FRE flow.
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    SigninUtilWinBrowserTestBase::SetUpDefaultCommandLine(command_line);
    // Remove this default switch to ensure the FRE is attempted properly.
    command_line->RemoveSwitch(switches::kNoFirstRun);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    // By default the service is not created. Enforce it here before attempting
    // to open a browser.
    FirstRunServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating([](content::BrowserContext* browser_context)
                                -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(browser_context);
          return std::make_unique<FirstRunService>(
              *profile, *IdentityManagerFactory::GetForProfile(profile));
        }));
  }

 private:
  // Keeps the browser process running while browsers are closed.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
};

// This test is similar to
// `SigninUtilWinBrowserTest5/SigninUtilWinBrowserTestWithParams.Run` as it runs
// the GCPW flow from FRE with final expectations on being signed in, but has a
// different initial setup which is more aligned with to the production setup
// for the GCPW flow. Along with the test suite setup, it actually tests the
// basic GCPW FRE flow with a proper token set in the registry.
IN_PROC_BROWSER_TEST_F(SigninUtilWinNoStartingWindowBrowserTest,
                       FRESigninFlow) {
  // First run and no browser is active yet.
  ASSERT_TRUE(first_run::IsChromeFirstRun());
  ASSERT_TRUE(BrowserList::GetInstance()->empty());
  ASSERT_FALSE(g_browser_process->IsShuttingDown());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile = profile_manager->GetLastUsedProfile();
  ASSERT_EQ(ProfileManager::GetInitialProfileDir(), profile->GetBaseName());
  ASSERT_EQ(nullptr, chrome::FindLastActiveWithProfile(profile));

  // FRE completion is not set yet since no attempt to open a browser was done
  // yet.
  ASSERT_FALSE(IsFirstRunFinished());
  // Make sure the service exists for a complete test.
  ASSERT_TRUE(FirstRunServiceFactory::GetForBrowserContext(profile));

  // User is expected to be signed in.
  AssertSigninStarted(true, profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Attempt to run the first browser through the startup flow; simulating
  // opening the first browser after GCPW was done processing the refresh token
  // set in the registry. This call is synchronous.
  profiles::FindOrCreateNewWindowForProfile(
      profile, chrome::startup::IsProcessStartup::kYes,
      chrome::startup::IsFirstRun::kYes, /*always_create=*/true);
  Browser* first_browser = chrome::FindLastActiveWithProfile(profile);
  EXPECT_TRUE(first_browser);

  // FRE should be marked as completed because it was bypassed by the already
  // signed in profile without the user seeing the FRE screens.
  EXPECT_FALSE(ProfilePicker::IsFirstRunOpen());
  EXPECT_TRUE(IsFirstRunFinished());

  // Refresh token are expected to be consumed.
  ExpectRefreshTokenExists(false);

  // User is still signed in.
  EXPECT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Cleanup browsers created as part of the flow explicitly.
  CloseAllBrowsers();
}

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

  base::test::ScopedFeatureList feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
};

IN_PROC_BROWSER_TEST_P(ExistingWinBrowserSigninUtilTest,
                       PRE_ExistingWinBrowser) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetLastUsedProfile();

  ASSERT_EQ(ProfileManager::GetInitialProfileDir(), profile->GetBaseName());

  if (!GetParam().existing_email.empty()) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

    ASSERT_TRUE(identity_manager);

    signin::MakePrimaryAccountAvailable(
        identity_manager, base::WideToUTF8(GetParam().existing_email),
        signin::ConsentLevel::kSignin);

    ASSERT_TRUE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  }
}

IN_PROC_BROWSER_TEST_P(ExistingWinBrowserSigninUtilTest, ExistingWinBrowser) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile = profile_manager->GetLastUsedProfile();
  ASSERT_EQ(ProfileManager::GetInitialProfileDir(), profile->GetBaseName());

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
  base::test::ScopedFeatureList feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
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

  ASSERT_EQ(ProfileManager::GetInitialProfileDir(), profile->GetBaseName());

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  ASSERT_TRUE(identity_manager);
  ASSERT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) ==
      GetParam().cred_provider_used_other_profile);

  if (!GetParam().cred_provider_used_other_profile &&
      !GetParam().email_in_other_profile.empty()) {
    signin::MakePrimaryAccountAvailable(
        identity_manager, base::WideToUTF8(GetParam().email_in_other_profile),
        signin::ConsentLevel::kSignin);

    ASSERT_TRUE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
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
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  if (!GetParam().email_in_current_profile.empty()) {
    signin::MakePrimaryAccountAvailable(
        identity_manager, base::WideToUTF8(GetParam().email_in_current_profile),
        signin::ConsentLevel::kSignin);

    ASSERT_TRUE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
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
