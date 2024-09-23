// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/user_cloud_policy_token_forwarder.h"

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kEmail[] = "email@gmail.com";
constexpr char kGaiaId[] = "gaia_id";
constexpr char kOAuthToken[] = "oauth_token";

constexpr base::TimeDelta kTokenLifetime = base::Minutes(30);

}  // namespace

// Mock of UserCloudPolicyManagerAsh used to verify calls from
// UserCloudPolicyTokenForwarder.
class MockUserCloudPolicyManagerAsh : public UserCloudPolicyManagerAsh {
 public:
  MockUserCloudPolicyManagerAsh(
      Profile* profile,
      const AccountId& account_id,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : UserCloudPolicyManagerAsh(
            profile,
            std::make_unique<MockCloudPolicyStore>(),
            std::make_unique<MockCloudExternalDataManager>(),
            base::FilePath() /* component_policy_cache_path */,
            UserCloudPolicyManagerAsh::PolicyEnforcement::kPolicyRequired,
            g_browser_process->local_state(),
            base::Minutes(1) /* policy_refresh_timeout */,
            base::BindOnce(&MockUserCloudPolicyManagerAsh::OnFatalError,
                           base::Unretained(this)),
            account_id,
            task_runner) {}

  MockUserCloudPolicyManagerAsh(const MockUserCloudPolicyManagerAsh&) = delete;
  MockUserCloudPolicyManagerAsh& operator=(
      const MockUserCloudPolicyManagerAsh&) = delete;

  ~MockUserCloudPolicyManagerAsh() override = default;

  MOCK_METHOD1(OnAccessTokenAvailable, void(const std::string&));

 private:
  void OnFatalError() {}
};

class UserCloudPolicyTokenForwarderTest : public testing::Test {
 public:
  UserCloudPolicyTokenForwarderTest(const UserCloudPolicyTokenForwarderTest&) =
      delete;
  UserCloudPolicyTokenForwarderTest& operator=(
      const UserCloudPolicyTokenForwarderTest&) = delete;

