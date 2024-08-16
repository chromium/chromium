// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/auth/arc_auth_service.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_data_remover.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/account_manager/account_apps_availability_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_data_removal_dialog.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/auth/arc_auth_context.h"
#include "chrome/browser/ash/arc/auth/arc_background_auth_code_fetcher.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kFakeUserName[] = "test@example.com";
constexpr char kSecondaryAccountEmail[] = "email.111@gmail.com";
constexpr char kFakeAuthCode[] = "fake-auth-code";

std::string GetFakeAuthTokenResponse() {
  return base::StringPrintf(R"({ "token" : "%s"})", kFakeAuthCode);
}

std::unique_ptr<KeyedService> CreateCertificateProviderService(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::CertificateProviderService>();
}

}  // namespace

class TestSettingsWindowManager : public chrome::SettingsWindowManager {
 public:
  void ShowChromePageForProfile(Profile* profile,
                                const GURL& gurl,
                                int64_t display_id,
                                apps::LaunchCallback callback) override {
    last_url_ = gurl;
    if (callback) {
      std::move(callback).Run(apps::LaunchResult(apps::State::kSuccess));
    }
  }

  const GURL& last_url() const { return last_url_; }

 private:
  GURL last_url_;
};

namespace arc {

class FakeAuthInstance : public mojom::AuthInstance {
 public:
  FakeAuthInstance() = default;

  FakeAuthInstance(const FakeAuthInstance&) = delete;
  FakeAuthInstance& operator=(const FakeAuthInstance&) = delete;

  ~FakeAuthInstance() override = default;

  void Init(mojo::PendingRemote<mojom::AuthHost> host_remote,
            InitCallback callback) override {
    // For every change in a connection bind latest remote.
    host_remote_.reset();
    host_remote_.Bind(std::move(host_remote));
    std::move(callback).Run();
  }

  void OnAccountUpdated(const std::string& account_name,
                        mojom::AccountUpdateType update_type) override {
    switch (update_type) {
      case mojom::AccountUpdateType::UPSERT:
        ++num_account_upserted_calls_;
        last_upserted_account_ = account_name;
        break;
      case mojom::AccountUpdateType::REMOVAL:
        ++num_account_removed_calls_;
        last_removed_account_ = account_name;
        break;
    }
  }

  void SetAccounts(std::vector<mojom::ArcAccountInfoPtr> accounts) override {
    ++num_set_accounts_calls_;
    last_set_accounts_list_ = std::move(accounts);
  }

  void RequestPrimaryAccountInfo(base::OnceClosure done_closure) {
    host_remote_->RequestPrimaryAccountInfo(base::BindOnce(
        &FakeAuthInstance::OnPrimaryAccountInfoResponse,
        weak_ptr_factory_.GetWeakPtr(), std::move(done_closure)));
  }

  void RequestAccountInfo(const std::string& account_name,
                          base::OnceClosure done_closure) {
    host_remote_->RequestAccountInfo(
        account_name, base::BindOnce(&FakeAuthInstance::OnAccountInfoResponse,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(done_closure)));
  }

  void GetGoogleAccounts(GetGoogleAccountsCallback callback) override {
    std::vector<mojom::ArcAccountInfoPtr> accounts;
    accounts.emplace_back(mojom::ArcAccountInfo::New(
        kFakeUserName, signin::GetTestGaiaIdForEmail(kFakeUserName)));
    std::move(callback).Run(std::move(accounts));
  }

  void GetMainAccountResolutionStatus(
      GetMainAccountResolutionStatusCallback callback) override {
    std::move(callback).Run(
        mojom::MainAccountResolutionStatus::HASH_CODE_MATCH_SINGLE_ACCOUNT);
  }

  mojom::AccountInfo* account_info() { return account_info_.get(); }

  mojom::ArcAuthCodeStatus auth_code_status() const { return status_; }

  bool sign_in_persistent_error() const { return persistent_error_; }

  int num_account_upserted_calls() const { return num_account_upserted_calls_; }

  std::string last_upserted_account() const { return last_upserted_account_; }

  int num_account_removed_calls() const { return num_account_removed_calls_; }

  std::string last_removed_account() const { return last_removed_account_; }

  int num_set_accounts_calls() const { return num_set_accounts_calls_; }

  const std::vector<mojom::ArcAccountInfoPtr>* last_set_accounts_list() const {
    return &last_set_accounts_list_;
  }

 private:
  void OnPrimaryAccountInfoResponse(base::OnceClosure done_closure,
                                    mojom::ArcAuthCodeStatus status,
                                    mojom::AccountInfoPtr account_info) {
    account_info_ = std::move(account_info);
    status_ = status;
    std::move(done_closure).Run();
  }

  void OnAccountInfoResponse(base::OnceClosure done_closure,
                             mojom::ArcAuthCodeStatus status,
                             mojom::AccountInfoPtr account_info,
                             bool persistent_error) {
    status_ = status;
    account_info_ = std::move(account_info);
    persistent_error_ = persistent_error;
    std::move(done_closure).Run();
  }

  mojo::Remote<mojom::AuthHost> host_remote_;
  mojom::ArcAuthCodeStatus status_;
  bool persistent_error_;
  mojom::AccountInfoPtr account_info_;
  base::OnceClosure done_closure_;

  int num_account_upserted_calls_ = 0;
  std::string last_upserted_account_;
  int num_account_removed_calls_ = 0;
  std::string last_removed_account_;
  int num_set_accounts_calls_ = 0;
  std::vector<mojom::ArcAccountInfoPtr> last_set_accounts_list_;

  base::WeakPtrFactory<FakeAuthInstance> weak_ptr_factory_{this};
};

// Set account availability in ARC by gaia id.
class AccountAppsAvailabilitySetter {
 public:
  AccountAppsAvailabilitySetter(
      ash::AccountAppsAvailability* account_apps_availability,
      account_manager::AccountManagerFacade* account_manager_facade)
      : account_apps_availability_(account_apps_availability),
        account_manager_facade_(account_manager_facade) {}

