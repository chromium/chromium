// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
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
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"
#else
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#endif

namespace em = enterprise_management;

using testing::_;
using testing::AnyNumber;
using testing::Mock;
using testing::Return;
using testing::SaveArg;

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
      base::SingleThreadTaskRunner::GetCurrentDefault(),
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

  void OnRegisterCompleted(
      const std::string& dm_token,
      const std::string& client_id,
      const std::vector<std::string>& user_affiliation_ids) {
    register_completed_ = true;
    dm_token_ = dm_token;
    client_id_ = client_id;
    user_affiliation_ids_ = user_affiliation_ids;
  }

  void RegisterPolicyClientWithCallback(UserPolicySigninService* service) {
    UserPolicySigninServiceBase::PolicyRegistrationCallback callback =
        base::BindOnce(&UserPolicySigninServiceTest::OnRegisterCompleted,
                       base::Unretained(this));
    AccountInfo account_info =
        identity_test_env()
            ->identity_manager()
            ->FindExtendedAccountInfoByEmailAddress(kTestUser);
    if (account_info.IsEmpty()) {
      account_info = identity_test_env()->MakeAccountAvailable(kTestUser);
    }
    DCHECK(!account_info.IsEmpty());
    service->RegisterForPolicyWithAccountId(kTestUser, account_info.account_id,
                                            std::move(callback));
    ASSERT_TRUE(IsRequestActive());
  }

  void SetUp() override {
    device_management_service_.ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();

    UserPolicySigninServiceFactory::SetDeviceManagementServiceForTesting(
        &device_management_service_);

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
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

    UserPolicySigninServiceFactory::GetForProfile(profile_.get())
        ->set_profile_can_be_managed_for_testing(true);
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
    DCHECK(!identity_test_env()->identity_manager()->HasPrimaryAccount(
        signin::ConsentLevel::kSignin));

    // Initializing UserPolicySigninService while the user is not signed in
    // should result in the store being cleared to remove any lingering policy.
    EXPECT_CALL(*mock_store_, Clear());

    // Let the SigninService know that the profile has been created.
#if BUILDFLAG(IS_ANDROID)
    UserPolicySigninServiceFactory::GetForProfile(profile_.get())
        ->OnProfileAdded(profile_.get());
#else
    UserPolicySigninServiceFactory::GetForProfile(profile_.get())
        ->OnProfileReady(profile_.get());
#endif  // BUILDFLAG(IS_ANDROID)
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
    identity_test_env()->SetPrimaryAccount(kTestUser,
                                           signin::ConsentLevel::kSync);

    // Mimic successful oauth token fetch.
    MakeOAuthTokenFetchSucceed();

    // When the user is from a hosted domain, this should kick off client
    // registration.
    DeviceManagementService::JobConfiguration::JobType job_type =
        DeviceManagementService::JobConfiguration::TYPE_INVALID;
    DeviceManagementService::JobForTesting job;
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type),
                        SaveArg<0>(&job)));

    // Now mimic the user being a hosted domain - this should cause a Register()
    // call.
    ReportHostedDomainStatus(true);

    // Should have no more outstanding requests.
    ASSERT_FALSE(IsRequestActive());
    Mock::VerifyAndClearExpectations(this);
    ASSERT_TRUE(job.IsActive());
    EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
              job_type);

    std::string expected_dm_token = "dm_token";
    std::string expected_user_affiliation_id = "affiliation_id";
    em::DeviceManagementResponse dm_response;
    auto* register_response = dm_response.mutable_register_response();
    register_response->set_device_management_token(expected_dm_token);
    register_response->set_enrollment_type(
        em::DeviceRegisterResponse::ENTERPRISE);
    register_response->add_user_affiliation_ids(expected_user_affiliation_id);
    device_management_service_.SendJobOKNow(&job, dm_response);

    EXPECT_TRUE(register_completed_);
    EXPECT_EQ(dm_token_, expected_dm_token);
    std::vector<std::string> expected_user_affiliation_ids = {
        expected_user_affiliation_id};
    EXPECT_EQ(user_affiliation_ids_, expected_user_affiliation_ids);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 protected:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<MockUserCloudPolicyStore, DanglingUntriaged> mock_store_ =
      nullptr;  // Not owned.
  SchemaRegistry schema_registry_;
  raw_ptr<UserCloudPolicyManager, DanglingUntriaged> manager_ =
      nullptr;  // Not owned.

  // BrowserPolicyConnector and UrlFetcherFactory want to initialize and free
  // various components asynchronously via tasks, so create fake threads here.
  content::BrowserTaskEnvironment task_environment_;

  // Used in conjunction with OnRegisterCompleted() to test client registration
  // callbacks.
  std::string dm_token_;
  std::string client_id_;
  std::vector<std::string> user_affiliation_ids_;

  // AccountId for the test user.
  AccountId test_account_id_;

  // True if OnRegisterCompleted() was called.
  bool register_completed_;

  testing::StrictMock<MockJobCreationHandler> job_creation_handler_;
  FakeDeviceManagementService device_management_service_{
      &job_creation_handler_};

  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  network::TestURLLoaderFactory test_url_loader_factory_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::ScopedLacrosServiceTestHelper test_helper;
