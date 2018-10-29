// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_service_launcher.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_context.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/auth/arc_background_auth_code_fetcher.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/ui/app_list/arc/arc_data_removal_dialog.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/account_manager/account_manager.h"
#include "chromeos/account_manager/account_manager_factory.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_data_remover.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_session_runner.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kRefreshToken[] = "fake-refresh-token";
constexpr char kFakeUserName[] = "test@example.com";
constexpr char kFakeGaiaId[] = "1234567890";
constexpr char kFakeAuthCode[] = "fake-auth-code";

std::string GetFakeAuthTokenResponse() {
  return base::StringPrintf(R"({ "token" : "%s"})", kFakeAuthCode);
}

}  // namespace

namespace arc {

class FakeAuthInstance : public mojom::AuthInstance {
 public:
  FakeAuthInstance() : weak_ptr_factory_(this) {}
  ~FakeAuthInstance() override = default;

  // mojom::AuthInstance:
  void InitDeprecated(mojom::AuthHostPtr host) override {
    Init(std::move(host), base::DoNothing());
  }

  void Init(mojom::AuthHostPtr host, InitCallback callback) override {
    host_ = std::move(host);
    std::move(callback).Run();
  }

  void OnAccountInfoReadyDeprecated(mojom::AccountInfoPtr account_info,
                                    mojom::ArcSignInStatus status) override {
    account_info_ = std::move(account_info);
    std::move(done_closure_).Run();
  }

  void RequestAccountInfoDeprecated(base::OnceClosure done_closure) {
    done_closure_ = std::move(done_closure);
    host_->RequestAccountInfoDeprecated(true /* initial_signin */);
  }

  void RequestPrimaryAccountInfo(base::OnceClosure done_closure) {
    host_->RequestPrimaryAccountInfo(base::BindOnce(
        &FakeAuthInstance::OnAccountInfoResponse,
        weak_ptr_factory_.GetWeakPtr(), std::move(done_closure)));
  }

  mojom::AccountInfo* account_info() { return account_info_.get(); }

 private:
  void OnAccountInfoResponse(base::OnceClosure done_closure,
                             mojom::ArcSignInStatus status,
                             mojom::AccountInfoPtr account_info) {
    account_info_ = std::move(account_info);
    std::move(done_closure).Run();
  }

  mojom::AuthHostPtr host_;
  mojom::AccountInfoPtr account_info_;
  base::OnceClosure done_closure_;

  base::WeakPtrFactory<FakeAuthInstance> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FakeAuthInstance);
};

class ArcAuthServiceTest : public InProcessBrowserTest {
 protected:
  ArcAuthServiceTest() = default;