  AccountAppsAvailabilitySetter(const AccountAppsAvailabilitySetter&) = delete;
  AccountAppsAvailabilitySetter& operator=(
      const AccountAppsAvailabilitySetter&) = delete;

  ~AccountAppsAvailabilitySetter() = default;

  // Returns `true` if account with `gaia_id` was found in AccountManager and
  // `SetIsAccountAvailableInArc` for this account was called. Returns `false`
  // otherwise.
  bool SetIsAccountAvailableInArc(const std::string& gaia_id,
                                  bool is_available) {
    std::vector<account_manager::Account> result;
    base::RunLoop run_loop;
    account_manager_facade_->GetAccounts(base::BindLambdaForTesting(
        [&result,
         &run_loop](const std::vector<account_manager::Account>& accounts) {
          result = accounts;
          run_loop.Quit();
        }));
    run_loop.Run();

    for (auto account : result) {
      if (account.key.id() == gaia_id) {
        account_apps_availability_->SetIsAccountAvailableInArc(account,
                                                               is_available);
        return true;
      }
    }

    return false;
  }

  const raw_ptr<ash::AccountAppsAvailability> account_apps_availability_;
  const raw_ptr<account_manager::AccountManagerFacade> account_manager_facade_;
};

class ArcAuthServiceTest : public InProcessBrowserTest,
                           public ::testing::WithParamInterface<bool> {
 public:
  ArcAuthServiceTest(const ArcAuthServiceTest&) = delete;
  ArcAuthServiceTest& operator=(const ArcAuthServiceTest&) = delete;

 protected:
  ArcAuthServiceTest() = default;

  // InProcessBrowserTest:
  ~ArcAuthServiceTest() override = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> lacros =
        ash::standalone_browser::GetFeatureRefs();
    lacros.push_back(
        ash::standalone_browser::features::kLacrosForSupervisedUsers);
    lacros.push_back(
        ash::features::kSecondaryAccountAllowedInArcPolicy);
    if (IsArcAccountRestrictionsEnabled()) {
      feature_list_.InitWithFeatures(lacros, {});
      scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
          ash::switches::kEnableLacrosForTesting);
    } else {
      feature_list_.InitWithFeatures({}, lacros);
    }
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    // Init ArcSessionManager for testing.
    ArcServiceLauncher::Get()->ResetForTesting();
    ArcSessionManager::SetUiEnabledForTesting(false);
    ArcSessionManager::EnableCheckAndroidManagementForTesting(true);
    ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
        std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    ExpandPropertyFilesForTesting(ArcSessionManager::Get());