#endif
};

class UserPolicySigninServiceSignedInTest : public UserPolicySigninServiceTest {
 public:
  void AddProfile() override {
    // UserCloudPolicyManager should not be initialized.
    ASSERT_FALSE(manager_->core()->service());

    // Set the user as signed in.
    identity_test_env()->SetPrimaryAccount(kTestUser,
                                           signin::ConsentLevel::kSync);

    // Let the SigninService know that the profile has been created.
#if BUILDFLAG(IS_ANDROID)
    UserPolicySigninServiceFactory::GetForProfile(profile_.get())
        ->OnProfileAdded(profile_.get());
#else
    UserPolicySigninServiceFactory::GetForProfile(profile_.get())
        ->OnProfileReady(profile_.get());
#endif  // BUILDFLAG(IS_ANDROID)
  }
};

TEST_F(UserPolicySigninServiceTest, InitWhileSignedOut) {
  // Make sure user is not signed in.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());
  EXPECT_FALSE(manager_->ArePoliciesRequired());
}

// TODO(crbug.com/40831734): Extend the test coverage by merging tests from
// ios/chrome/browser/policy/cloud/user_policy_signin_service_unittest.mm here.

#if !BUILDFLAG(IS_ANDROID)
TEST_F(UserPolicySigninServiceTest, InitRefreshTokenAvailableBeforeSignin) {
  // Make sure user is not signed in.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  // No oauth access token yet, so client registration should be deferred.
  ASSERT_FALSE(IsRequestActive());

  // Make oauth token available.
  identity_test_env()->MakeAccountAvailable(kTestUser);

  // Not signed in yet, so client registration should be deferred.
  ASSERT_FALSE(IsRequestActive());

  // Sign in to Chrome.
  identity_test_env()->SetPrimaryAccount(kTestUser,
                                         signin::ConsentLevel::kSync);

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Client registration should be in progress since we now have an oauth token
  // for the authenticated account id.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(IsRequestActive());
  EXPECT_TRUE(manager_->ArePoliciesRequired());
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(joaodasilva): these tests rely on issuing the OAuth2 login refresh
// token after signin. Revisit this after figuring how to handle that on
// Android.
#if !BUILDFLAG(IS_ANDROID)

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
  EXPECT_TRUE(manager_->ArePoliciesRequired());
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
  EXPECT_TRUE(manager_->ArePoliciesRequired());
}

TEST_F(UserPolicySigninServiceTest, SignInAfterInit) {
  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in the user.
  identity_test_env()->SetPrimaryAccount(kTestUser,
                                         signin::ConsentLevel::kSync);

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Make oauth token available.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  // UserCloudPolicyManager should be initialized.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());

  // Client registration should be in progress since we have an oauth token.
  ASSERT_TRUE(IsRequestActive());
  EXPECT_TRUE(manager_->ArePoliciesRequired());
}

TEST_F(UserPolicySigninServiceTest, SignInWithNonEnterpriseUser) {
  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in a non-enterprise user (gmail.com domain).
  identity_test_env()->SetPrimaryAccount("non_enterprise_user@gmail.com",
                                         signin::ConsentLevel::kSync);

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Make oauth token available.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  // UserCloudPolicyManager should not be initialized and there should be no
  // DMToken request active.
  ASSERT_TRUE(!manager_->core()->service());
  ASSERT_FALSE(IsRequestActive());
  EXPECT_FALSE(manager_->ArePoliciesRequired());
}

