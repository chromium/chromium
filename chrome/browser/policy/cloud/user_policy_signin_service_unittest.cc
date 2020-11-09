// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"
#else
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#endif

namespace em = enterprise_management;

using testing::_;
using testing::AnyNumber;
using testing::Mock;

namespace policy {

namespace {

constexpr char kTestUser[] = "testuser@test.com";

constexpr char kHostedDomainResponse[] = R"(
    {
      "hd": "test.com"
    })";

std::unique_ptr<UserCloudPolicyManager> BuildCloudPolicyManager() {
  auto store = std::make_unique<MockUserCloudPolicyStore>();
  EXPECT_CALL(*store, Load()).Times(AnyNumber());

  return std::make_unique<UserCloudPolicyManager>(
      std::move(store), base::FilePath(),
      /*cloud_external_data_manager=*/nullptr,
      base::ThreadTaskRunnerHandle::Get(),
      network::TestNetworkConnectionTracker::CreateGetter());
}

class UserPolicySigninServiceTest : public testing::Test {
 public:
  UserPolicySigninServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_account_id_(AccountId::FromUserEmailGaiaId(
            kTestUser,
            signin::GetTestGaiaIdForEmail(kTestUser))),
        register_completed_(false) {}

  MOCK_METHOD1(OnPolicyRefresh, void(bool));

  void OnRegisterCompleted(const std::string& dm_token,
                           const std::string& client_id) {
    register_completed_ = true;
    dm_token_ = dm_token;
    client_id_ = client_id;
  }

  void RegisterPolicyClientWithCallback(UserPolicySigninService* service) {
    UserPolicySigninServiceBase::PolicyRegistrationCallback callback =
        base::BindOnce(&UserPolicySigninServiceTest::OnRegisterCompleted,
                       base::Unretained(this));
    AccountInfo account_info =
        identity_test_env()->MakeAccountAvailable(kTestUser);
    service->RegisterForPolicyWithAccountId(kTestUser, account_info.account_id,
                                            std::move(callback));
    ASSERT_TRUE(IsRequestActive());
  }

  void SetUp() override {
    device_management_service_.ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();

    UserPolicySigninServiceFactory::SetDeviceManagementServiceForTesting(
        &device_management_service_);

    local_state_.reset(new TestingPrefServiceSimple);
    RegisterLocalState(local_state_->registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(local_state_.get());
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());

    g_browser_process->browser_policy_connector()->Init(
        local_state_.get(), test_url_loader_factory_.GetSafeWeakWrapper());

    // Create a testing profile with cloud-policy-on-signin enabled, and bring
    // up a UserCloudPolicyManager with a MockUserCloudPolicyStore.
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs(
        new sync_preferences::TestingPrefServiceSyncable());
    RegisterUserProfilePrefs(prefs->registry());

    TestingProfile::Builder builder;
    builder.SetPrefService(
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(
            std::move(prefs)));
    builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    builder.SetUserCloudPolicyManager(BuildCloudPolicyManager());
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    manager_ = profile_->GetUserCloudPolicyManager();
    DCHECK(manager_);
    manager_->Init(&schema_registry_);
    mock_store_ =
        static_cast<MockUserCloudPolicyStore*>(manager_->core()->store());
    DCHECK(mock_store_);
    AddProfile();

    Mock::VerifyAndClearExpectations(mock_store_);
  }

  void TearDown() override {
    UserPolicySigninServiceFactory::SetDeviceManagementServiceForTesting(NULL);

    // Free the profile before we clear out the browser prefs.
    identity_test_env_adaptor_.reset();
    profile_.reset();
    TestingBrowserProcess* testing_browser_process =
        TestingBrowserProcess::GetGlobal();
    testing_browser_process->SetLocalState(NULL);
    local_state_.reset();
    testing_browser_process->ShutdownBrowserPolicyConnector();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  virtual void AddProfile() {
    // For this test, the user should not be signed in yet.
    DCHECK(!identity_test_env()->identity_manager()->HasPrimaryAccount());

    // Initializing UserPolicySigninService while the user is not signed in
    // should result in the store being cleared to remove any lingering policy.
    EXPECT_CALL(*mock_store_, Clear());

    // Let the SigninService know that the profile has been created.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_ADDED,
        content::Source<Profile>(profile_.get()),
        content::NotificationService::NoDetails());
  }

  bool IsRequestActive() {
    if (identity_test_env()->IsAccessTokenRequestPending())
      return true;
    return test_url_loader_factory_.NumPending() > 0;
  }

