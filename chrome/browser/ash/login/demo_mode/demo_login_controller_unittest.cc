// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_login_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/metrics/demo_session_metrics_recorder.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ui/ash/login/mock_login_display_host.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr char kValidGaiaCreds[] =
    R"({
      "username":"example@gmail.com",
      "obfuscatedGaiaId":"%s",
      "authorizationCode":"abc"
    })";

constexpr char kInValidGaiaCreds[] =
    R"({
      "username":"example@gmail.com",
      "gaia_id":"123"
    })";

constexpr char kServerError[] =
    R"({
        "error": {
          "code": 500,
           "message": "Internal error encountered.",
           "status": "INTERNAL",
           "detail":[]
          }
    })";

constexpr char kSetupDemoAccountFailedRetriableResponse[] =
    R"({
      "retryDetails":{},
      "status":{
        "code":8
      }
    })";

constexpr char kSetupDemoAccountUrl[] =
    "https://demomode-pa.googleapis.com/v1/accounts";

constexpr char kCleanUpDemoAccountUrl[] =
    "https://demomode-pa.googleapis.com/v1/accounts:remove";

constexpr char kApiKeyParam[] = "key";

constexpr char kPublicAccountUserId[] = "public_session_user@localhost";

constexpr GaiaId::Literal kTestGaiaId("123");
constexpr char kTestEmail[] = "example@gmail.com";
constexpr char kSessionId[] = "session_id";

constexpr char kSetupDemoAccountRequestResultHistogram[] =
    "DemoMode.SignedIn.Request.SetupResult";
constexpr char kCleanupDemoAccountRequestResultHistogram[] =
    "DemoMode.SignedIn.Request.CleanupResult";

}  // namespace

class DemoLoginControllerTest : public testing::Test {
 protected:
  ash::MockLoginDisplayHost& login_display_host() {
    return mock_login_display_host_;
  }

  DemoLoginController* GetDemoLoginController() {
    return existing_user_controller_->GetDemoLoginControllerForTest();
  }

  void SetUp() override {
    features_.InitAndEnableFeature(features::kDemoModeSignIn);

    settings_helper_.InstallAttributes()->SetDemoMode();
    fake_user_manager_->AddPublicAccountUser(auto_login_account_id_);
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();

    chromeos::PowerManagerClient::InitializeFake();
    chromeos::PowerPolicyController::Initialize(
        chromeos::FakePowerManagerClient::Get());
    policy_controller_ = chromeos::PowerPolicyController::Get();

    base::Value::Dict account;
    account.Set(kAccountsPrefDeviceLocalAccountsKeyId, kPublicAccountUserId);
    account.Set(
        kAccountsPrefDeviceLocalAccountsKeyType,
        static_cast<int>(policy::DeviceLocalAccountType::kPublicSession));
    base::Value::List accounts;
    accounts.Append(std::move(account));
    settings_helper_.Set(kAccountsPrefDeviceLocalAccounts,
                         base::Value(std::move(accounts)));

    auth_events_recorder_ = ash::AuthEventsRecorder::CreateForTesting();

    existing_user_controller_ = std::make_unique<ExistingUserController>();

    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    ExpectGetExistingController();
  }