 protected:
  static ash::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  UserCloudPolicyTokenForwarderTest()
      : mock_time_task_runner_(
            base::MakeRefCounted<base::TestMockTimeTaskRunner>()),
        user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())),
        store_(std::make_unique<MockCloudPolicyStore>()) {}

  ~UserCloudPolicyTokenForwarderTest() override = default;

  void SetUp() override {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ASSERT_TRUE(profile_manager_->SetUp());
    scoped_feature_list_.InitAndEnableFeature(
        features::kDMServerOAuthForChildUser);
  }

  void TearDown() override {
    user_policy_manager_->core()->Disconnect();
    // Must be torn down before |profile_manager_|.
    user_policy_manager_.reset();
    ash::ConciergeClient::Shutdown();
  }

  // Creates user with given |user_type|. Initializes identity test environment
  // and user policy manager.
  void CreateUserWithType(user_manager::UserType user_type) {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kEmail, kGaiaId);
    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        account_id.GetUserEmail(),
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(account_id.GetUserEmail()), 0 /* avatar_id */,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile);
    identity_test_env_profile_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(kEmail, signin::ConsentLevel::kSignin);

    auto* user_manager = GetFakeUserManager();
    user_manager->AddUser(account_id);
    user_manager->AddUserWithAffiliationAndTypeAndProfile(
        account_id, false /* is_affiliated */, user_type, profile);
    user_manager->SwitchActiveUser(account_id);
    ASSERT_TRUE(user_manager->GetActiveUser());

    user_policy_manager_ = std::make_unique<MockUserCloudPolicyManagerAsh>(
        profile, account_id, mock_time_task_runner_);
    std::unique_ptr<MockCloudPolicyClient> client =
        std::make_unique<MockCloudPolicyClient>();
    CloudPolicyClient* client_ptr = client.get();
    user_policy_manager_->core()->ConnectForTesting(
        std::make_unique<MockCloudPolicyService>(client_ptr, store_.get()),
        std::move(client));
  }

  // Creates token forwarder for tests. Should be called after user is created
  // with CreateUserWithType().
  std::unique_ptr<UserCloudPolicyTokenForwarder> CreateTokenForwarder() {
    auto token_forwarder = std::make_unique<UserCloudPolicyTokenForwarder>(
        user_policy_manager_.get(),
        identity_test_env_profile_adaptor_->identity_test_env()
            ->identity_manager());
    token_forwarder->OverrideTimeForTesting(
        mock_time_task_runner_->GetMockClock(),
        mock_time_task_runner_->GetMockTickClock(), mock_time_task_runner_);
    return token_forwarder;
  }

  // Issues OAuth token for device management scope for any pending token
  // requests. Blocks waiting for the request if there are no pending requests.
  void IssueOAuthToken(const std::string& token, base::Time expiration) {
    signin::ScopeSet scopes;
    scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
    scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
    identity_test_env_profile_adaptor_->identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
            token, expiration, std::string() /*id_token*/, scopes);
  }

  // Issues OAuth token error for any pending token requests. Blocks waiting for
  // the request if there are no pending requests.
  void IssueOAuthTokenError() {
    identity_test_env_profile_adaptor_->identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
            GoogleServiceAuthError(
                GoogleServiceAuthError::State::SERVICE_UNAVAILABLE));
  }

  // Simulates CloudPolicyService changing state to initialized.
  // CloudPolicyService waits for the store to be initialized. When
  // NotifyStoreLoaded() is called the service updates its state to initialized
  // and informs listeners by calling
  // OnCloudPolicyServiceInitializationCompleted().
  void SimulateCloudPolicyServiceInitialized() { store_->NotifyStoreLoaded(); }

  content::BrowserTaskEnvironment task_environment_;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<MockUserCloudPolicyManagerAsh> user_policy_manager_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

 private:
  user_manager::ScopedUserManager user_manager_enabler_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<MockCloudPolicyStore> store_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UserCloudPolicyTokenForwarderTest,
       RegularUserWaitingForServiceInitialization) {
  CreateUserWithType(user_manager::UserType::kRegular);

  // Initialized CloudPolicyService is needed to start token fetch.
  // Simulate CloudPolicyService initialization after token forwarder was
  // created. Token forwarder should wait with sending request.
  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder =
      CreateTokenForwarder();
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  SimulateCloudPolicyServiceInitialized();
  EXPECT_TRUE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  EXPECT_CALL(*user_policy_manager_, OnAccessTokenAvailable(kOAuthToken))
      .Times(1);
  IssueOAuthToken(kOAuthToken, mock_time_task_runner_->Now() + kTokenLifetime);
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  histogram_tester_.ExpectTotalCount(
      UserCloudPolicyTokenForwarder::kUMAChildUserOAuthTokenError, 0);
}

TEST_F(UserCloudPolicyTokenForwarderTest, RegularUserServiceInitialized) {
  CreateUserWithType(user_manager::UserType::kRegular);

  // Initialized CloudPolicyService is needed to start token fetch.
  // Simulate CloudPolicyService initialization before token forwarder was
  // created. Token forwarder should send request right away.
  SimulateCloudPolicyServiceInitialized();

  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder =
      CreateTokenForwarder();
  EXPECT_TRUE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  histogram_tester_.ExpectTotalCount(
      UserCloudPolicyTokenForwarder::kUMAChildUserOAuthTokenError, 0);
}

TEST_F(UserCloudPolicyTokenForwarderTest,
       RegularUserShutdownBeforeTokenFetched) {
  CreateUserWithType(user_manager::UserType::kRegular);

  SimulateCloudPolicyServiceInitialized();

  EXPECT_CALL(*user_policy_manager_, OnAccessTokenAvailable(kOAuthToken))
      .Times(0);

  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder =
      CreateTokenForwarder();
  EXPECT_TRUE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  token_forwarder->Shutdown();
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  histogram_tester_.ExpectTotalCount(
      UserCloudPolicyTokenForwarder::kUMAChildUserOAuthTokenError, 0);
}