    settings_window_manager_ = std::make_unique<TestSettingsWindowManager>();
    chrome::SettingsWindowManager::SetInstanceForTesting(
        settings_window_manager_.get());
  }

  void TearDownOnMainThread() override {
    if (arc_bridge_service_) {
      arc_bridge_service_->auth()->CloseInstance(&auth_instance_);
    }

    // Explicitly removing the user is required; otherwise ProfileHelper keeps
    // a dangling pointer to the User.
    // TODO(nya): Consider removing all users from ProfileHelper in the
    // destructor of ash::FakeChromeUserManager.
    auto* user = fake_user_manager_->GetActiveUser();
    if (user) {
      fake_user_manager_->RemoveUserFromList(
          fake_user_manager_->GetActiveUser()->GetAccountId());
    }
    // Since ArcServiceLauncher is (re-)set up with profile() in
    // SetUpOnMainThread() it is necessary to Shutdown() before the profile()
    // is destroyed. ArcServiceLauncher::Shutdown() will be called again on
    // fixture destruction (because it is initialized with the original Profile
    // instance in fixture, once), but it should be no op.
    // TODO(hidehiko): Think about a way to test the code cleanly.
    ArcServiceLauncher::Get()->Shutdown();
    if (IsArcAccountRestrictionsEnabled()) {
      arc_availability_setter_.reset();
    }
    identity_test_environment_adaptor_.reset();
    profile_.reset();
    fake_user_manager_.Reset();

    chrome::SettingsWindowManager::SetInstanceForTesting(nullptr);
    settings_window_manager_.reset();
  }

  void EnableRemovalOfExtendedAccountInfo() {
    identity_test_environment_adaptor_->identity_test_env()
        ->EnableRemovalOfExtendedAccountInfo();
  }

  void SetAccountAndProfile(const user_manager::UserType user_type) {
    AccountId account_id = AccountId::FromUserEmailGaiaId(
        kFakeUserName, signin::GetTestGaiaIdForEmail(kFakeUserName));
    const user_manager::User* user = nullptr;
    switch (user_type) {
      case user_manager::UserType::kChild:
        user = fake_user_manager_->AddChildUser(account_id);
        break;
      case user_manager::UserType::kRegular:
        user = fake_user_manager_->AddUser(account_id);
        break;
      case user_manager::UserType::kPublicAccount:
        user = fake_user_manager_->AddPublicAccountUser(account_id);
        break;
      default:
        ADD_FAILURE() << "Unexpected user type " << user_type;
        return;
    }

    fake_user_manager_->LoginUser(account_id,
                                  /*set_profile_created_flag=*/false);

    // Create test profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_builder.SetProfileName(kFakeUserName);
    if (user_type == user_manager::UserType::kChild) {
      profile_builder.SetIsSupervisedProfile();
    }

    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(profile_builder);
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, profile_.get());
    fake_user_manager_->SimulateUserProfileLoad(account_id);

    auto* identity_test_env =
        identity_test_environment_adaptor_->identity_test_env();
    identity_test_env->SetAutomaticIssueOfAccessTokens(true);
    // Use ConsentLevel::kSignin because ARC doesn't care about browser sync
    // consent.
    identity_test_env->MakePrimaryAccountAvailable(
        kFakeUserName, signin::ConsentLevel::kSignin);
    // Wait for all callbacks to complete, so that they are not called during
    // the test execution.
    base::RunLoop().RunUntilIdle();

    profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);
    profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
    MigrateSigninScopedDeviceId(profile());

    // TestingProfile is not interpreted as a primary profile. Inject factory so
    // that the instance of CertificateProviderService for the profile can be
    // created.
    chromeos::CertificateProviderServiceFactory::GetInstance()
        ->SetTestingFactory(
            profile(), base::BindRepeating(&CreateCertificateProviderService));

    ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(profile());

    auth_service_ = ArcAuthService::GetForBrowserContext(profile());
    DCHECK(auth_service_);

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    auth_service_->SetURLLoaderFactoryForTesting(test_shared_loader_factory_);
    if (IsArcAccountRestrictionsEnabled()) {
      arc_availability_setter_ =
          std::make_unique<AccountAppsAvailabilitySetter>(
              ash::AccountAppsAvailabilityFactory::GetForProfile(profile()),
              ::GetAccountManagerFacade(profile()->GetPath().value()));
    }
    arc_bridge_service_ = ArcServiceManager::Get()->arc_bridge_service();
    DCHECK(arc_bridge_service_);
    arc_bridge_service_->auth()->SetInstance(&auth_instance_);
    WaitForInstanceReady(arc_bridge_service_->auth());
    // Waiting for users and profiles to be setup.
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(user_manager::UserManager::Get()->IsPrimaryUser(
        ash::ProfileHelper::Get()->GetUserByProfile(profile())));
    ASSERT_EQ(IsArcAccountRestrictionsEnabled(),
              ash::AccountAppsAvailability::IsArcAccountRestrictionsEnabled());
  }

  bool SetIsAccountAvailableInArc(std::string gaia, bool is_available) {
    DCHECK(arc_availability_setter_);
    return arc_availability_setter_->SetIsAccountAvailableInArc(gaia,
                                                                is_available);
  }

  AccountInfo SeedAccountInfo(const std::string& email,
                              bool make_available_in_arc = true) {
    auto account_info = identity_test_environment_adaptor_->identity_test_env()
                            ->MakeAccountAvailable(email);
    // Wait for async calls to finish.
    base::RunLoop().RunUntilIdle();
    if (IsArcAccountRestrictionsEnabled() && make_available_in_arc) {
      EXPECT_TRUE(
          SetIsAccountAvailableInArc(account_info.gaia, make_available_in_arc));
    }
    return account_info;
  }

  void SetInvalidRefreshTokenForAccount(const CoreAccountId& account_id) {
    identity_test_environment_adaptor_->identity_test_env()
        ->SetInvalidRefreshTokenForAccount(account_id);
    // Wait for async calls to finish.
    base::RunLoop().RunUntilIdle();
  }

  void SetRefreshTokenForAccount(const CoreAccountId& account_id) {
    identity_test_environment_adaptor_->identity_test_env()
        ->SetRefreshTokenForAccount(account_id);
    // Wait for async calls to finish.
    base::RunLoop().RunUntilIdle();
  }

  void RemoveRefreshTokenForAccount(const CoreAccountId& account_id) {
    identity_test_environment_adaptor_->identity_test_env()
        ->RemoveRefreshTokenForAccount(account_id);
    // Wait for async calls to finish.
    base::RunLoop().RunUntilIdle();
  }

  void UpdatePersistentErrorOfRefreshTokenForAccount(
      const CoreAccountId& account_id,
      const GoogleServiceAuthError& error) {
    identity_test_environment_adaptor_->identity_test_env()
        ->UpdatePersistentErrorOfRefreshTokenForAccount(account_id, error);
  }

  void RequestGoogleAccountsInArc() {
    arc_google_accounts_.clear();
    arc_google_accounts_callback_called_ = false;
    run_loop_ = std::make_unique<base::RunLoop>();

    ArcAuthService::GetGoogleAccountsInArcCallback callback = base::BindOnce(
        [](std::vector<mojom::ArcAccountInfoPtr>* accounts,
           bool* arc_google_accounts_callback_called,
           base::OnceClosure quit_closure,
           std::vector<mojom::ArcAccountInfoPtr> returned_accounts) {
          *accounts = std::move(returned_accounts);
          *arc_google_accounts_callback_called = true;
          std::move(quit_closure).Run();
        },
        &arc_google_accounts_, &arc_google_accounts_callback_called_,
        run_loop_->QuitClosure());

    auth_service().GetGoogleAccountsInArc(std::move(callback));
  }

  AccountInfo SetupGaiaAccount(const std::string& email,
                               bool make_available_in_arc = true) {
    SetAccountAndProfile(user_manager::UserType::kRegular);
    return SeedAccountInfo(email, make_available_in_arc);
  }

  void WaitForGoogleAccountsInArcCallback() { run_loop_->RunUntilIdle(); }

  std::pair<std::string, mojom::ChromeAccountType> RequestPrimaryAccount() {
    base::RunLoop run_loop;
    std::string account_name;
    mojom::ChromeAccountType account_type = mojom::ChromeAccountType::UNKNOWN;
    base::OnceCallback<void(const std::string&, mojom::ChromeAccountType)>
        callback = base::BindLambdaForTesting(
            [&run_loop, &account_name, &account_type](
                const std::string& returned_account_name,
                mojom::ChromeAccountType returned_account_type) {
              account_name = returned_account_name;
              account_type = returned_account_type;
              run_loop.Quit();
            });

    auth_service().RequestPrimaryAccount(std::move(callback));
    run_loop.Run();

    return std::make_pair(account_name, account_type);
  }

  void OnArcInitialStart() { auth_service().OnArcInitialStart(); }

  bool IsArcAccountRestrictionsEnabled() { return GetParam(); }

  Profile* profile() { return profile_.get(); }

  void set_profile_name(const std::string& username) {
    profile_->set_profile_name(username);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }
  ArcAuthService& auth_service() { return *auth_service_; }
  FakeAuthInstance& auth_instance() { return auth_instance_; }
  ArcBridgeService& arc_bridge_service() { return *arc_bridge_service_; }
  const std::vector<mojom::ArcAccountInfoPtr>& arc_google_accounts() const {
    return arc_google_accounts_;
  }
  bool arc_google_accounts_callback_called() const {
    return arc_google_accounts_callback_called_;
  }

  TestSettingsWindowManager& settings_window_manager() {
    return *settings_window_manager_;
  }

 private:
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  FakeAuthInstance auth_instance_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;

  std::vector<mojom::ArcAccountInfoPtr> arc_google_accounts_;
  bool arc_google_accounts_callback_called_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<AccountAppsAvailabilitySetter> arc_availability_setter_;
  std::unique_ptr<TestSettingsWindowManager> settings_window_manager_;
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;

  // Not owned.
  raw_ptr<ArcAuthService, DanglingUntriaged> auth_service_ = nullptr;
  raw_ptr<ArcBridgeService, DanglingUntriaged> arc_bridge_service_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, GetPrimaryAccountForGaiaAccounts) {
  SetAccountAndProfile(user_manager::UserType::kRegular);
  const std::pair<std::string, mojom::ChromeAccountType> primary_account =
      RequestPrimaryAccount();
  EXPECT_EQ(kFakeUserName, primary_account.first);
  EXPECT_EQ(mojom::ChromeAccountType::USER_ACCOUNT, primary_account.second);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, GetPrimaryAccountForChildAccounts) {
  SetAccountAndProfile(user_manager::UserType::kChild);
  const std::pair<std::string, mojom::ChromeAccountType> primary_account =
      RequestPrimaryAccount();
  EXPECT_EQ(kFakeUserName, primary_account.first);
  EXPECT_EQ(mojom::ChromeAccountType::CHILD_ACCOUNT, primary_account.second);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, GetPrimaryAccountForPublicAccounts) {
  SetAccountAndProfile(user_manager::UserType::kPublicAccount);
  const std::pair<std::string, mojom::ChromeAccountType> primary_account =
      RequestPrimaryAccount();
  EXPECT_EQ(std::string(), primary_account.first);
  EXPECT_EQ(mojom::ChromeAccountType::ROBOT_ACCOUNT, primary_account.second);
}