  // This will trigger `ExistingUserController:ConfigureAutoLogin` since the
  // `ExistingUserController` subscribe these settings.
  void ConfigureAutoLoginSetting() {
    settings_helper()->SetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                 kPublicAccountUserId);
    settings_helper()->SetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay,
                                  0);
  }

  void SetUpPolicyClient(std::string dm_token = "fake-dm-token",
                         std::string client_id = "fake-client-id") {
    std::unique_ptr<policy::MockCloudPolicyStore> store =
        std::make_unique<policy::MockCloudPolicyStore>();
    std::unique_ptr<policy::MockCloudPolicyClient> cloud_policy_client =
        std::make_unique<policy::MockCloudPolicyClient>();
    policy::CloudPolicyClient* client_ptr = cloud_policy_client.get();
    auto service = std::make_unique<policy::MockCloudPolicyService>(
        client_ptr, store.get());

    cloud_policy_client->SetDMToken(dm_token);
    cloud_policy_client->client_id_ = client_id;

    cloud_policy_manager_ = std::make_unique<policy::MockCloudPolicyManager>(
        std::move(store), task_environment_.GetMainThreadTaskRunner());
    cloud_policy_manager_->core()->ConnectForTesting(
        std::move(service), std::move(cloud_policy_client));

    GetDemoLoginController()->SetDeviceCloudPolicyManagerForTesting(
        cloud_policy_manager_.get());
  }

  void TearDown() override {
    existing_user_controller_.reset();
    if (chromeos::PowerPolicyController::IsInitialized()) {
      chromeos::PowerPolicyController::Shutdown();
    }
    chromeos::PowerManagerClient::Shutdown();
  }

  GURL GetSetupUrl() {
    return net::AppendQueryParameter(GURL(kSetupDemoAccountUrl), kApiKeyParam,
                                     google_apis::GetAPIKey());
  }

  GURL GetCleanUpUrl() {
    return net::AppendQueryParameter(GURL(kCleanUpDemoAccountUrl), kApiKeyParam,
                                     google_apis::GetAPIKey());
  }

  // Mock a setup response return provided `gaia_id`. Verify that setup request
  // gets triggered and the login is successful.
  void MockSuccessSetupResponseAndVerifyLogin(const GaiaId& gaia_id) {
    // Mock a setup request will be success.
    test_url_loader_factory_.AddResponse(
        GetSetupUrl().spec(),
        base::StringPrintf(kValidGaiaCreds, gaia_id.ToString()));
    // Expect login if after clean up success.
    base::RunLoop loop;
    EXPECT_CALL(login_display_host(), CompleteLogin)
        .Times(1)
        .WillOnce(testing::Invoke([&](const UserContext& user_context) {
          const auto device_id = user_context.GetDeviceId();
          EXPECT_FALSE(device_id.empty());
          EXPECT_EQ(g_browser_process->local_state()->GetString(
                        prefs::kDemoModeSessionIdentifier),
                    device_id);
          EXPECT_EQ(GaiaId(g_browser_process->local_state()->GetString(
                        prefs::kDemoAccountGaiaId)),
                    gaia_id);

          loop.Quit();
        }));
    loop.Run();

    EXPECT_TRUE(CrosSettings::Get()->IsUserAllowlisted(
        kTestEmail, nullptr, std::optional<user_manager::UserType>()));
  }

  void ExpectGetExistingController() {
    EXPECT_CALL(login_display_host(), GetExistingUserController())
        .WillRepeatedly(testing::Return(existing_user_controller_.get()));
  }

  ScopedCrosSettingsTestHelper* settings_helper() { return &settings_helper_; }
  ExistingUserController* existing_user_controller() {
    return existing_user_controller_.get();
  }

  void AppendTestUserToUserList() {
    EXPECT_EQ(1U, fake_user_manager_->GetPersistedUsers().size());
    fake_user_manager_->AddUser(AccountId::FromNonCanonicalEmail(
        kTestEmail, kTestGaiaId, AccountType::GOOGLE));
    // Expect 2 users: test user with `kTestGaiaId` and public account user.
    EXPECT_EQ(2U, fake_user_manager_->GetPersistedUsers().size());
  }

  void ExpectOnlyDeviceLocalAccountInUserList() {
    const auto user_list = fake_user_manager_->GetPersistedUsers();
    EXPECT_EQ(1U, user_list.size());
    EXPECT_TRUE(user_list[0]->IsDeviceLocalAccount());
  }

  base::HistogramTester histogram_tester_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  base::test::ScopedFeatureList features_;
  content::BrowserTaskEnvironment task_environment_;

  // We don't own the destruction of `PowerPolicyController` which causes it
  // dangling.
  raw_ptr<chromeos::PowerPolicyController, DisableDanglingPtrDetection>
      policy_controller_;

  testing::NiceMock<ash::MockLoginDisplayHost> mock_login_display_host_;
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  system::FakeStatisticsProvider statistics_provider_;

  // Required for `user_manager::UserList`:
  std::unique_ptr<ash::AuthEventsRecorder> auth_events_recorder_;

  // Dependencies for `ExistingUserController`:
  FakeSessionManagerClient fake_session_manager_client_;
  ScopedCrosSettingsTestHelper settings_helper_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<FakeChromeUserManager>()};
  session_manager::SessionManager session_manager_;
  const AccountId auto_login_account_id_ =
      AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
          kPublicAccountUserId,
          policy::DeviceLocalAccountType::kPublicSession));
  std::unique_ptr<ExistingUserController> existing_user_controller_;

  std::unique_ptr<policy::MockCloudPolicyManager> cloud_policy_manager_;
};