TEST_F(UserPolicySigninServiceTest, UnregisteredClient) {
  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in the user.
  identity_test_env()->SetPrimaryAccount(kTestUser,
                                         signin::ConsentLevel::kSync);

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
  EXPECT_TRUE(manager_->ArePoliciesRequired());
}

TEST_F(UserPolicySigninServiceTest, RegisteredClient) {
  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in the user.
  identity_test_env()->SetPrimaryAccount(kTestUser,
                                         signin::ConsentLevel::kSync);

  // Make oauth token available.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  // UserCloudPolicyManager should be initialized.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());

  // Client registration should not be in progress since the store is not
  // yet initialized.
  ASSERT_FALSE(manager_->IsClientRegistered());
  ASSERT_FALSE(IsRequestActive());

  auto data = std::make_unique<enterprise_management::PolicyData>();
  data->set_request_token("fake token");
  data->set_device_id("fake client id");
  mock_store_->set_policy_data_for_testing(std::move(data));

  // Since there is a signed-in user expect a policy fetch to be started to
  // refresh the policy for the user.
  DeviceManagementService::JobConfiguration::JobType job_type_1 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType job_type_2 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobForTesting job;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .Times(2)
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type_1),
                      SaveArg<0>(&job)))
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type_2),
                      SaveArg<0>(&job)));

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Client registration should not be in progress since the client should be
  // already registered.
  ASSERT_TRUE(manager_->IsClientRegistered());
  ASSERT_FALSE(IsRequestActive());
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS,
            job_type_1);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_2);
}

// Tests that the explicit policy registration can coexist with registration
// triggered by sign-in.
TEST_F(UserPolicySigninServiceTest,
       InitializeForSignedInUserWhileRegisteringForPolicy) {
  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());

  // Start registration process in a temporary client.
  RegisterPolicyClientWithCallback(signin_service);
  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());
  // Complete several registration steps.
  MakeOAuthTokenFetchSucceed();
  DeviceManagementService::JobForTesting job;
  EXPECT_CALL(job_creation_handler_, OnJobCreation).WillOnce(SaveArg<0>(&job));
  ReportHostedDomainStatus(true);
  ASSERT_FALSE(IsRequestActive());
  Mock::VerifyAndClearExpectations(&job_creation_handler_);
  ASSERT_TRUE(job.IsActive());
  EXPECT_FALSE(register_completed_);

  // Add a primary account now. This should trigger the UserCloudPolicyManager
  // initialization.
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);
  // UserCloudPolicyManager should be initialized.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());
  ASSERT_TRUE(manager_->core()->client());
  ASSERT_FALSE(manager_->IsClientRegistered());
  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();
  // New access token request has been sent.
  ASSERT_TRUE(IsRequestActive());

  // Complete registration in the temporary client.
  em::DeviceManagementResponse registration_response;
  std::string expected_dm_token = "dm_token";
  registration_response.mutable_register_response()
      ->set_device_management_token(expected_dm_token);
  registration_response.mutable_register_response()->set_enrollment_type(
      em::DeviceRegisterResponse::ENTERPRISE);
  device_management_service_.SendJobOKNow(&job, registration_response);
  EXPECT_TRUE(register_completed_);
  EXPECT_EQ(dm_token_, expected_dm_token);
}