// Tests that when ARC requests account info for a non-managed account,
// Chrome supplies the info configured in SetAccountAndProfile() method.
IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, SuccessfulBackgroundFetch) {
  SetAccountAndProfile(user_manager::UserType::kRegular);
  test_url_loader_factory()->AddResponse(arc::kTokenBootstrapEndPoint,
                                         GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::USER_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

// Tests that the `ArcBackgroundAuthCodeFetcher` will retry the network request
// which fetches the auth code to be used for Google Play Store sign-in if the
// request has failed because of a unreachable mandatory PAC script.
IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, SuccessfulBackgroundProxyBypass) {
  SetAccountAndProfile(user_manager::UserType::kRegular);
  int requests_count = 0;
  test_url_loader_factory()->SetInterceptor(base::BindLambdaForTesting(
      [&requests_count, this](const network::ResourceRequest& request) {
        network::URLLoaderCompletionStatus status(
            net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED);
        switch (requests_count) {
          case 0:
            // Reply with broken PAC script state.
            test_url_loader_factory()->AddResponse(
                GURL(arc::kTokenBootstrapEndPoint),
                network::mojom::URLResponseHead::New(), "response", status);
            break;
          case 1:
            // Reply with the auth token.
            test_url_loader_factory()->AddResponse(arc::kTokenBootstrapEndPoint,
                                                   GetFakeAuthTokenResponse());
            break;
          default:
            NOTREACHED_IN_MIGRATION();
        }
        requests_count++;
      }));
  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  // Expect two network requests to have happened: the first one which failed
  // because the mandatory PAC script is unreachable and the second request
  // which bypassed the proxy and succeeded.
  EXPECT_EQ(2, requests_count);

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::USER_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       ReAuthenticatePrimaryAccountSucceeds) {
  base::HistogramTester tester;
  SetAccountAndProfile(user_manager::UserType::kRegular);
  test_url_loader_factory()->AddResponse(arc::kTokenBootstrapEndPoint,
                                         GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kFakeUserName, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::USER_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
  EXPECT_FALSE(auth_instance().sign_in_persistent_error());
  tester.ExpectUniqueSample(
      kArcAuthRequestAccountInfoResultPrimaryHistogramName,
      mojom::ArcAuthCodeStatus::SUCCESS, 1);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       RetryAuthTokenExchangeRequestOnUnauthorizedError) {
  base::HistogramTester tester;
  SetAccountAndProfile(user_manager::UserType::kRegular);

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kFakeUserName, run_loop.QuitClosure());

  EXPECT_TRUE(
      test_url_loader_factory()->IsPending(arc::kTokenBootstrapEndPoint));
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      arc::kTokenBootstrapEndPoint, std::string(), net::HTTP_UNAUTHORIZED);

  // Should retry auth token exchange request
  EXPECT_TRUE(
      test_url_loader_factory()->IsPending(arc::kTokenBootstrapEndPoint));
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      arc::kTokenBootstrapEndPoint, GetFakeAuthTokenResponse());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  tester.ExpectUniqueSample(
      kArcAuthRequestAccountInfoResultPrimaryHistogramName,
      mojom::ArcAuthCodeStatus::SUCCESS, 1);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       ReAuthenticatePrimaryAccountFailsForInvalidAccount) {
  base::HistogramTester tester;
  SetAccountAndProfile(user_manager::UserType::kRegular);
  test_url_loader_factory()->AddResponse(arc::kTokenBootstrapEndPoint,
                                         std::string() /* response */,
                                         net::HTTP_UNAUTHORIZED);

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kFakeUserName, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(auth_instance().account_info());
  EXPECT_EQ(mojom::ArcAuthCodeStatus::CHROME_SERVER_COMMUNICATION_ERROR,
            auth_instance().auth_code_status());
  tester.ExpectUniqueSample(
      kArcAuthRequestAccountInfoResultPrimaryHistogramName,
      mojom::ArcAuthCodeStatus::CHROME_SERVER_COMMUNICATION_ERROR, 1);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, FetchSecondaryAccountInfoSucceeds) {
  base::HistogramTester tester;
  // Add a Secondary Account.
  SetAccountAndProfile(user_manager::UserType::kRegular);
  SeedAccountInfo(kSecondaryAccountEmail);
  test_url_loader_factory()->AddResponse(arc::kTokenBootstrapEndPoint,
                                         GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kSecondaryAccountEmail,
                                     run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kSecondaryAccountEmail,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::USER_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
  EXPECT_FALSE(auth_instance().sign_in_persistent_error());
  tester.ExpectUniqueSample(
      kArcAuthRequestAccountInfoResultSecondaryHistogramName,
      mojom::ArcAuthCodeStatus::SUCCESS, 1);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       FetchSecondaryAccountInfoFailsForInvalidAccounts) {
  base::HistogramTester tester;
  // Add a Secondary Account.
  SetAccountAndProfile(user_manager::UserType::kRegular);
  SeedAccountInfo(kSecondaryAccountEmail);
  test_url_loader_factory()->AddResponse(arc::kTokenBootstrapEndPoint,
                                         std::string() /* response */,
                                         net::HTTP_UNAUTHORIZED);

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kSecondaryAccountEmail,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(auth_instance().account_info());
  EXPECT_EQ(mojom::ArcAuthCodeStatus::CHROME_SERVER_COMMUNICATION_ERROR,
            auth_instance().auth_code_status());
  tester.ExpectUniqueSample(
      kArcAuthRequestAccountInfoResultSecondaryHistogramName,
      mojom::ArcAuthCodeStatus::CHROME_SERVER_COMMUNICATION_ERROR, 1);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       FetchSecondaryAccountInfoInvalidRefreshToken) {
  base::HistogramTester tester;
  const AccountInfo account_info = SetupGaiaAccount(kSecondaryAccountEmail);
  SetInvalidRefreshTokenForAccount(account_info.account_id);
  test_url_loader_factory()->AddResponse(arc::kTokenBootstrapEndPoint,
                                         std::string() /* response */,
                                         net::HTTP_UNAUTHORIZED);

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kSecondaryAccountEmail,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(auth_instance().account_info());
  EXPECT_EQ(mojom::ArcAuthCodeStatus::CHROME_SERVER_COMMUNICATION_ERROR,
            auth_instance().auth_code_status());
  EXPECT_TRUE(auth_instance().sign_in_persistent_error());
  tester.ExpectUniqueSample(
      kArcAuthRequestAccountInfoResultSecondaryHistogramName,
      mojom::ArcAuthCodeStatus::CHROME_SERVER_COMMUNICATION_ERROR, 1);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       FetchSecondaryAccountRefreshTokenHasPersistentError) {
  base::HistogramTester tester;
  const AccountInfo account_info = SetupGaiaAccount(kSecondaryAccountEmail);
  UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kSecondaryAccountEmail,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(auth_instance().account_info());
  EXPECT_EQ(mojom::ArcAuthCodeStatus::CHROME_SERVER_COMMUNICATION_ERROR,
            auth_instance().auth_code_status());
  EXPECT_TRUE(auth_instance().sign_in_persistent_error());
  tester.ExpectUniqueSample(
      kArcAuthRequestAccountInfoResultSecondaryHistogramName,
      mojom::ArcAuthCodeStatus::CHROME_SERVER_COMMUNICATION_ERROR, 1);
}

IN_PROC_BROWSER_TEST_P(
    ArcAuthServiceTest,
    FetchSecondaryAccountInfoReturnsErrorForNotFoundAccounts) {
  base::HistogramTester tester;
  SetAccountAndProfile(user_manager::UserType::kRegular);
  // Don't add account with kSecondaryAccountEmail.

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kSecondaryAccountEmail,
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(auth_instance().account_info());
  EXPECT_EQ(mojom::ArcAuthCodeStatus::CHROME_ACCOUNT_NOT_FOUND,
            auth_instance().auth_code_status());
  EXPECT_TRUE(auth_instance().sign_in_persistent_error());
  tester.ExpectUniqueSample(
      kArcAuthRequestAccountInfoResultSecondaryHistogramName,
      mojom::ArcAuthCodeStatus::CHROME_ACCOUNT_NOT_FOUND, 1);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, FetchGoogleAccountsFromArc) {
  SetAccountAndProfile(user_manager::UserType::kRegular);

  EXPECT_FALSE(arc_google_accounts_callback_called());
  RequestGoogleAccountsInArc();
  WaitForGoogleAccountsInArcCallback();

  EXPECT_TRUE(arc_google_accounts_callback_called());
  ASSERT_EQ(1u, arc_google_accounts().size());
  EXPECT_EQ(kFakeUserName, arc_google_accounts()[0]->email);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kFakeUserName),
            arc_google_accounts()[0]->gaia_id);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       FetchGoogleAccountsFromArcWorksAcrossConnectionResets) {
  SetAccountAndProfile(user_manager::UserType::kRegular);

  // Close the connection.
  arc_bridge_service().auth()->CloseInstance(&auth_instance());
  // Make a request.
  EXPECT_FALSE(arc_google_accounts_callback_called());
  RequestGoogleAccountsInArc();
  WaitForGoogleAccountsInArcCallback();
  // Callback should not be called before connection is restarted.
  EXPECT_FALSE(arc_google_accounts_callback_called());
  // Restart the connection.
  arc_bridge_service().auth()->SetInstance(&auth_instance());
  WaitForInstanceReady(arc_bridge_service().auth());

  EXPECT_TRUE(arc_google_accounts_callback_called());
  ASSERT_EQ(1u, arc_google_accounts().size());
  EXPECT_EQ(kFakeUserName, arc_google_accounts()[0]->email);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kFakeUserName),
            arc_google_accounts()[0]->gaia_id);
}