  void MakeOAuthTokenFetchSucceed() {
    ASSERT_TRUE(IsRequestActive());
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            "access_token", base::Time::Now());
  }

  void MakeOAuthTokenFetchFail() {
    ASSERT_TRUE(identity_test_env()->IsAccessTokenRequestPending());
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
            GoogleServiceAuthError::FromServiceError("fail"));
  }

  void ReportHostedDomainStatus(bool is_hosted_domain) {
    ASSERT_TRUE(IsRequestActive());
    ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GaiaUrls::GetInstance()->oauth_user_info_url().spec(),
        is_hosted_domain ? kHostedDomainResponse : "{}"));
  }

  void TestSuccessfulSignin() {
    UserPolicySigninService* signin_service =
        UserPolicySigninServiceFactory::GetForProfile(profile_.get());
    EXPECT_CALL(*this, OnPolicyRefresh(true)).Times(0);
    RegisterPolicyClientWithCallback(signin_service);

    // Sign in to Chrome.
    identity_test_env()->SetPrimaryAccount(kTestUser);

    // Mimic successful oauth token fetch.
    MakeOAuthTokenFetchSucceed();

    // When the user is from a hosted domain, this should kick off client
    // registration.
    DeviceManagementService::JobConfiguration::JobType job_type =
        DeviceManagementService::JobConfiguration::TYPE_INVALID;
    DeviceManagementService::JobControl* job_control = nullptr;
    EXPECT_CALL(device_management_service_, StartJob(_))
        .WillOnce(DoAll(
            device_management_service_.CaptureJobType(&job_type),
            device_management_service_.StartJobFullControl(&job_control)));

    // Now mimic the user being a hosted domain - this should cause a Register()
    // call.
    ReportHostedDomainStatus(true);

    // Should have no more outstanding requests.
    ASSERT_FALSE(IsRequestActive());
    Mock::VerifyAndClearExpectations(this);
    ASSERT_NE(nullptr, job_control);
    EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
              job_type);

    std::string expected_dm_token = "dm_token";
    em::DeviceManagementResponse registration_response;
    registration_response.mutable_register_response()
        ->set_device_management_token(expected_dm_token);
    registration_response.mutable_register_response()->set_enrollment_type(
        em::DeviceRegisterResponse::ENTERPRISE);
    device_management_service_.DoURLCompletion(
        &job_control, net::OK, DeviceManagementService::kSuccess,
        registration_response);

    EXPECT_TRUE(register_completed_);
    EXPECT_EQ(dm_token_, expected_dm_token);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 protected:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  MockUserCloudPolicyStore* mock_store_ = nullptr;  // Not owned.
  SchemaRegistry schema_registry_;
  UserCloudPolicyManager* manager_ = nullptr;  // Not owned.

  // BrowserPolicyConnector and UrlFetcherFactory want to initialize and free
  // various components asynchronously via tasks, so create fake threads here.
  content::BrowserTaskEnvironment task_environment_;

  // Used in conjunction with OnRegisterCompleted() to test client registration
  // callbacks.
  std::string dm_token_;
  std::string client_id_;

  // AccountId for the test user.
  AccountId test_account_id_;

  // True if OnRegisterCompleted() was called.
  bool register_completed_;

  MockDeviceManagementService device_management_service_;

  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

class UserPolicySigninServiceSignedInTest : public UserPolicySigninServiceTest {
 public:
  void AddProfile() override {
    // UserCloudPolicyManager should not be initialized.
    ASSERT_FALSE(manager_->core()->service());

    // Set the user as signed in.
    identity_test_env()->SetPrimaryAccount(kTestUser);

    // Let the SigninService know that the profile has been created.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_ADDED,
        content::Source<Profile>(profile_.get()),
        content::NotificationService::NoDetails());
  }
};

TEST_F(UserPolicySigninServiceTest, InitWhileSignedOut) {
  // Make sure user is not signed in.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount());

  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());
}

#if !defined(OS_ANDROID)
TEST_F(UserPolicySigninServiceTest, InitRefreshTokenAvailableBeforeSignin) {
  // Make sure user is not signed in.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount());

  // No oauth access token yet, so client registration should be deferred.
  ASSERT_FALSE(IsRequestActive());

  // Make oauth token available.
  identity_test_env()->MakeAccountAvailable(kTestUser);

  // Not signed in yet, so client registration should be deferred.
  ASSERT_FALSE(IsRequestActive());

  // Sign in to Chrome.
  identity_test_env()->SetPrimaryAccount(kTestUser);

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Client registration should be in progress since we now have an oauth token
  // for the authenticated account id.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(IsRequestActive());
}
#endif  // !defined(OS_ANDROID)