TEST_F(DemoLoginControllerTest, OnSetupDemoAccountSuccessFirstTime) {
  SetUpPolicyClient();
  const GaiaId gaia_id(kTestGaiaId);
  test_url_loader_factory_.AddResponse(
      GetSetupUrl().spec(),
      base::StringPrintf(kValidGaiaCreds, gaia_id.ToString()));
  EXPECT_TRUE(g_browser_process->local_state()
                  ->GetString(prefs::kDemoModeSessionIdentifier)
                  .empty());
  base::RunLoop loop;
  EXPECT_CALL(login_display_host(), CompleteLogin)
      .Times(1)
      .WillOnce(testing::Invoke([&](const UserContext& user_context) {
        const auto device_id = user_context.GetDeviceId();
        EXPECT_FALSE(device_id.empty());
        EXPECT_EQ(g_browser_process->local_state()->GetString(
                      prefs::kDemoModeSessionIdentifier),
                  device_id);
        EXPECT_EQ(GaiaId(g_browser_process->local_state()->GetString(
                      prefs::kDemoAccountGaiaId)),
                  gaia_id);
        EXPECT_EQ(
            DemoSessionMetricsRecorder::GetCurrentSessionTypeForTesting(),
            DemoSessionMetricsRecorder::SessionType::kSignedInDemoSession);
        loop.Quit();
      }));

  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();
  // For first time setup demo account, no clean up get triggered.
  ASSERT_FALSE(test_url_loader_factory_.IsPending(GetCleanUpUrl().spec()));
  loop.Run();
}

TEST_F(DemoLoginControllerTest, InValidGaia) {
  SetUpPolicyClient();
  test_url_loader_factory_.AddResponse(GetSetupUrl().spec(), kInValidGaiaCreds);

  EXPECT_CALL(login_display_host(), CompleteLogin).Times(0);
  base::RunLoop loop;
  GetDemoLoginController()->SetSetupRequestCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        // Expect the setup request to fail by checking metrics.
        histogram_tester_.ExpectTotalCount(
            kSetupDemoAccountRequestResultHistogram, 1);
        histogram_tester_.ExpectBucketCount(
            kSetupDemoAccountRequestResultHistogram,
            DemoSessionMetricsRecorder::DemoAccountRequestResultCode::
                kInvalidCreds,
            1);
        EXPECT_EQ(DemoSessionMetricsRecorder::GetCurrentSessionTypeForTesting(),
                  DemoSessionMetricsRecorder::SessionType::kFallbackMGS);
        loop.Quit();
      }));
  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();
  loop.Run();
}

TEST_F(DemoLoginControllerTest,
       SetupDemoAccountCannotObtainDMTokenAndClientID) {
  SetUpPolicyClient();
  // In unit tests, there is no real cloud policy manager and
  // `policy_connector_ash->GetDeviceCloudPolicyManager()` is null. We remove
  // the fake one here so `DemoLoginController::GetDeviceIntegrity()` cannot
  // find any policy managers, and it will return failure (an empty
  // base::Value::Dict), causing the request to fail.
  GetDemoLoginController()->SetDeviceCloudPolicyManagerForTesting(nullptr);

  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();

  // Expect the setup request to fail by checking metrics.
  histogram_tester_.ExpectTotalCount(kSetupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kSetupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::
          kCloudPolicyNotConnected,
      1);
}

TEST_F(DemoLoginControllerTest, SetupDemoAccountEmptyDMToken) {
  // Set up the policy client again with an empty DM Token.
  SetUpPolicyClient("", "fake-client-id");

  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();

  // Expect the setup request to fail by checking metrics.
  histogram_tester_.ExpectTotalCount(kSetupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kSetupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::kEmptyDMToken,
      1);
}

TEST_F(DemoLoginControllerTest, SetupDemoAccountEmptyClientID) {
  // Set up the policy client again with an empty Client ID.
  SetUpPolicyClient("fake-dm-token", "");

  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();

  // Expect the setup request to fail by checking metrics.
  histogram_tester_.ExpectTotalCount(kSetupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kSetupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::kEmptyClientID,
      1);
  EXPECT_EQ(DemoSessionMetricsRecorder::GetCurrentSessionTypeForTesting(),
            DemoSessionMetricsRecorder::SessionType::kFallbackMGS);
}