IN_PROC_BROWSER_TEST_P(
    ArcAuthServiceTest,
    PrimaryAccountReauthIsNotAttemptedJustAfterProvisioning) {
  SetAccountAndProfile(user_manager::UserType::kRegular);
  const int initial_num_account_upserted_calls =
      auth_instance().num_account_upserted_calls();
  const int initial_num_set_accounts_calls =
      auth_instance().num_set_accounts_calls();
  // Our test setup manually sets the device as provisioned and invokes
  // |ArcAuthService::OnConnectionReady|. Hence, we would have received an
  // update for the Primary Account.
  if (IsArcAccountRestrictionsEnabled()) {
    // 1 SetAccounts() call for the Primary account.
    EXPECT_EQ(1, initial_num_set_accounts_calls);
    EXPECT_EQ(1u, auth_instance().last_set_accounts_list()->size());
    EXPECT_EQ(kFakeUserName,
              (*auth_instance().last_set_accounts_list())[0]->email);
    EXPECT_EQ(0, initial_num_account_upserted_calls);
  } else {
    EXPECT_EQ(1, initial_num_account_upserted_calls);
  }

  // Simulate ARC first time provisioning call.
  OnArcInitialStart();
  EXPECT_EQ(initial_num_account_upserted_calls,
            auth_instance().num_account_upserted_calls());
  EXPECT_EQ(initial_num_set_accounts_calls,
            auth_instance().num_set_accounts_calls());
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       UnAuthenticatedAccountsAreNotPropagated) {
  const AccountInfo account_info = SetupGaiaAccount(kSecondaryAccountEmail);

  const int initial_num_calls = auth_instance().num_account_upserted_calls();
  if (IsArcAccountRestrictionsEnabled()) {
    // 1 SetAccounts() call for the Primary account.
    EXPECT_EQ(1, auth_instance().num_set_accounts_calls());
    EXPECT_EQ(1u, auth_instance().last_set_accounts_list()->size());
    EXPECT_EQ(kFakeUserName,
              (*auth_instance().last_set_accounts_list())[0]->email);
    // 1 call for the Secondary Account.
    EXPECT_EQ(1, auth_instance().num_account_upserted_calls());
  } else {
    // 2 calls: 1 for the Primary Account and 1 for the Secondary Account.
    EXPECT_EQ(2, auth_instance().num_account_upserted_calls());
  }

  SetInvalidRefreshTokenForAccount(account_info.account_id);
  EXPECT_EQ(initial_num_calls, auth_instance().num_account_upserted_calls());
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, AccountUpdatesArePropagated) {
  AccountInfo account_info = SetupGaiaAccount(kSecondaryAccountEmail);

  SetInvalidRefreshTokenForAccount(account_info.account_id);
  const int initial_num_calls = auth_instance().num_account_upserted_calls();
  if (IsArcAccountRestrictionsEnabled()) {
    // 1 SetAccounts() call for the Primary account.
    EXPECT_EQ(1, auth_instance().num_set_accounts_calls());
    EXPECT_EQ(1u, auth_instance().last_set_accounts_list()->size());
    EXPECT_EQ(kFakeUserName,
              (*auth_instance().last_set_accounts_list())[0]->email);
    // 1 call for the Secondary Account.
    EXPECT_EQ(1, initial_num_calls);
  } else {
    // 2 calls: 1 for the Primary Account and 1 for the Secondary Account.
    EXPECT_EQ(2, initial_num_calls);
  }
  SetRefreshTokenForAccount(account_info.account_id);
  // Expect exactly one call for the account update above.
  EXPECT_EQ(1,
            auth_instance().num_account_upserted_calls() - initial_num_calls);
  EXPECT_EQ(kSecondaryAccountEmail, auth_instance().last_upserted_account());
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       AccountUpdatesAreNotPropagatedIfAccountIsNotAvailable) {
  if (!IsArcAccountRestrictionsEnabled()) {
    return;
  }

  AccountInfo account_info = SetupGaiaAccount(kSecondaryAccountEmail);

  SetInvalidRefreshTokenForAccount(account_info.account_id);
  const int initial_num_calls = auth_instance().num_account_upserted_calls();
  // 1 SetAccounts() call for the Primary account.
  EXPECT_EQ(1, auth_instance().num_set_accounts_calls());
  EXPECT_EQ(1u, auth_instance().last_set_accounts_list()->size());
  EXPECT_EQ(kFakeUserName,
            (*auth_instance().last_set_accounts_list())[0]->email);
  // 1 call for the Secondary Account.
  EXPECT_EQ(1, initial_num_calls);

  EXPECT_TRUE(SetIsAccountAvailableInArc(account_info.gaia,
                                         /*make_available_in_arc=*/false));
  // Wait for async calls to finish.
  base::RunLoop().RunUntilIdle();
  // Expect one call for the account update above.
  EXPECT_EQ(1, auth_instance().num_account_removed_calls());

  SetRefreshTokenForAccount(account_info.account_id);
  // Expect zero calls for the account update above.
  EXPECT_EQ(0,
            auth_instance().num_account_upserted_calls() - initial_num_calls);
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, AccountRemovalsArePropagated) {
  SetAccountAndProfile(user_manager::UserType::kRegular);
  SeedAccountInfo(kSecondaryAccountEmail);

  EXPECT_EQ(0, auth_instance().num_account_removed_calls());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByEmailAddress(
          kSecondaryAccountEmail);
  ASSERT_TRUE(!account_info.IsEmpty());

  // Necessary to ensure that the OnExtendedAccountInfoRemoved() observer will
  // be sent.
  EnableRemovalOfExtendedAccountInfo();

  RemoveRefreshTokenForAccount(account_info.account_id);

  EXPECT_EQ(1, auth_instance().num_account_removed_calls());
  EXPECT_EQ(kSecondaryAccountEmail, auth_instance().last_removed_account());
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       AccountRemovalsAreNotPropagatedIfAccountIsNotAvailable) {
  if (!IsArcAccountRestrictionsEnabled()) {
    return;
  }

  SetAccountAndProfile(user_manager::UserType::kRegular);
  SeedAccountInfo(kSecondaryAccountEmail);

  EXPECT_EQ(0, auth_instance().num_account_removed_calls());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByEmailAddress(
          kSecondaryAccountEmail);
  ASSERT_TRUE(!account_info.IsEmpty());

  EXPECT_TRUE(SetIsAccountAvailableInArc(account_info.gaia,
                                         /*make_available_in_arc=*/false));

  // Wait for async calls to finish.
  base::RunLoop().RunUntilIdle();
  // Expect one call for the account update above.
  EXPECT_EQ(1, auth_instance().num_account_removed_calls());
  const int last_num_calls = auth_instance().num_account_removed_calls();

  // Necessary to ensure that the OnExtendedAccountInfoRemoved() observer will
  // be sent.
  EnableRemovalOfExtendedAccountInfo();

  RemoveRefreshTokenForAccount(account_info.account_id);

  // Expect zero calls for the account removal above.
  EXPECT_EQ(0, auth_instance().num_account_removed_calls() - last_num_calls);
}