  // InProcessBrowserTest:
  ~ArcAuthServiceTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());
    // Init ArcSessionManager for testing.
    ArcServiceLauncher::Get()->ResetForTesting();
    ArcSessionManager::SetUiEnabledForTesting(false);
    ArcSessionManager::EnableCheckAndroidManagementForTesting(true);
    ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
        std::make_unique<ArcSessionRunner>(base::Bind(FakeArcSession::Create)));

    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDownOnMainThread() override {
    if (arc_bridge_service_)
      arc_bridge_service_->auth()->CloseInstance(&auth_instance_);

    // Explicitly removing the user is required; otherwise ProfileHelper keeps
    // a dangling pointer to the User.
    // TODO(nya): Consider removing all users from ProfileHelper in the
    // destructor of FakeChromeUserManager.
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kFakeUserName, kFakeGaiaId));
    GetFakeUserManager()->RemoveUserFromList(account_id);
    // Since ArcServiceLauncher is (re-)set up with profile() in
    // SetUpOnMainThread() it is necessary to Shutdown() before the profile()
    // is destroyed. ArcServiceLauncher::Shutdown() will be called again on
    // fixture destruction (because it is initialized with the original Profile
    // instance in fixture, once), but it should be no op.
    // TODO(hidehiko): Think about a way to test the code cleanly.
    ArcServiceLauncher::Get()->Shutdown();
    profile_.reset();
    user_manager_enabler_.reset();
    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void SetAccountAndProfile(const user_manager::UserType user_type) {
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kFakeUserName, kFakeGaiaId));
    switch (user_type) {
      case user_manager::USER_TYPE_CHILD:
        GetFakeUserManager()->AddChildUser(account_id);
        break;
      case user_manager::USER_TYPE_REGULAR:
        GetFakeUserManager()->AddUser(account_id);
        break;
      case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
        GetFakeUserManager()->AddPublicAccountUser(account_id);
        break;
      default:
        ADD_FAILURE() << "Unexpected user type " << user_type;
        return;
    }

    GetFakeUserManager()->LoginUser(account_id);
    GetFakeUserManager()->CreateLocalState();

    // Create test profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_builder.SetProfileName(kFakeUserName);

    profile_builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeProfileOAuth2TokenService));
    profile_builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(&BuildFakeSigninManagerForTesting));
    if (user_type == user_manager::USER_TYPE_CHILD)
      profile_builder.SetSupervisedUserId(supervised_users::kChildAccountSUID);

    profile_ = profile_builder.Build();

    SeedAccountInfo(kFakeGaiaId, kFakeUserName);
    chromeos::AccountManagerFactory* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    chromeos::AccountManager* account_manager =
        factory->GetAccountManager(profile_->GetPath().value());
    account_manager->Initialize(
        temp_dir_.GetPath(), test_shared_loader_factory_,
        base::BindRepeating([](const base::RepeatingClosure& closure) -> void {
          closure.Run();
        }));

    FakeProfileOAuth2TokenService* token_service =
        static_cast<FakeProfileOAuth2TokenService*>(
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile()));
    token_service->UpdateCredentials(kFakeUserName, kRefreshToken);
    token_service->set_auto_post_fetch_response_on_message_loop(true);

    FakeSigninManagerBase* signin_manager = static_cast<FakeSigninManagerBase*>(
        SigninManagerFactory::GetForProfile(profile()));
    signin_manager->SetAuthenticatedAccountInfo(kFakeGaiaId, kFakeUserName);

    profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);
    profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
    MigrateSigninScopedDeviceId(profile());

    ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(profile());

    auth_service_ = ArcAuthService::GetForBrowserContext(profile());
    DCHECK(auth_service_);

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    auth_service_->SetURLLoaderFactoryForTesting(test_shared_loader_factory_);
    // It is non-trivial to navigate through the merge session in a testing
    // context; currently we just skip it.
    // TODO(blundell): Figure out how to enable this flow.
    auth_service_->SkipMergeSessionForTesting();

    arc_bridge_service_ = ArcServiceManager::Get()->arc_bridge_service();
    DCHECK(arc_bridge_service_);
    arc_bridge_service_->auth()->SetInstance(&auth_instance_);
    WaitForInstanceReady(arc_bridge_service_->auth());
  }

  void SeedAccountInfo(const std::string& gaia_id, const std::string& email) {
    AccountTrackerService* account_tracker_service =
        AccountTrackerServiceFactory::GetInstance()->GetForProfile(profile());

    AccountInfo account_info;
    account_info.gaia = gaia_id;
    account_info.email = email;
    account_info.full_name = "name";
    account_info.given_name = "name";
    account_info.hosted_domain = "example.com";
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";
    account_info.is_child_account = false;
    account_info.account_id = account_tracker_service->PickAccountIdForAccount(
        account_info.gaia, account_info.email);

    ASSERT_TRUE(account_info.IsValid());

    FakeProfileOAuth2TokenService* token_service =
        static_cast<FakeProfileOAuth2TokenService*>(
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile()));
    token_service->UpdateCredentials(
        account_tracker_service->SeedAccountInfo(account_info), kRefreshToken);
  }

  Profile* profile() { return profile_.get(); }

  void set_profile_name(const std::string& username) {
    profile_->set_profile_name(username);
  }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }
  ArcAuthService& auth_service() { return *auth_service_; }
  FakeAuthInstance& auth_instance() { return auth_instance_; }

 private:
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  FakeAuthInstance auth_instance_;
  // Not owned.
  ArcAuthService* auth_service_ = nullptr;
  ArcBridgeService* arc_bridge_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthServiceTest);
};