TEST_F(DemoLoginControllerTest, ServerCleanupSuccess) {
  SetUpPolicyClient();
  AppendTestUserToUserList();
  auto* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDemoAccountGaiaId, kTestGaiaId.ToString());
  local_state->SetString(prefs::kDemoModeSessionIdentifier, kSessionId);

  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();

  // Verify the request was sent.
  ASSERT_TRUE(test_url_loader_factory_.IsPending(GetCleanUpUrl().spec()));
  test_url_loader_factory_.AddResponse(GetCleanUpUrl().spec(), "{}");

  base::RunLoop loop;
  GetDemoLoginController()->SetCleanupRequestCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        // Expect the prefs to be cleared upon the cleanup success.
        EXPECT_TRUE(local_state->GetString(prefs::kDemoAccountGaiaId).empty());
        EXPECT_TRUE(
            local_state->GetString(prefs::kDemoModeSessionIdentifier).empty());
        loop.Quit();
      }));
  loop.Run();

  MockSuccessSetupResponseAndVerifyLogin(GaiaId("234"));
  const auto new_session_id =
      local_state->GetString(prefs::kDemoModeSessionIdentifier);
  EXPECT_NE(new_session_id, kSessionId);

  // Expect the cleanup request to succeed by checking metrics.
  histogram_tester_.ExpectTotalCount(kCleanupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kCleanupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::kSuccess, 1);

  ExpectOnlyDeviceLocalAccountInUserList();
}

TEST_F(DemoLoginControllerTest, ServerCleanupFailed) {
  SetUpPolicyClient();
  AppendTestUserToUserList();
  auto* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDemoAccountGaiaId, kTestGaiaId.ToString());
  local_state->SetString(prefs::kDemoModeSessionIdentifier, kSessionId);
  test_url_loader_factory_.AddResponse(GetCleanUpUrl().spec(), "{}",
                                       net::HTTP_UNAUTHORIZED);
  base::RunLoop loop;
  GetDemoLoginController()->SetCleanupRequestCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        // Expect the cleanup request to fail by checking metrics.
        histogram_tester_.ExpectTotalCount(
            kCleanupDemoAccountRequestResultHistogram, 1);
        histogram_tester_.ExpectBucketCount(
            kCleanupDemoAccountRequestResultHistogram,
            DemoSessionMetricsRecorder::DemoAccountRequestResultCode::
                kRequestFailed,
            1);

        // Expect the prefs to persist upon the cleanup failure.
        EXPECT_EQ(local_state->GetString(prefs::kDemoAccountGaiaId),
                  kTestGaiaId.ToString());
        EXPECT_EQ(local_state->GetString(prefs::kDemoModeSessionIdentifier),
                  kSessionId);
        loop.Quit();
      }));
  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();
  loop.Run();

  // Verify login:
  MockSuccessSetupResponseAndVerifyLogin(GaiaId("234"));

  const auto new_session_id =
      local_state->GetString(prefs::kDemoModeSessionIdentifier);
  EXPECT_NE(new_session_id, kSessionId);

  // Expect the test account to be removed from local even if the server cleanup
  // failed.
  ExpectOnlyDeviceLocalAccountInUserList();
}

TEST_F(DemoLoginControllerTest, CleanupSuccessSetupFailure) {
  SetUpPolicyClient();
  // Add the test account locally. We expect it to be removed at the end of the
  // test.
  AppendTestUserToUserList();

  // Set the prefs. We expect them to be cleared upon the cleanup success at the
  // end of the test.
  auto* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDemoAccountGaiaId, kTestGaiaId.ToString());
  local_state->SetString(prefs::kDemoModeSessionIdentifier, kSessionId);

  // Verify that the demo account login gets triggered by
  // `ExistingUserController`.
  ConfigureAutoLoginSetting();

  // Verify the cleanup request was sent.
  ASSERT_TRUE(test_url_loader_factory_.IsPending(GetCleanUpUrl().spec()));
  test_url_loader_factory_.AddResponse(GetCleanUpUrl().spec(), "{}");

  // Customize the setup response so it will fail due to kInValidGaiaCreds.
  test_url_loader_factory_.AddResponse(GetSetupUrl().spec(), kInValidGaiaCreds);
  EXPECT_CALL(login_display_host(), CompleteLogin).Times(0);

  base::RunLoop loop;
  GetDemoLoginController()->SetSetupRequestCallbackForTesting(
      base::BindLambdaForTesting([&]() { loop.Quit(); }));
  loop.Run();

  // Expect the previous cleanup request to succeed by checking metrics.
  histogram_tester_.ExpectTotalCount(kCleanupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kCleanupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::kSuccess, 1);

  // Expect the prefs to be cleared after the cleanup success followed by a
  // setup request failure.
  EXPECT_TRUE(local_state->GetString(prefs::kDemoAccountGaiaId).empty());
  EXPECT_TRUE(
      local_state->GetString(prefs::kDemoModeSessionIdentifier).empty());

  // Expect the setup request failure by checking metrics.
  histogram_tester_.ExpectTotalCount(kSetupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kSetupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::kInvalidCreds,
      1);
  EXPECT_EQ(DemoSessionMetricsRecorder::GetCurrentSessionTypeForTesting(),
            DemoSessionMetricsRecorder::SessionType::kFallbackMGS);

  // Expect the test account to be removed locally.
  ExpectOnlyDeviceLocalAccountInUserList();
}

