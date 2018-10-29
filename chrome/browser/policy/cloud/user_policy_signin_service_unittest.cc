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
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_account_fetcher_service_builder.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
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
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
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

using testing::AnyNumber;
using testing::Mock;
using testing::_;

namespace policy {

namespace {

constexpr char kTestGaiaId[] = "gaia-id-testuser@test.com";
constexpr char kTestUser[] = "testuser@test.com";

#if !defined(OS_ANDROID)
constexpr char kValidTokenResponse[] = R"(
    {
      "access_token": "at1",
      "expires_in": 3600,
      "token_type": "Bearer"
    })";
#endif

constexpr char kHostedDomainResponse[] = R"(
    {
      "hd": "test.com"
    })";

UserCloudPolicyManager* BuildCloudPolicyManager(
    content::BrowserContext* context) {
  MockUserCloudPolicyStore* store = new MockUserCloudPolicyStore();
  EXPECT_CALL(*store, Load()).Times(AnyNumber());

  return new UserCloudPolicyManager(
      std::unique_ptr<UserCloudPolicyStore>(store), base::FilePath(),
      std::unique_ptr<CloudExternalDataManager>(),
      base::ThreadTaskRunnerHandle::Get(),
      network::TestNetworkConnectionTracker::CreateGetter());
}

class UserPolicySigninServiceTest : public testing::Test {
 public:
  UserPolicySigninServiceTest()
      : mock_store_(NULL),
        thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        test_account_id_(
            AccountId::FromUserEmailGaiaId(kTestUser, kTestGaiaId)),
        register_completed_(false),
        test_system_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  MOCK_METHOD1(OnPolicyRefresh, void(bool));

  void OnRegisterCompleted(const std::string& dm_token,
                           const std::string& client_id) {
    register_completed_ = true;
    dm_token_ = dm_token;
    client_id_ = client_id;
  }

  void RegisterPolicyClientWithCallback(UserPolicySigninService* service) {
    // Policy client registration on Android depends on Token Service having
    // a valid login token, while on other platforms, the login refresh token
    // is specified directly.
    UserPolicySigninServiceBase::PolicyRegistrationCallback callback =
        base::Bind(&UserPolicySigninServiceTest::OnRegisterCompleted,
                   base::Unretained(this));
#if defined(OS_ANDROID)
    GetTokenService()->UpdateCredentials(
        AccountTrackerServiceFactory::GetForProfile(profile_.get())
            ->SeedAccountInfo(kTestGaiaId, kTestUser),
        "oauth2_login_refresh_token");
    service->RegisterForPolicyWithAccountId(kTestUser, kTestGaiaId, callback);
    ASSERT_TRUE(IsRequestActive());
#else
    service->RegisterForPolicyWithLoginToken(kTestUser, "mock_oauth_token",
                                             callback);
    ASSERT_TRUE(IsRequestActive());
#endif
  }