// Tests that the explicit policy registration can coexist with registration
// triggered by sign-in.
TEST_F(UserPolicySigninServiceTest,
       RegisterForPolicyWhileInitializingForSignedInUser) {
  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());

  // Register a primary account now. This should trigger UserCloudPolicyManager
  // initialization.
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);
  // UserCloudPolicyManager should be initialized.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());
  ASSERT_TRUE(manager_->core()->client());
  ASSERT_FALSE(manager_->IsClientRegistered());
  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();
  // New access token request has been sent.
  ASSERT_TRUE(IsRequestActive());
  // Mimic successful oauth token fetch.
  MakeOAuthTokenFetchSucceed();
  // Request is not active because UserPolicySigninService doesn't use
  // `test_url_loader_factory_`.
  ASSERT_FALSE(IsRequestActive());
  Mock::VerifyAndClearExpectations(this);

  // Start and complete registration in a temporary client.
  RegisterPolicyClientWithCallback(signin_service);
  EXPECT_FALSE(register_completed_);
  ASSERT_TRUE(IsRequestActive());
  MakeOAuthTokenFetchSucceed();
  DeviceManagementService::JobForTesting job;
  EXPECT_CALL(job_creation_handler_, OnJobCreation).WillOnce(SaveArg<0>(&job));
  ReportHostedDomainStatus(true);
  ASSERT_FALSE(IsRequestActive());
  Mock::VerifyAndClearExpectations(&job_creation_handler_);
  ASSERT_TRUE(job.IsActive());
  em::DeviceManagementResponse registration_response;
  std::string expected_dm_token = "dm_token";
  registration_response.mutable_register_response()
      ->set_device_management_token(expected_dm_token);
  registration_response.mutable_register_response()->set_enrollment_type(
      em::DeviceRegisterResponse::ENTERPRISE);
  device_management_service_.SendJobOKNow(&job, registration_response);
  EXPECT_TRUE(register_completed_);
  EXPECT_EQ(dm_token_, expected_dm_token);
}

// Tests that `FetchPolicyForSignedInUser()` can be called in the middle of a
// client registration.
TEST_F(UserPolicySigninServiceTest,
       FetchPolicyForSignedInUserWhileUnregisteredClient) {
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);

  // UserCloudPolicyManager should be initialized.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());
  ASSERT_FALSE(manager_->IsClientRegistered());
  // Client registration should not be in progress since the store is not
  // yet initialized.
  ASSERT_FALSE(IsRequestActive());

  // `FetchPolicyForSignedInUser()` will notify the callback after the client
  // registers and fetches policies.
  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());
  network::TestURLLoaderFactory fetch_policy_url_loader_factory;
  base::test::TestFuture<bool> future;
  signin_service->FetchPolicyForSignedInUser(
      test_account_id_, "dm_token", "client-id", std::vector<std::string>(),
      fetch_policy_url_loader_factory.GetSafeWeakWrapper(),
      future.GetCallback());

  // Complete the store initialization with the registration info.
  auto data = std::make_unique<enterprise_management::PolicyData>();
  data->set_request_token("fake token");
  data->set_device_id("fake client id");
  mock_store_->set_policy_data_for_testing(std::move(data));

  // Since there is a signed-in user expect a policy fetch to be started to
  // refresh the policy for the user.
  DeviceManagementService::JobConfiguration::JobType job_type_1 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType job_type_2 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobForTesting job;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type_1),
                      SaveArg<0>(&job)))
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type_2),
                      SaveArg<0>(&job)));
  // A task to trigger policy fetch should have been posted to the task queue.

  mock_store_->NotifyStoreLoaded();
  // The client should be registered.
  ASSERT_TRUE(manager_->IsClientRegistered());

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&job_creation_handler_);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS,
            job_type_1);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_2);

  EXPECT_CALL(*mock_store_, Store(_));
  // Complete the policy fetch request.
  em::DeviceManagementResponse policy_fetch_response;
  UserPolicyBuilder policy_builder;
  policy_builder.Build();
  policy_fetch_response.mutable_policy_response()->add_responses()->CopyFrom(
      policy_builder.policy());
  device_management_service_.SendJobOKNow(&job, policy_fetch_response);
  // The callback isn't called until `Store()` completes.
  ASSERT_FALSE(future.IsReady());
  Mock::VerifyAndClearExpectations(mock_store_);

  mock_store_->NotifyStoreLoaded();
  // `FetchPolicyForSignedInUser()` callback should be executed.
  ASSERT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get());
}