TEST_F(DemoLoginControllerTest,
       CleanupDemoAccountCannotObtainDMTokenAndClientID) {
  SetUpPolicyClient();
  // In unit tests, there is no real cloud policy manager and
  // `policy_connector_ash->GetDeviceCloudPolicyManager()` is null. We remove
  // the fake one here so `DemoLoginController::GetDeviceIdentifier()` cannot
  // find any policy managers, and it will return failure and cause the request
  // to fail.
  GetDemoLoginController()->SetDeviceCloudPolicyManagerForTesting(nullptr);

  AppendTestUserToUserList();
  auto* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDemoAccountGaiaId, kTestGaiaId.ToString());
  local_state->SetString(prefs::kDemoModeSessionIdentifier, kSessionId);

  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();

  // Expect the test account to be removed even if the cleanup failed.
  ExpectOnlyDeviceLocalAccountInUserList();

  // Expect the cleanup request to fail by checking metrics.
  histogram_tester_.ExpectTotalCount(kCleanupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kCleanupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::
          kCloudPolicyNotConnected,
      1);

  // Right after the account cleanup failed, it'll try to set up the demo
  // account regardless of the cleanup result. However, it's still
  // unable to obtain the DM Token and the Client ID, so it will fail again and
  // fall back to MGS. Therefore, we expect the auto login managed guest session
  // to start.
  EXPECT_TRUE(existing_user_controller()->IsAutoLoginTimerRunningForTesting());
  // Also expect the setup request to fail for the same reason by checking
  // metrics.
  histogram_tester_.ExpectTotalCount(kSetupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kSetupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::
          kCloudPolicyNotConnected,
      1);
}

TEST_F(DemoLoginControllerTest, CleanupDemoAccountEmptyDMToken) {
  // Set up the policy client again with an empty DM Token.
  SetUpPolicyClient("", "fake-client-id");

  AppendTestUserToUserList();
  auto* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDemoAccountGaiaId, kTestGaiaId.ToString());
  local_state->SetString(prefs::kDemoModeSessionIdentifier, kSessionId);

  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();

  // Expect the test account to be removed even if the cleanup failed.
  ExpectOnlyDeviceLocalAccountInUserList();

  // Expect the cleanup request to fail by checking metrics.
  histogram_tester_.ExpectTotalCount(kCleanupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kCleanupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::kEmptyDMToken,
      1);

  // Right after the account cleanup failed, it'll try to set up the demo
  // account regardless of the cleanup result. However, it'll get an empty DM
  // Token again, so it will fail again and fall back to MGS. Therefore, we
  // expect the auto login managed guest session to start.
  EXPECT_TRUE(existing_user_controller()->IsAutoLoginTimerRunningForTesting());
  // Also expect the setup request to fail for the same reason by checking
  // metrics.
  histogram_tester_.ExpectTotalCount(kSetupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kSetupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::kEmptyDMToken,
      1);
}

TEST_F(DemoLoginControllerTest, CleanupDemoAccountEmptyClientID) {
  // Set up the policy client again with an empty Client ID..
  SetUpPolicyClient("fake-dm-token", "");

  AppendTestUserToUserList();
  auto* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDemoAccountGaiaId, kTestGaiaId.ToString());
  local_state->SetString(prefs::kDemoModeSessionIdentifier, kSessionId);

  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();

  // Expect the test account to be removed even if the cleanup failed.
  ExpectOnlyDeviceLocalAccountInUserList();

  // Expect the cleanup request to fail by checking metrics.
  histogram_tester_.ExpectTotalCount(kCleanupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kCleanupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::kEmptyClientID,
      1);

  // Right after the account cleanup failed, it'll try to set up the demo
  // account regardless of the cleanup result. However, it'll get an empty
  // Client ID again, so it will fail again and fall back to MGS. Therefore, we
  // expect the auto login managed guest session to start.
  EXPECT_TRUE(existing_user_controller()->IsAutoLoginTimerRunningForTesting());
  // Also expect the setup request to fail for the same reason by checking
  // metrics.
  histogram_tester_.ExpectTotalCount(kSetupDemoAccountRequestResultHistogram,
                                     1);
  histogram_tester_.ExpectBucketCount(
      kSetupDemoAccountRequestResultHistogram,
      DemoSessionMetricsRecorder::DemoAccountRequestResultCode::kEmptyClientID,
      1);
  EXPECT_EQ(DemoSessionMetricsRecorder::GetCurrentSessionTypeForTesting(),
            DemoSessionMetricsRecorder::SessionType::kFallbackMGS);
}