  void SetUp() override {
    UserPolicySigninServiceFactory::SetDeviceManagementServiceForTesting(
        &device_management_service_);

    local_state_.reset(new TestingPrefServiceSimple);
    RegisterLocalState(local_state_->registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(local_state_.get());
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_system_shared_loader_factory_);

    g_browser_process->browser_policy_connector()->Init(
        local_state_.get(), test_system_shared_loader_factory_);

    // Create a testing profile with cloud-policy-on-signin enabled, and bring
    // up a UserCloudPolicyManager with a MockUserCloudPolicyStore.
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs(
        new sync_preferences::TestingPrefServiceSyncable());
    RegisterUserProfilePrefs(prefs->registry());

    // UserCloudPolicyManagerFactory isn't a real
    // BrowserContextKeyedServiceFactory (it derives from
    // BrowserContextKeyedBaseFactory and exposes its own APIs to get
    // instances) so we have to inject our testing factory via a special
    // API before creating the profile.
    UserCloudPolicyManagerFactory::GetInstance()->RegisterTestingFactory(
        base::BindRepeating(&BuildCloudPolicyManager));
    TestingProfile::Builder builder;
    builder.SetPrefService(
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(
            std::move(prefs)));
    builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(&BuildFakeSigninManagerForTesting));
    builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeProfileOAuth2TokenService));
    builder.AddTestingFactory(
        AccountFetcherServiceFactory::GetInstance(),
        base::BindRepeating(&FakeAccountFetcherServiceBuilder::BuildForTests));
    builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));

    profile_ = builder.Build();

    signin_manager_ = static_cast<FakeSigninManager*>(
        SigninManagerFactory::GetForProfile(profile_.get()));
    // Tests are responsible for freeing the UserCloudPolicyManager instances
    // they inject.
    manager_.reset(UserCloudPolicyManagerFactory::GetForBrowserContext(
        profile_.get()));
    manager_->Init(&schema_registry_);
    mock_store_ = static_cast<MockUserCloudPolicyStore*>(
        manager_->core()->store());
    DCHECK(mock_store_);
    AddProfile();

    Mock::VerifyAndClearExpectations(mock_store_);
  }

  void TearDown() override {
    UserPolicySigninServiceFactory::SetDeviceManagementServiceForTesting(NULL);
    UserCloudPolicyManagerFactory::GetInstance()->ClearTestingFactory();
    // Free the profile before we clear out the browser prefs.
    profile_.reset();
    TestingBrowserProcess* testing_browser_process =
        TestingBrowserProcess::GetGlobal();
    testing_browser_process->SetLocalState(NULL);
    local_state_.reset();
    testing_browser_process->ShutdownBrowserPolicyConnector();
    test_system_shared_loader_factory_->Detach();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  virtual void AddProfile() {
    // For this test, the user should not be signed in yet.
    DCHECK(!signin_manager_->IsAuthenticated());

    // Initializing UserPolicySigninService while the user is not signed in
    // should result in the store being cleared to remove any lingering policy.
    EXPECT_CALL(*mock_store_, Clear());

    // Let the SigninService know that the profile has been created.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_ADDED,
        content::Source<Profile>(profile_.get()),
        content::NotificationService::NoDetails());
  }

  FakeProfileOAuth2TokenService* GetTokenService() {
    ProfileOAuth2TokenService* service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get());
    return static_cast<FakeProfileOAuth2TokenService*>(service);
  }

  bool IsRequestActive() {
    if (!GetTokenService()->GetPendingRequests().empty())
      return true;
    return test_url_loader_factory_.NumPending() > 0;
  }

  void MakeOAuthTokenFetchSucceed() {
#if defined(OS_ANDROID)
    ASSERT_TRUE(IsRequestActive());
    GetTokenService()->IssueTokenForAllPendingRequests("access_token",
                                                       base::Time::Now());
#else
    ASSERT_TRUE(IsRequestActive());
    test_url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(),
        kValidTokenResponse);
    base::RunLoop().RunUntilIdle();
    test_url_loader_factory_.ClearResponses();
#endif
  }

  void MakeOAuthTokenFetchFail() {
#if defined(OS_ANDROID)
    ASSERT_TRUE(!GetTokenService()->GetPendingRequests().empty());
    GetTokenService()->IssueErrorForAllPendingRequests(
        GoogleServiceAuthError::FromServiceError("fail"));
#else
    ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(), "",
        net::HTTP_BAD_REQUEST));