TEST_F(UserCloudPolicyTokenForwarderTest, RegularUserTokenFetchFailed) {
  CreateUserWithType(user_manager::UserType::kRegular);

  SimulateCloudPolicyServiceInitialized();

  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder =
      CreateTokenForwarder();
  EXPECT_TRUE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  EXPECT_CALL(*user_policy_manager_, OnAccessTokenAvailable(kOAuthToken))
      .Times(0);
  IssueOAuthTokenError();
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  histogram_tester_.ExpectTotalCount(
      UserCloudPolicyTokenForwarder::kUMAChildUserOAuthTokenError, 0);
}

TEST_F(UserCloudPolicyTokenForwarderTest,
       ChildUserWaitingForServiceInitialization) {
  CreateUserWithType(user_manager::UserType::kChild);

  // Initialized CloudPolicyService is needed to start token fetch.
  // Simulate CloudPolicyService initialization after token forwarder was
  // created. Token forwarder should wait with sending request.
  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder =
      CreateTokenForwarder();
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  SimulateCloudPolicyServiceInitialized();
  EXPECT_TRUE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  EXPECT_CALL(*user_policy_manager_, OnAccessTokenAvailable(kOAuthToken))
      .Times(1);
  IssueOAuthToken(kOAuthToken, mock_time_task_runner_->Now() + kTokenLifetime);
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_TRUE(token_forwarder->IsTokenRefreshScheduledForTesting());
  EXPECT_EQ(token_forwarder->GetTokenRefreshDelayForTesting(), kTokenLifetime);

  token_forwarder->Shutdown();
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  histogram_tester_.ExpectUniqueSample(
      UserCloudPolicyTokenForwarder::kUMAChildUserOAuthTokenError,
      GoogleServiceAuthError::State::NONE, 1);
}

TEST_F(UserCloudPolicyTokenForwarderTest, ChildUserServiceInitialized) {
  CreateUserWithType(user_manager::UserType::kChild);

  // Initialized CloudPolicyService is needed to start token fetch.
  // Simulate CloudPolicyService initialization before token forwarder was
  // created. Token forwarder should send request right away.
  SimulateCloudPolicyServiceInitialized();

  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder =
      CreateTokenForwarder();
  EXPECT_TRUE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  histogram_tester_.ExpectTotalCount(
      UserCloudPolicyTokenForwarder::kUMAChildUserOAuthTokenError, 0);
}

TEST_F(UserCloudPolicyTokenForwarderTest, ChildUserShutdownBeforeTokenFetched) {
  CreateUserWithType(user_manager::UserType::kChild);

  SimulateCloudPolicyServiceInitialized();

  EXPECT_CALL(*user_policy_manager_.get(), OnAccessTokenAvailable(kOAuthToken))
      .Times(0);

  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder =
      CreateTokenForwarder();
  EXPECT_TRUE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  token_forwarder->Shutdown();
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  histogram_tester_.ExpectTotalCount(
      UserCloudPolicyTokenForwarder::kUMAChildUserOAuthTokenError, 0);
}