// TODO(joaodasilva): these tests rely on issuing the OAuth2 login refresh
// token after signin. Revisit this after figuring how to handle that on
// Android.
#if !defined(OS_ANDROID)

TEST_F(UserPolicySigninServiceSignedInTest, InitWhileSignedIn) {
  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->core()->service());

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // No oauth access token yet, so client registration should be deferred.
  ASSERT_FALSE(IsRequestActive());

  // Make oauth token available.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  // Client registration should be in progress since we now have an oauth token.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceSignedInTest, InitWhileSignedInOAuthError) {
  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->core()->service());

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // No oauth access token yet, so client registration should be deferred.
  ASSERT_FALSE(IsRequestActive());

  // Make oauth token available.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  // Client registration should be in progress since we now have an oauth token.
  ASSERT_TRUE(IsRequestActive());

  // Now fail the access token fetch.
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      error);
  ASSERT_FALSE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceTest, SignInAfterInit) {
  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in the user.
  identity_test_env()->SetPrimaryAccount(kTestUser);

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Make oauth token available.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  // UserCloudPolicyManager should be initialized.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());

  // Client registration should be in progress since we have an oauth token.
  ASSERT_TRUE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceTest, SignInWithNonEnterpriseUser) {
  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in a non-enterprise user (blacklisted gmail.com domain).
  identity_test_env()->SetPrimaryAccount("non_enterprise_user@gmail.com");

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Make oauth token available.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  // UserCloudPolicyManager should not be initialized and there should be no
  // DMToken request active.
  ASSERT_TRUE(!manager_->core()->service());
  ASSERT_FALSE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceTest, UnregisteredClient) {
  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in the user.
  identity_test_env()->SetPrimaryAccount(kTestUser);

  // Make oauth token available.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  // UserCloudPolicyManager should be initialized.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());

  // Client registration should not be in progress since the store is not
  // yet initialized.
  ASSERT_FALSE(IsRequestActive());

  // Complete initialization of the store with no policy (unregistered client).
  mock_store_->NotifyStoreLoaded();

  // Client registration should be in progress since we have an oauth token.
  ASSERT_TRUE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceTest, RegisteredClient) {
  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in the user.
  identity_test_env()->SetPrimaryAccount(kTestUser);

  // Make oauth token available.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  // UserCloudPolicyManager should be initialized.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());

  // Client registration should not be in progress since the store is not
  // yet initialized.
  ASSERT_FALSE(manager_->IsClientRegistered());
  ASSERT_FALSE(IsRequestActive());

  mock_store_->policy_.reset(new enterprise_management::PolicyData());
  mock_store_->policy_->set_request_token("fake token");
  mock_store_->policy_->set_device_id("fake client id");

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Since there is a signed-in user expect a policy fetch to be started to
  // refresh the policy for the user.
  DeviceManagementService::JobConfiguration::JobType job_type =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobControl* job_control = nullptr;
  EXPECT_CALL(device_management_service_, StartJob(_))
      .WillOnce(
          DoAll(device_management_service_.CaptureJobType(&job_type),
                device_management_service_.StartJobFullControl(&job_control)));

  // Client registration should not be in progress since the client should be
  // already registered.
  ASSERT_TRUE(manager_->IsClientRegistered());
  ASSERT_FALSE(IsRequestActive());
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
}

#endif  // !defined(OS_ANDROID)

TEST_F(UserPolicySigninServiceSignedInTest, SignOutAfterInit) {
  // UserCloudPolicyManager should be initialized.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());

  // Signing out will clear the policy from the store.
  EXPECT_CALL(*mock_store_, Clear());

  // Now sign out.
  identity_test_env()->ClearPrimaryAccount();

  // UserCloudPolicyManager should be shut down.
  ASSERT_FALSE(manager_->core()->service());
}