#endif
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
    signin_manager_->SignIn(kTestGaiaId, kTestUser, "");

    // Mimic successful oauth token fetch.
    MakeOAuthTokenFetchSucceed();

    // When the user is from a hosted domain, this should kick off client
    // registration.
    MockDeviceManagementJob* register_request = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION, _))
        .WillOnce(device_management_service_.CreateAsyncJob(
            &register_request));
    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(1);

    // Now mimic the user being a hosted domain - this should cause a Register()
    // call.
    ReportHostedDomainStatus(true);

    // Should have no more outstanding requests.
    ASSERT_FALSE(IsRequestActive());
    Mock::VerifyAndClearExpectations(this);
    ASSERT_TRUE(register_request);

    // Mimic successful client registration - this should register the client
    // and invoke the callback.
    em::DeviceManagementResponse registration_blob;
    std::string expected_dm_token = "dm_token";
    registration_blob.mutable_register_response()->set_device_management_token(
        expected_dm_token);
    registration_blob.mutable_register_response()->set_enrollment_type(
        em::DeviceRegisterResponse::ENTERPRISE);
    register_request->SendResponse(DM_STATUS_SUCCESS, registration_blob);

    // UserCloudPolicyManager should not be initialized yet.
    ASSERT_FALSE(manager_->core()->service());
    EXPECT_TRUE(register_completed_);
    EXPECT_EQ(dm_token_, expected_dm_token);

    // Now call to fetch policy - this should fire off a fetch request.
    MockDeviceManagementJob* fetch_request = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
        .WillOnce(device_management_service_.CreateAsyncJob(&fetch_request));
    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(1);

    signin_service->FetchPolicyForSignedInUser(
        test_account_id_, dm_token_, client_id_,
        test_system_shared_loader_factory_,
        base::Bind(&UserPolicySigninServiceTest::OnPolicyRefresh,
                   base::Unretained(this)));

    Mock::VerifyAndClearExpectations(this);
    ASSERT_TRUE(fetch_request);

    // UserCloudPolicyManager should now be initialized.
    EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
    ASSERT_TRUE(manager_->core()->service());

    // Make the policy fetch succeed - this should result in a write to the
    // store and ultimately result in a call to OnPolicyRefresh().
    EXPECT_CALL(*mock_store_, Store(_));
    EXPECT_CALL(*this, OnPolicyRefresh(true)).Times(1);

    // Create a fake policy blob to deliver to the client.
    em::DeviceManagementResponse policy_blob;
    em::PolicyData policy_data;
    policy_data.set_policy_type(dm_protocol::kChromeUserPolicyType);
    em::PolicyFetchResponse* policy_response =
        policy_blob.mutable_policy_response()->add_response();
    ASSERT_TRUE(policy_data.SerializeToString(
        policy_response->mutable_policy_data()));
    fetch_request->SendResponse(DM_STATUS_SUCCESS, policy_blob);

    // Complete the store which should cause the policy fetch callback to be
    // invoked.
    mock_store_->NotifyStoreLoaded();
    Mock::VerifyAndClearExpectations(this);
  }

  std::unique_ptr<TestingProfile> profile_;
  MockUserCloudPolicyStore* mock_store_;  // Not owned.
  SchemaRegistry schema_registry_;
  std::unique_ptr<UserCloudPolicyManager> manager_;

  // BrowserPolicyConnector and UrlFetcherFactory want to initialize and free
  // various components asynchronously via tasks, so create fake threads here.
  content::TestBrowserThreadBundle thread_bundle_;

  FakeSigninManager* signin_manager_;

  // Used in conjunction with OnRegisterCompleted() to test client registration
  // callbacks.
  std::string dm_token_;
  std::string client_id_;

  // AccountId for the test user.
  AccountId test_account_id_;

  // True if OnRegisterCompleted() was called.
  bool register_completed_;

  // Weak ptr to the MockDeviceManagementService (object is owned by the
  // BrowserPolicyConnector).
  MockDeviceManagementService device_management_service_;

  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_system_shared_loader_factory_;
};

class UserPolicySigninServiceSignedInTest : public UserPolicySigninServiceTest {
 public:
  void AddProfile() override {
    // UserCloudPolicyManager should not be initialized.
    ASSERT_FALSE(manager_->core()->service());

    // Set the user as signed in.
    SigninManagerFactory::GetForProfile(profile_.get())->
        SetAuthenticatedAccountInfo(kTestGaiaId, kTestUser);

    // Let the SigninService know that the profile has been created.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_ADDED,
        content::Source<Profile>(profile_.get()),
        content::NotificationService::NoDetails());
  }
};