// Tests that when ARC requests account info for a non-managed account, via
// |RequestAccountInfoDeprecated| API, Chrome supplies the info configured in
// SetAccountAndProfile() method.
IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest,
                       SuccessfulBackgroundFetchViaDeprecatedApi) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  test_url_loader_factory().AddResponse(arc::kAuthTokenExchangeEndPoint,
                                        GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfoDeprecated(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::USER_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

// Tests that when ARC requests account info for a non-managed account,
// Chrome supplies the info configured in SetAccountAndProfile() method.
IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, SuccessfulBackgroundFetch) {
  SetAccountAndProfile(user_manager::USER_TYPE_REGULAR);
  test_url_loader_factory().AddResponse(arc::kAuthTokenExchangeEndPoint,
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
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

class ArcRobotAccountAuthServiceTest : public ArcAuthServiceTest {
 public:
  ArcRobotAccountAuthServiceTest() = default;
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
                   network::TestURLLoaderFactory& factory) {
    enterprise_management::DeviceManagementResponse response;
    response.mutable_service_api_access_response()->set_auth_code(
        kFakeAuthCode);

    std::string response_data;
    EXPECT_TRUE(response.SerializeToString(&response_data));

    factory.AddResponse(request.url.spec(), response_data);
  }

 private:
  void SetUpPolicyClient() {
    policy::BrowserPolicyConnectorChromeOS* const connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    policy::DeviceCloudPolicyManagerChromeOS* const cloud_policy_manager =
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

  DISALLOW_COPY_AND_ASSIGN(ArcRobotAccountAuthServiceTest);
};

// Tests that when ARC requests account info for a demo session account, via
// |RequestAccountInfoDeprecated| API, Chrome supplies the info configured in
// SetAccountAndProfile() above.
IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest,
                       GetDemoAccountViaDeprecatedApi) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  test_url_loader_factory().SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ResponseJob(request, test_url_loader_factory());
      }));

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfoDeprecated(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::ROBOT_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

// Tests that when ARC requests account info for a demo session account,
// Chrome supplies the info configured in SetAccountAndProfile() above.
IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest, GetDemoAccount) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  test_url_loader_factory().SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ResponseJob(request, test_url_loader_factory());
      }));

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::ROBOT_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest,
                       GetOfflineDemoAccountViaDeprecatedApi) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOffline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfoDeprecated(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_TRUE(auth_instance().account_info()->auth_code.value().empty());
  EXPECT_EQ(mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest, GetOfflineDemoAccount) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOffline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_TRUE(auth_instance().account_info()->auth_code.value().empty());
  EXPECT_EQ(mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest,
                       GetDemoAccountOnAuthTokenFetchFailureViaDeprecatedApi) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  test_url_loader_factory().SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        test_url_loader_factory().AddResponse(request.url.spec(), std::string(),
                                              net::HTTP_NOT_FOUND);
      }));

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfoDeprecated(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_TRUE(auth_instance().account_info()->auth_code.value().empty());
  EXPECT_EQ(mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcRobotAccountAuthServiceTest,
                       GetDemoAccountOnAuthTokenFetchFailure) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  chromeos::DemoSession::StartIfInDemoMode();

  SetAccountAndProfile(user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  test_url_loader_factory().SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        test_url_loader_factory().AddResponse(request.url.spec(), std::string(),
                                              net::HTTP_NOT_FOUND);
      }));

  base::RunLoop run_loop;
  auth_instance().RequestPrimaryAccountInfo(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_TRUE(auth_instance().account_info()->account_name.value().empty());
  EXPECT_TRUE(auth_instance().account_info()->auth_code.value().empty());
  EXPECT_EQ(mojom::ChromeAccountType::OFFLINE_DEMO_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_TRUE(auth_instance().account_info()->is_managed);
}

class ArcAuthServiceChildAccountTest : public ArcAuthServiceTest {
 protected:
  ArcAuthServiceChildAccountTest() = default;
  ~ArcAuthServiceChildAccountTest() override = default;

  void SetUpOnMainThread() override {
    scoped_feature_list_.InitAndEnableFeature(
        arc::kAvailableForChildAccountFeature);
    ArcAuthServiceTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthServiceChildAccountTest);
};

// Tests that when ARC requests account info for a child account, via
// |RequestAccountInfoDeprecated| and Chrome supplies the info configured in
// SetAccountAndProfile() above.
IN_PROC_BROWSER_TEST_F(ArcAuthServiceChildAccountTest,
                       ChildAccountFetchViaDeprecatedApi) {
  SetAccountAndProfile(user_manager::USER_TYPE_CHILD);
  EXPECT_TRUE(profile()->IsChild());
  test_url_loader_factory().AddResponse(arc::kAuthTokenExchangeEndPoint,
                                        GetFakeAuthTokenResponse());

  base::RunLoop run_loop;
  auth_instance().RequestAccountInfoDeprecated(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(auth_instance().account_info());
  EXPECT_EQ(kFakeUserName,
            auth_instance().account_info()->account_name.value());
  EXPECT_EQ(kFakeAuthCode, auth_instance().account_info()->auth_code.value());
  EXPECT_EQ(mojom::ChromeAccountType::CHILD_ACCOUNT,
            auth_instance().account_info()->account_type);
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

// Tests that when ARC requests account info for a child account and
// Chrome supplies the info configured in SetAccountAndProfile() above.
IN_PROC_BROWSER_TEST_F(ArcAuthServiceChildAccountTest, ChildAccountFetch) {
  SetAccountAndProfile(user_manager::USER_TYPE_CHILD);
  EXPECT_TRUE(profile()->IsChild());
  test_url_loader_factory().AddResponse(arc::kAuthTokenExchangeEndPoint,
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
  EXPECT_FALSE(auth_instance().account_info()->enrollment_token);
  EXPECT_FALSE(auth_instance().account_info()->is_managed);
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceChildAccountTest, ChildTransition) {
  SetAccountAndProfile(user_manager::USER_TYPE_CHILD);

  ArcSessionManager* session = ArcSessionManager::Get();
  ASSERT_TRUE(session);

  // Used to track data removal requests.
  ArcDataRemover data_remover(profile()->GetPrefs(),
                              cryptohome::Identification{EmptyAccountId()});

  const std::vector<mojom::SupervisionChangeStatus> success_statuses{
      mojom::SupervisionChangeStatus::CLOUD_DPC_DISABLED,
      mojom::SupervisionChangeStatus::CLOUD_DPC_ALREADY_DISABLED,
      mojom::SupervisionChangeStatus::CLOUD_DPC_ENABLED,
      mojom::SupervisionChangeStatus::CLOUD_DPC_ALREADY_ENABLED};

  const std::vector<mojom::SupervisionChangeStatus> failure_statuses{
      mojom::SupervisionChangeStatus::CLOUD_DPC_DISABLING_FAILED,
      mojom::SupervisionChangeStatus::CLOUD_DPC_ENABLING_FAILED};

  // Suppress ToS.
  profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);

  // Success statuses do not affect running state of ARC++.
  for (mojom::SupervisionChangeStatus status : success_statuses) {
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
    auth_service().ReportSupervisionChangeStatus(status);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
  }

  // Test failure statuses that lead to showing data removal confirmation and
  // ARC++ stopping. This block tests cancelation of data removal.
  for (mojom::SupervisionChangeStatus status : failure_statuses) {
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    // Confirmation dialog is not shown.
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
    // No data removal request.
    EXPECT_FALSE(data_remover.IsScheduledForTesting());
    // Report a failure that brings confirmation dialog.
    auth_service().ReportSupervisionChangeStatus(status);
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
  for (mojom::SupervisionChangeStatus status : failure_statuses) {
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
    EXPECT_FALSE(data_remover.IsScheduledForTesting());
    auth_service().ReportSupervisionChangeStatus(status);
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
  for (mojom::SupervisionChangeStatus status : failure_statuses) {
    // Suppress ToS.
    profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
    profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
    session->StartArcForTesting();
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, session->state());

    auth_service().ReportSupervisionChangeStatus(status);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(IsDataRemovalConfirmationDialogOpenForTesting());

    profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(ArcSessionManager::State::STOPPED, session->state());
    EXPECT_FALSE(IsDataRemovalConfirmationDialogOpenForTesting());
  }
}

}  // namespace arc