class ArcRobotAccountAuthServiceTest : public ArcAuthServiceTest {
 public:
  ArcRobotAccountAuthServiceTest() = default;

  ArcRobotAccountAuthServiceTest(const ArcRobotAccountAuthServiceTest&) =
      delete;
  ArcRobotAccountAuthServiceTest& operator=(
      const ArcRobotAccountAuthServiceTest&) = delete;

  ~ArcRobotAccountAuthServiceTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    "http://localhost");
    ArcAuthServiceTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ArcAuthServiceTest::SetUpOnMainThread();
    SetUpPolicyClient();
  }

  void TearDownOnMainThread() override {
    ArcAuthServiceTest::TearDownOnMainThread();
  }

 protected:
  void ResponseJob(const network::ResourceRequest& request,
                   network::TestURLLoaderFactory* factory) {
    enterprise_management::DeviceManagementResponse response;
    response.mutable_service_api_access_response()->set_auth_code(
        kFakeAuthCode);

    std::string response_data;
    EXPECT_TRUE(response.SerializeToString(&response_data));

    factory->AddResponse(request.url.spec(), response_data);
  }

 private:
  void SetUpPolicyClient() {
    policy::BrowserPolicyConnectorAsh* const connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    policy::DeviceCloudPolicyManagerAsh* const cloud_policy_manager =
        connector->GetDeviceCloudPolicyManager();

    cloud_policy_manager->StartConnection(
        std::make_unique<policy::MockCloudPolicyClient>(),
        connector->GetInstallAttributes());

    policy::MockCloudPolicyClient* const cloud_policy_client =
        static_cast<policy::MockCloudPolicyClient*>(
            cloud_policy_manager->core()->client());
    cloud_policy_client->SetDMToken("fake-dm-token");
    cloud_policy_client->client_id_ = "client-id";
  }
};