TEST_F(UserPolicySigninServiceTest, InitWhileSignedOut) {
  // Make sure user is not signed in.
  ASSERT_FALSE(SigninManagerFactory::GetForProfile(profile_.get())->
      IsAuthenticated());

  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());
}

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
TEST_F(UserPolicySigninServiceTest, InitRefreshTokenAvailableBeforeSignin) {
  // Make sure user is not signed in.
  ASSERT_FALSE(
      SigninManagerFactory::GetForProfile(profile_.get())->IsAuthenticated());

  // No oauth access token yet, so client registration should be deferred.
  ASSERT_FALSE(IsRequestActive());

  // Make oauth token available.
  std::string account_id =
      AccountTrackerServiceFactory::GetForProfile(profile_.get())
          ->SeedAccountInfo(kTestGaiaId, kTestUser);
  GetTokenService()->UpdateCredentials(account_id, "oauth_login_refresh_token");

  // Not signed in yet, so client registration should be deferred.
  ASSERT_FALSE(IsRequestActive());

  // Sign in to Chrome.
  signin_manager_->SignIn(kTestGaiaId, kTestUser, "");

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Client registration should be in progress since we now have an oauth token
  // for the authenticated account id.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(IsRequestActive());
}
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

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
  GetTokenService()->UpdateCredentials(
      SigninManagerFactory::GetForProfile(profile_.get())
          ->GetAuthenticatedAccountId(),
      "oauth_login_refresh_token");

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
  GetTokenService()->UpdateCredentials(
      SigninManagerFactory::GetForProfile(profile_.get())
          ->GetAuthenticatedAccountId(),
      "oauth_login_refresh_token");

  // Client registration should be in progress since we now have an oauth token.
  ASSERT_TRUE(IsRequestActive());

  // Now fail the access token fetch.
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  GetTokenService()->IssueErrorForAllPendingRequests(error);
  ASSERT_FALSE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceTest, SignInAfterInit) {
  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in the user.
  SigninManagerFactory::GetForProfile(profile_.get())
      ->SetAuthenticatedAccountInfo(kTestGaiaId, kTestUser);

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Make oauth token available.
  GetTokenService()->UpdateCredentials(
      SigninManagerFactory::GetForProfile(profile_.get())
          ->GetAuthenticatedAccountId(),
      "oauth_login_refresh_token");

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
  SigninManagerFactory::GetForProfile(profile_.get())
      ->SetAuthenticatedAccountInfo("gaia-id-non_enterprise_user@gmail.com",
                                    "non_enterprise_user@gmail.com");

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Make oauth token available.
  GetTokenService()->UpdateCredentials(
      SigninManagerFactory::GetForProfile(profile_.get())
          ->GetAuthenticatedAccountId(),
      "oauth_login_refresh_token");

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
  SigninManagerFactory::GetForProfile(profile_.get())
      ->SetAuthenticatedAccountInfo(kTestGaiaId, kTestUser);

  // Make oauth token available.
  GetTokenService()->UpdateCredentials(
      SigninManagerFactory::GetForProfile(profile_.get())
          ->GetAuthenticatedAccountId(),
      "oauth_login_refresh_token");

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
  SigninManagerFactory::GetForProfile(profile_.get())
      ->SetAuthenticatedAccountInfo(kTestGaiaId, kTestUser);

  // Make oauth token available.
  GetTokenService()->UpdateCredentials(
      SigninManagerFactory::GetForProfile(profile_.get())
          ->GetAuthenticatedAccountId(),
      "oauth_login_refresh_token");

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
  MockDeviceManagementJob* fetch_request = nullptr;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
      .WillOnce(device_management_service_.CreateAsyncJob(&fetch_request));

  // Client registration should not be in progress since the client should be
  // already registered.
  ASSERT_TRUE(manager_->IsClientRegistered());
  ASSERT_FALSE(IsRequestActive());
}

#endif  // !defined(OS_ANDROID)

TEST_F(UserPolicySigninServiceSignedInTest, SignOutAfterInit) {
  // UserCloudPolicyManager should be initialized.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());

  // Signing out will clear the policy from the store.
  EXPECT_CALL(*mock_store_, Clear());

  // Now sign out.
  SigninManagerFactory::GetForProfile(profile_.get())
      ->SignOut(signin_metrics::SIGNOUT_TEST,
                signin_metrics::SignoutDelete::IGNORE_METRIC);

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
  MockDeviceManagementJob* register_request = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION, _))
      .WillOnce(device_management_service_.CreateAsyncJob(&register_request));
  EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
      .Times(1);

  // Now mimic the user being a hosted domain - this should cause a Register()
  // call.
  ReportHostedDomainStatus(true);

  // Should have no more outstanding requests.
  ASSERT_FALSE(IsRequestActive());
  Mock::VerifyAndClearExpectations(this);
  ASSERT_TRUE(register_request);
  EXPECT_FALSE(register_completed_);

  // Make client registration fail (hosted domain user that is not managed).
  register_request->SendResponse(DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED,
                                 em::DeviceManagementResponse());
  EXPECT_TRUE(register_completed_);
  EXPECT_TRUE(dm_token_.empty());
}