TEST_F(UserCloudPolicyTokenForwarderTest, ChildUserExpiredToken) {
  CreateUserWithType(user_manager::UserType::kChild);

  SimulateCloudPolicyServiceInitialized();

  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder =
      CreateTokenForwarder();

  EXPECT_CALL(*user_policy_manager_, OnAccessTokenAvailable(kOAuthToken))
      .Times(1);
  IssueOAuthToken(kOAuthToken, mock_time_task_runner_->Now() - kTokenLifetime);
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_TRUE(token_forwarder->IsTokenRefreshScheduledForTesting());
  // If the token fetch fails then next token fetch is scheduled according to
  // backoff policy.
  const double max_initial_retry_delay =
      UserCloudPolicyTokenForwarder::kFetchTokenRetryBackoffPolicy
          .initial_delay_ms *
      UserCloudPolicyTokenForwarder::kFetchTokenRetryBackoffPolicy
          .multiply_factor;
  const double retry_delay =
      token_forwarder->GetTokenRefreshDelayForTesting()->InMilliseconds();
  EXPECT_LE(retry_delay, max_initial_retry_delay);
  EXPECT_GT(retry_delay, 0);

  token_forwarder->Shutdown();
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  histogram_tester_.ExpectUniqueSample(
      UserCloudPolicyTokenForwarder::kUMAChildUserOAuthTokenError,
      GoogleServiceAuthError::State::NONE, 1);
}

TEST_F(UserCloudPolicyTokenForwarderTest, ChildUserTokenFetchFailed) {
  CreateUserWithType(user_manager::UserType::kChild);

  SimulateCloudPolicyServiceInitialized();

  EXPECT_CALL(*user_policy_manager_, OnAccessTokenAvailable(kOAuthToken))
      .Times(0);

  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder =
      CreateTokenForwarder();

  IssueOAuthTokenError();
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_TRUE(token_forwarder->IsTokenRefreshScheduledForTesting());
  // If the token fetch fails then next token fetch is scheduled according to
  // backoff policy.
  const double max_initial_retry_delay =
      UserCloudPolicyTokenForwarder::kFetchTokenRetryBackoffPolicy
          .initial_delay_ms *
      UserCloudPolicyTokenForwarder::kFetchTokenRetryBackoffPolicy
          .multiply_factor;
  const double retry_delay =
      token_forwarder->GetTokenRefreshDelayForTesting()->InMilliseconds();
  EXPECT_LE(retry_delay, max_initial_retry_delay);
  EXPECT_GT(retry_delay, 0);

  token_forwarder->Shutdown();
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  histogram_tester_.ExpectUniqueSample(
      UserCloudPolicyTokenForwarder::kUMAChildUserOAuthTokenError,
      GoogleServiceAuthError::State::SERVICE_UNAVAILABLE, 1);
}

TEST_F(UserCloudPolicyTokenForwarderTest, ChildUserRecurringTokenFetch) {
  CreateUserWithType(user_manager::UserType::kChild);
  SimulateCloudPolicyServiceInitialized();
  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder =
      CreateTokenForwarder();

  // First token fetch should schedule another fetch when token lifetime ends.
  EXPECT_CALL(*user_policy_manager_, OnAccessTokenAvailable(kOAuthToken))
      .Times(1);
  IssueOAuthToken(kOAuthToken, mock_time_task_runner_->Now() + kTokenLifetime);
  EXPECT_EQ(token_forwarder->GetTokenRefreshDelayForTesting(), kTokenLifetime);

  // Advance the time long enough that new token request is posted.
  mock_time_task_runner_->FastForwardBy(kTokenLifetime);
  EXPECT_TRUE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_FALSE(token_forwarder->IsTokenRefreshScheduledForTesting());

  // Issue new token and observe that new fetch was scheduled (different token
  // lifetime).
  EXPECT_CALL(*user_policy_manager_, OnAccessTokenAvailable(kOAuthToken))
      .Times(1);
  IssueOAuthToken(kOAuthToken,
                  mock_time_task_runner_->Now() + kTokenLifetime * 2);
  EXPECT_FALSE(token_forwarder->IsTokenFetchInProgressForTesting());
  EXPECT_TRUE(token_forwarder->IsTokenRefreshScheduledForTesting());
  EXPECT_EQ(token_forwarder->GetTokenRefreshDelayForTesting(),
            kTokenLifetime * 2);

  token_forwarder->Shutdown();

  histogram_tester_.ExpectUniqueSample(
      UserCloudPolicyTokenForwarder::kUMAChildUserOAuthTokenError,
      GoogleServiceAuthError::NONE, 2);
}

}  // namespace policy