// Tests that when ARC requests account info for a demo session account,
// Chrome supplies the info configured in SetAccountAndProfile() above.
// TODO(crbug.com/355199222): Flaky test
IN_PROC_BROWSER_TEST_P(ArcRobotAccountAuthServiceTest,
                       DISABLED_GetDemoAccount) {
  ash::DemoSession::SetDemoConfigForTesting(
      ash::DemoSession::DemoModeConfig::kOnline);
  ash::test::LockDemoDeviceInstallAttributes();
  ash::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::UserType::kPublicAccount);

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ResponseJob(request, test_url_loader_factory());
      }));

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::ROBOT_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

// TODO(crbug.com/354131115): Flaky test
IN_PROC_BROWSER_TEST_P(ArcRobotAccountAuthServiceTest,
                       DISABLED_GetDemoAccountOnAuthTokenFetchFailure) {
  ash::DemoSession::SetDemoConfigForTesting(
      ash::DemoSession::DemoModeConfig::kOnline);
  ash::test::LockDemoDeviceInstallAttributes();
  ash::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::UserType::kPublicAccount);

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        test_url_loader_factory()->AddResponse(
            request.url.spec(), std::string(), net::HTTP_NOT_FOUND);
      }));

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_TRUE(auth_instance().account_info()->auth_code.value().empty());
  EXPECT_EQ(mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_P(ArcRobotAccountAuthServiceTest,
                       GetDemoAccountWithOfflineFlag) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDemoModeForceArcOfflineProvision);

  ash::DemoSession::SetDemoConfigForTesting(
      ash::DemoSession::DemoModeConfig::kOnline);
  ash::test::LockDemoDeviceInstallAttributes();
  ash::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::UserType::kPublicAccount);

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ResponseJob(request, test_url_loader_factory());
      }));

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_TRUE(auth_instance().account_info()->auth_code.value().empty());
  EXPECT_EQ(mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
}

// TODO(crbug.com/352951605): Flaky test
IN_PROC_BROWSER_TEST_P(ArcRobotAccountAuthServiceTest,
                       DISABLED_RequestPublicAccountInfo) {
  SetAccountAndProfile(user_manager::UserType::kPublicAccount);
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ResponseJob(request, test_url_loader_factory());
      }));

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfo(kFakeUserName, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::ROBOT_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
  EXPECT_FALSE(auth_instance().sign_in_persistent_error());
}