TEST_F(UserPolicySigninServiceTest, RegisterPolicyClientSucceeded) {
  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());
  RegisterPolicyClientWithCallback(signin_service);

  // Mimic successful oauth token fetch.
  MakeOAuthTokenFetchSucceed();

  // When the user is from a hosted domain, this should kick off client
  // registration.
  MockDeviceManagementJob* register_request = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION, _))
      .WillOnce(device_management_service_.CreateAsyncJob(&register_request));
  EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
      .Times(1);

  // Now mimic the user being a hosted domain - this should cause a Register()
  // call.
  ReportHostedDomainStatus(true);

  // Should have no more outstanding requests.
  ASSERT_FALSE(IsRequestActive());
  Mock::VerifyAndClearExpectations(this);
  ASSERT_TRUE(register_request);
  EXPECT_FALSE(register_completed_);

  em::DeviceManagementResponse registration_blob;
  std::string expected_dm_token = "dm_token";
  registration_blob.mutable_register_response()->set_device_management_token(
      expected_dm_token);
  registration_blob.mutable_register_response()->set_enrollment_type(
      em::DeviceRegisterResponse::ENTERPRISE);
  register_request->SendResponse(DM_STATUS_SUCCESS, registration_blob);
  Mock::VerifyAndClearExpectations(this);
  EXPECT_TRUE(register_completed_);
  EXPECT_EQ(dm_token_, expected_dm_token);
  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());
}

TEST_F(UserPolicySigninServiceTest, FetchPolicyFailed) {
  // Initiate a policy fetch request.
  MockDeviceManagementJob* fetch_request = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
      .WillOnce(device_management_service_.CreateAsyncJob(&fetch_request));
  EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
      .Times(1);
  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());
  signin_service->FetchPolicyForSignedInUser(
      test_account_id_, "mock_dm_token", "mock_client_id",
      test_system_shared_loader_factory_,
      base::Bind(&UserPolicySigninServiceTest::OnPolicyRefresh,
                 base::Unretained(this)));
  ASSERT_TRUE(fetch_request);

  // Make the policy fetch fail.
  EXPECT_CALL(*this, OnPolicyRefresh(false)).Times(1);
  fetch_request->SendResponse(DM_STATUS_REQUEST_FAILED,
                              em::DeviceManagementResponse());

  // UserCloudPolicyManager should be initialized.
  EXPECT_EQ(mock_store_->signin_account_id(), test_account_id_);
  ASSERT_TRUE(manager_->core()->service());
}

TEST_F(UserPolicySigninServiceTest, FetchPolicySuccess) {
  ASSERT_NO_FATAL_FAILURE(TestSuccessfulSignin());
}

TEST_F(UserPolicySigninServiceTest, SignOutThenSignInAgain) {
  ASSERT_NO_FATAL_FAILURE(TestSuccessfulSignin());

  EXPECT_CALL(*mock_store_, Clear());
  signin_manager_->ForceSignOut();
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in again.
  ASSERT_NO_FATAL_FAILURE(TestSuccessfulSignin());
}

TEST_F(UserPolicySigninServiceTest, PolicyFetchFailureTemporary) {
  ASSERT_NO_FATAL_FAILURE(TestSuccessfulSignin());

  ASSERT_TRUE(manager_->IsClientRegistered());

  // Kick off another policy fetch.
  MockDeviceManagementJob* fetch_request = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
      .WillOnce(device_management_service_.CreateAsyncJob(&fetch_request));
  EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
      .Times(1);
  manager_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(this);

  // Now, fake a transient error from the server on this policy fetch. This
  // should have no impact on the cached policy.
  fetch_request->SendResponse(DM_STATUS_REQUEST_FAILED,
                              em::DeviceManagementResponse());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(manager_->IsClientRegistered());
}

TEST_F(UserPolicySigninServiceTest, PolicyFetchFailureDisableManagement) {
  ASSERT_NO_FATAL_FAILURE(TestSuccessfulSignin());

  EXPECT_TRUE(manager_->IsClientRegistered());
#if !defined(OS_ANDROID)
  EXPECT_FALSE(signin_util::IsUserSignoutAllowedForProfile(profile_.get()));
#endif

  // Kick off another policy fetch.
  MockDeviceManagementJob* fetch_request = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
      .WillOnce(device_management_service_.CreateAsyncJob(&fetch_request));
  EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
      .Times(1);
  manager_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(this);

  // Now, fake a SC_FORBIDDEN error from the server on this policy fetch. This
  // indicates that chrome management is disabled and will result in the cached
  // policy being removed and the manager shut down.
  EXPECT_CALL(*mock_store_, Clear());
  fetch_request->SendResponse(DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED,
                              em::DeviceManagementResponse());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager_->IsClientRegistered());
#if !defined(OS_ANDROID)
  EXPECT_TRUE(signin_util::IsUserSignoutAllowedForProfile(profile_.get()));
#endif
}

}  // namespace

}  // namespace policy