TEST_F(UserPolicySigninServiceTest, RegisterPolicyClientOAuthFailure) {
  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());
  RegisterPolicyClientWithCallback(signin_service);
  Mock::VerifyAndClearExpectations(this);

  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());
  ASSERT_TRUE(IsRequestActive());
  EXPECT_FALSE(register_completed_);

  // Cause the access token fetch to fail - callback should be invoked.
  MakeOAuthTokenFetchFail();

  EXPECT_TRUE(register_completed_);
  EXPECT_TRUE(dm_token_.empty());
  ASSERT_FALSE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceTest, RegisterPolicyClientNonHostedDomain) {
  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());
  RegisterPolicyClientWithCallback(signin_service);

  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());
  ASSERT_TRUE(IsRequestActive());

  // Cause the access token request to succeed.
  MakeOAuthTokenFetchSucceed();

  // Should be a follow-up fetch to check the hosted-domain status.
  ASSERT_TRUE(IsRequestActive());
  Mock::VerifyAndClearExpectations(this);

  EXPECT_FALSE(register_completed_);

  // Report that the user is not on a hosted domain - callback should be
  // invoked reporting a failed fetch.
  ReportHostedDomainStatus(false);

  // Since this is not a hosted domain, we should not issue a request for a
  // DMToken.
  EXPECT_TRUE(register_completed_);
  EXPECT_TRUE(dm_token_.empty());
  ASSERT_FALSE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceTest, RegisterPolicyClientFailedRegistration) {
  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());
  RegisterPolicyClientWithCallback(signin_service);

  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());

  // Mimic successful oauth token fetch.
  MakeOAuthTokenFetchSucceed();
  EXPECT_FALSE(register_completed_);

  // When the user is from a hosted domain, this should kick off client
  // registration.
  DeviceManagementService::JobConfiguration::JobType job_type =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobControl* job_control = nullptr;
  EXPECT_CALL(device_management_service_, StartJob(_))
      .WillOnce(
          DoAll(device_management_service_.CaptureJobType(&job_type),
                device_management_service_.StartJobFullControl(&job_control)));

  // Now mimic the user being a hosted domain - this should cause a Register()
  // call.
  ReportHostedDomainStatus(true);

  // Should have no more outstanding requests.
  ASSERT_FALSE(IsRequestActive());
  Mock::VerifyAndClearExpectations(this);
  ASSERT_NE(nullptr, job_control);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type);
  EXPECT_FALSE(register_completed_);

  // Make client registration fail (hosted domain user that is not managed).
  device_management_service_.DoURLCompletion(
      &job_control, net::OK,
      DeviceManagementService::kDeviceManagementNotAllowed,
      em::DeviceManagementResponse());

  EXPECT_TRUE(register_completed_);
  EXPECT_TRUE(dm_token_.empty());
}

TEST_F(UserPolicySigninServiceTest, RegisterPolicyClientSucceeded) {
  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());
  RegisterPolicyClientWithCallback(signin_service);

  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());

  // Mimic successful oauth token fetch.
  MakeOAuthTokenFetchSucceed();

  // When the user is from a hosted domain, this should kick off client
  // registration.
  DeviceManagementService::JobConfiguration::JobType job_type =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobControl* job_control = nullptr;
  EXPECT_CALL(device_management_service_, StartJob(_))
      .WillOnce(
          DoAll(device_management_service_.CaptureJobType(&job_type),
                device_management_service_.StartJobFullControl(&job_control)));

  // Now mimic the user being a hosted domain - this should cause a Register()
  // call.
  ReportHostedDomainStatus(true);

  // Should have no more outstanding requests.
  ASSERT_FALSE(IsRequestActive());
  Mock::VerifyAndClearExpectations(this);
  ASSERT_NE(nullptr, job_control);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type);
  EXPECT_FALSE(register_completed_);

  em::DeviceManagementResponse registration_response;
  std::string expected_dm_token = "dm_token";
  registration_response.mutable_register_response()
      ->set_device_management_token(expected_dm_token);
  registration_response.mutable_register_response()->set_enrollment_type(
      em::DeviceRegisterResponse::ENTERPRISE);
  device_management_service_.DoURLCompletion(&job_control, net::OK,
                                             DeviceManagementService::kSuccess,
                                             registration_response);

  EXPECT_TRUE(register_completed_);
  EXPECT_EQ(dm_token_, expected_dm_token);
  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());
}

TEST_F(UserPolicySigninServiceTest, SignOutThenSignInAgain) {
  // Explicitly forcing this call is necessary for the clearing of the primary
  // account to result in the account being fully removed in this testing
  // context
  identity_test_env()->EnableRemovalOfExtendedAccountInfo();

  ASSERT_NO_FATAL_FAILURE(TestSuccessfulSignin());

  EXPECT_CALL(*mock_store_, Clear());

  identity_test_env()->ClearPrimaryAccount();
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in again.
  ASSERT_NO_FATAL_FAILURE(TestSuccessfulSignin());
}

}  // namespace

}  // namespace policy