// Tests that when ARC requests account info for a child account and
// Chrome supplies the info configured in SetAccountAndProfile() above.
IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, ChildAccountFetch) {
  SetAccountAndProfile(user_manager::UserType::kChild);
  EXPECT_TRUE(profile()->IsChild());
  test_url_loader_factory()->AddResponse(arc::kTokenBootstrapEndPoint,
                                         GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::CHILD_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

// TODO(crbug.com/347393999): Re-enable this test.
IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, DISABLED_ChildTransition) {
  SetAccountAndProfile(user_manager::UserType::kChild);

  session_manager::SessionManager::Get()
      ->HandleUserSessionStartUpTaskCompleted();

  ArcSessionManager* session = ArcSessionManager::Get();
  ASSERT_TRUE(session);

  // Used to track data removal requests.
  ArcDataRemover data_remover(profile()->GetPrefs(),
                              cryptohome::Identification{EmptyAccountId()});

  const std::vector<mojom::ManagementChangeStatus> success_statuses{
      mojom::ManagementChangeStatus::CLOUD_DPC_DISABLED,
      mojom::ManagementChangeStatus::CLOUD_DPC_ALREADY_DISABLED,
      mojom::ManagementChangeStatus::CLOUD_DPC_ENABLED,
      mojom::ManagementChangeStatus::CLOUD_DPC_ALREADY_ENABLED};

  const std::vector<mojom::ManagementChangeStatus> failure_statuses{
      mojom::ManagementChangeStatus::CLOUD_DPC_DISABLING_FAILED,
      mojom::ManagementChangeStatus::CLOUD_DPC_ENABLING_FAILED};

  // Suppress ToS.
  profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);

  // Success statuses do not affect running state of ARC++.
  for (mojom::ManagementChangeStatus status : success_statuses) {
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
    auth_service().ReportManagementChangeStatus(status);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
  }

  // Test failure statuses that lead to showing data removal confirmation and
  // ARC++ stopping. This block tests cancellation of data removal.
  for (mojom::ManagementChangeStatus status : failure_statuses) {
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    // Confirmation dialog is not shown.
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
    // No data removal request.
    EXPECT_FALSE(data_remover.IsScheduledForTesting());
    // Report a failure that brings confirmation dialog.
    auth_service().ReportManagementChangeStatus(status);
    base::RunLoop().RunUntilIdle();
    // This does not cause ARC++ stopped.
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    // Dialog should be shown.
    EXPECT_TRUE(IsDataRemovalConfirmationDialogOpenForTesting());
    // No data removal request.
    EXPECT_FALSE(data_remover.IsScheduledForTesting());
    // Cancel data removal confirmation.
    CloseDataRemovalConfirmationDialogForTesting(false);
    // No data removal request.
    EXPECT_FALSE(data_remover.IsScheduledForTesting());
    // Session state does not change.
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
  }

  // At this time accepts data removal.
  for (mojom::ManagementChangeStatus status : failure_statuses) {
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
    EXPECT_FALSE(data_remover.IsScheduledForTesting());
    auth_service().ReportManagementChangeStatus(status);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    EXPECT_TRUE(IsDataRemovalConfirmationDialogOpenForTesting());
    EXPECT_FALSE(data_remover.IsScheduledForTesting());

    // Accept data removal confirmation.
    CloseDataRemovalConfirmationDialogForTesting(true);
    // Data removal request is issued.
    EXPECT_TRUE(data_remover.IsScheduledForTesting());
    // Session should switch to data removal.
    EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR, session->state());
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
    // After data removal ARC++ is automatically restarted.
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
  }

  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::STOPPED, session->state());

  // Opting out ARC++ forces confirmation dialog to close.
  for (mojom::ManagementChangeStatus status : failure_statuses) {
    // Suppress ToS.
    profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
    profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
    session->StartArcForTesting();
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());

    auth_service().ReportManagementChangeStatus(status);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(IsDataRemovalConfirmationDialogOpenForTesting());

    profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(ArcSessionManager::State::STOPPED, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
  }
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       RegularUserSecondaryAccountsArePropagated) {
  SetAccountAndProfile(user_manager::UserType::kRegular);
  SeedAccountInfo(kSecondaryAccountEmail);
  if (IsArcAccountRestrictionsEnabled()) {
    // 1 SetAccounts() call for the Primary account.
    EXPECT_EQ(1, auth_instance().num_set_accounts_calls());
    EXPECT_EQ(1u, auth_instance().last_set_accounts_list()->size());
    EXPECT_EQ(kFakeUserName,
              (*auth_instance().last_set_accounts_list())[0]->email);
    // 1 call for the Secondary Account.
    EXPECT_EQ(1, auth_instance().num_account_upserted_calls());
  } else {
    // 2 calls: 1 for the Primary Account and 1 for the Secondary Account.
    EXPECT_EQ(2, auth_instance().num_account_upserted_calls());
  }
}

// Tests child account propagation for Family Link user.
IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest,
                       ChildUserSecondaryAccountsPropagation) {
  SetAccountAndProfile(user_manager::UserType::kChild);
  SeedAccountInfo(kSecondaryAccountEmail);
  EXPECT_TRUE(profile()->IsChild());
  if (IsArcAccountRestrictionsEnabled()) {
    // 1 SetAccounts() call for the Primary account.
    EXPECT_EQ(1, auth_instance().num_set_accounts_calls());
    EXPECT_EQ(1u, auth_instance().last_set_accounts_list()->size());
    EXPECT_EQ(kFakeUserName,
              (*auth_instance().last_set_accounts_list())[0]->email);
    // 1 call for the Secondary Account.
    EXPECT_EQ(1, auth_instance().num_account_upserted_calls());
  } else {
    // 2 calls: 1 for the Primary Account and 1 for the Secondary Account.
    EXPECT_EQ(2, auth_instance().num_account_upserted_calls());
  }
}

IN_PROC_BROWSER_TEST_P(ArcAuthServiceTest, HandleRemoveAccountRequest) {
  SetAccountAndProfile(user_manager::UserType::kRegular);
  auth_service().HandleRemoveAccountRequest("dummyemail@google.com");

  EXPECT_EQ(chrome::GetOSSettingsUrl(
                chromeos::settings::mojom::kMyAccountsSubpagePath),
            settings_window_manager().last_url());
}

INSTANTIATE_TEST_SUITE_P(All, ArcRobotAccountAuthServiceTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, ArcAuthServiceTest, testing::Bool());

}  // namespace arc