#endif  // !BUILDFLAG(IS_ANDROID)

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
  DeviceManagementService::JobForTesting job;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type),
                      SaveArg<0>(&job)));

  // Now mimic the user being a hosted domain - this should cause a Register()
  // call.
  ReportHostedDomainStatus(true);

  // Should have no more outstanding requests.
  ASSERT_FALSE(IsRequestActive());
  Mock::VerifyAndClearExpectations(&job_creation_handler_);
  ASSERT_TRUE(job.IsActive());
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type);
  EXPECT_FALSE(register_completed_);

  // Make client registration fail (hosted domain user that is not managed).
  device_management_service_.SendJobResponseNow(
      &job, net::OK, DeviceManagementService::kDeviceManagementNotAllowed,
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
  DeviceManagementService::JobForTesting job;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type),
                      SaveArg<0>(&job)));

  // Now mimic the user being a hosted domain - this should cause a Register()
  // call.
  ReportHostedDomainStatus(true);

  // Should have no more outstanding requests.
  ASSERT_FALSE(IsRequestActive());
  Mock::VerifyAndClearExpectations(&job_creation_handler_);
  ASSERT_TRUE(job.IsActive());
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type);
  EXPECT_FALSE(register_completed_);

  em::DeviceManagementResponse registration_response;
  std::string expected_dm_token = "dm_token";
  registration_response.mutable_register_response()
      ->set_device_management_token(expected_dm_token);
  registration_response.mutable_register_response()->set_enrollment_type(
      em::DeviceRegisterResponse::ENTERPRISE);
  device_management_service_.SendJobOKNow(&job, registration_response);

  EXPECT_TRUE(register_completed_);
  EXPECT_EQ(dm_token_, expected_dm_token);
  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());
}

// Tests `FetchPolicyForSignedInUser()` with no active client.
TEST_F(UserPolicySigninServiceTest, FetchPolicyForSignedInUser) {
  mock_store_->NotifyStoreLoaded();
  identity_test_env()->MakeAccountAvailable(kTestUser);

  // `FetchPolicyForSignedInUser()` will create a new registered client and
  // fetch policies with it.
  DeviceManagementService::JobConfiguration::JobType job_type_1 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType job_type_2 =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  em::DeviceManagementRequest policy_fetch_request;
  DeviceManagementService::JobForTesting job;
  base::MockCallback<CloudPolicyClient::DeviceDMTokenCallback>
      device_dm_token_callback;
  std::string device_dm_token = "device-dm-token";
  std::string user_affiliation_id = "user-affiliation_id";

  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(device_management_service_.CaptureJobType(&job_type_1),
                      SaveArg<0>(&job)))
      .WillOnce(DoAll(
          device_management_service_.CaptureJobType(&job_type_2),
          device_management_service_.CaptureRequest(&policy_fetch_request),
          SaveArg<0>(&job)));
  EXPECT_CALL(device_dm_token_callback,
              Run(::testing::ElementsAre(user_affiliation_id)))
      .WillOnce(Return(device_dm_token));

  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());
  network::TestURLLoaderFactory fetch_policy_url_loader_factory;
  base::test::TestFuture<bool> future;

  signin_service->SetDeviceDMTokenCallbackForTesting(
      device_dm_token_callback.Get());
  signin_service->FetchPolicyForSignedInUser(
      test_account_id_, "dm_token", "client-id", {user_affiliation_id},
      fetch_policy_url_loader_factory.GetSafeWeakWrapper(),
      future.GetCallback());
  // The client should be registered.
  ASSERT_TRUE(manager_->IsClientRegistered());
  // A task to trigger policy fetch should have been posted to the task queue.
  // Let it execute.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&job_creation_handler_);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS,
            job_type_1);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_2);

  EXPECT_EQ(
      device_dm_token,
      policy_fetch_request.policy_request().requests(0).device_dm_token());

  // Complete the policy fetch request.
  EXPECT_CALL(*mock_store_, Store(_));
  em::DeviceManagementResponse policy_fetch_response;
  UserPolicyBuilder policy_builder;
  policy_builder.Build();
  policy_fetch_response.mutable_policy_response()->add_responses()->CopyFrom(
      policy_builder.policy());
  device_management_service_.SendJobOKNow(&job, policy_fetch_response);
  // The callback isn't called until `Store()` completes.
  ASSERT_FALSE(future.IsReady());
  Mock::VerifyAndClearExpectations(mock_store_);

  mock_store_->NotifyStoreLoaded();
  // `FetchPolicyForSignedInUser()` callback should be executed.
  ASSERT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get());
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