TEST_F(DemoLoginControllerTest, FallbackToMGS) {
  SetUpPolicyClient();
  // Mock setup failed by returning invalid credential.
  test_url_loader_factory_.AddResponse(GetSetupUrl().spec(), kInValidGaiaCreds);

  EXPECT_CALL(login_display_host(), CompleteLogin).Times(0);

  base::RunLoop loop;
  GetDemoLoginController()->SetSetupRequestCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        // Expect the setup request to fail by checking metrics.
        histogram_tester_.ExpectTotalCount(
            kSetupDemoAccountRequestResultHistogram, 1);
        histogram_tester_.ExpectBucketCount(
            kSetupDemoAccountRequestResultHistogram,
            DemoSessionMetricsRecorder::DemoAccountRequestResultCode::
                kInvalidCreds,
            1);
        EXPECT_EQ(DemoSessionMetricsRecorder::GetCurrentSessionTypeForTesting(),
                  DemoSessionMetricsRecorder::SessionType::kFallbackMGS);
        loop.Quit();
      }));
  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();
  loop.Run();

  // Expect auto login managed guest session to start.
  EXPECT_TRUE(existing_user_controller()->IsAutoLoginTimerRunningForTesting());
}

TEST_F(DemoLoginControllerTest, LogServerError) {
  SetUpPolicyClient();
  // Mock setup failed by returning server error.
  test_url_loader_factory_.AddResponse(GetSetupUrl().spec(), kServerError,
                                       net::HTTP_INTERNAL_SERVER_ERROR);
  base::test::MockLog log;
  EXPECT_CALL(
      log,
      Log(::logging::LOGGING_ERROR, testing::_, testing::_, testing::_,
          testing::HasSubstr("Setup response error: error code: 500; message: "
                             "Internal error encountered.; status: INTERNAL.")))
      .Times(1);
  EXPECT_CALL(login_display_host(), CompleteLogin).Times(0);

  base::RunLoop loop;
  GetDemoLoginController()->SetSetupRequestCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        // Expect the setup request to fail by checking metrics.
        histogram_tester_.ExpectTotalCount(
            kSetupDemoAccountRequestResultHistogram, 1);
        histogram_tester_.ExpectBucketCount(
            kSetupDemoAccountRequestResultHistogram,
            DemoSessionMetricsRecorder::DemoAccountRequestResultCode::
                kRequestFailed,
            1);
        loop.Quit();
        log.StartCapturingLogs();
      }));
  // Trigger auto sign in:
  ConfigureAutoLoginSetting();
  loop.Run();
}

TEST_F(DemoLoginControllerTest, SetupDemoAccountErrorRetriable) {
  SetUpPolicyClient();
  test_url_loader_factory_.AddResponse(
      GetSetupUrl().spec(), kSetupDemoAccountFailedRetriableResponse);

  EXPECT_CALL(login_display_host(), CompleteLogin).Times(0);
  base::RunLoop loop;
  GetDemoLoginController()->SetSetupRequestCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        // Expect the setup request to fail by checking metrics.
        histogram_tester_.ExpectTotalCount(
            kSetupDemoAccountRequestResultHistogram, 1);
        histogram_tester_.ExpectBucketCount(
            kSetupDemoAccountRequestResultHistogram,
            DemoSessionMetricsRecorder::DemoAccountRequestResultCode::
                kQuotaExhaustedRetriable,
            1);
        EXPECT_EQ(DemoSessionMetricsRecorder::GetCurrentSessionTypeForTesting(),
                  DemoSessionMetricsRecorder::SessionType::kFallbackMGS);
        loop.Quit();
      }));
  // Verify demo account login gets triggered by `ExistingUserController`.
  ConfigureAutoLoginSetting();
  loop.Run();
  EXPECT_TRUE(demo_mode::GetShouldScheduleLogoutForMGS());
}

// TODO(crbug.com/372771485): Add more request fail test cases.

}  // namespace ash
