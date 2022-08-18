// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_start_crd_session_job.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "remoting/host/chromeos/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

using ::base::test::TestFuture;
using ResultCode = DeviceCommandStartCrdSessionJob::ResultCode;
using UmaSessionType = DeviceCommandStartCrdSessionJob::UmaSessionType;
using remoting::features::kEnableCrdAdminRemoteAccess;
namespace em = ::enterprise_management;

constexpr char kResultCodeFieldName[] = "resultCode";
constexpr char kResultMessageFieldName[] = "message";
constexpr char kResultAccessCodeFieldName[] = "accessCode";
constexpr char kResultLastActivityFieldName[] = "lastActivitySec";

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

constexpr char kTestOAuthToken[] = "test-oauth-token";
constexpr char kTestAccessCode[] = "111122223333";
constexpr char kTestNoOAuthTokenReason[] = "Not authorized.";
constexpr char kTestAccountEmail[] = "test.account.email@example.com";

// Macro expecting success. We are using a macro because a function would
// report any error against the line in the function, and not against the
// place where EXPECT_SUCCESS is called.
#define EXPECT_SUCCESS(statement_)                                     \
  ({                                                                   \
    auto result_ = statement_;                                         \
    EXPECT_EQ(result_.status, RemoteCommandJob::Status::SUCCEEDED);    \
    EXPECT_EQ(result_.payload, CreateSuccessPayload(kTestAccessCode)); \
  })

// Macro expecting error. We are using a macro because a function would
// report any error against the line in the function, and not against the
// place where EXPECT_ERROR is called.
#define EXPECT_ERROR(statement_, error_code, ...)                              \
  ({                                                                           \
    auto result_ = statement_;                                                 \
    EXPECT_EQ(result_.status, RemoteCommandJob::Status::FAILED);               \
    EXPECT_EQ(result_.payload, CreateErrorPayload(error_code, ##__VA_ARGS__)); \
  })

em::RemoteCommand GenerateCommandProto(RemoteCommandJob::UniqueIDType unique_id,
                                       base::TimeDelta age_of_command,
                                       std::string payload) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_DEVICE_START_CRD_SESSION);
  command_proto.set_command_id(unique_id);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  command_proto.set_payload(payload);
  return command_proto;
}

class StubCrdHostDelegate : public DeviceCommandStartCrdSessionJob::Delegate {
 public:
  StubCrdHostDelegate() = default;
  ~StubCrdHostDelegate() override = default;

  void SetHasActiveSession(bool value) { has_active_session_ = value; }
  void MakeAccessCodeFetchFail() { access_code_success_ = false; }

  // Returns if TerminateSession() was called to terminate the active session.
  bool IsActiveSessionTerminated() const { return terminate_session_called_; }

  // Returns the |SessionParameters| sent to the last StartCrdHostAndGetCode()
  // call.
  SessionParameters session_parameters() {
    EXPECT_TRUE(received_session_parameters_.has_value());
    return received_session_parameters_.value_or(SessionParameters{});
  }

  // DeviceCommandStartCrdSessionJob::Delegate implementation:
  bool HasActiveSession() const override;
  void TerminateSession(base::OnceClosure callback) override;
  void StartCrdHostAndGetCode(
      const SessionParameters& parameters,
      DeviceCommandStartCrdSessionJob::AccessCodeCallback success_callback,
      DeviceCommandStartCrdSessionJob::ErrorCallback error_callback) override;

 private:
  bool has_active_session_ = false;
  bool access_code_success_ = true;
  bool terminate_session_called_ = false;
  absl::optional<SessionParameters> received_session_parameters_;
};

bool StubCrdHostDelegate::HasActiveSession() const {
  return has_active_session_;
}

void StubCrdHostDelegate::TerminateSession(base::OnceClosure callback) {
  has_active_session_ = false;
  terminate_session_called_ = true;
  std::move(callback).Run();
}

void StubCrdHostDelegate::StartCrdHostAndGetCode(
    const SessionParameters& parameters,
    DeviceCommandStartCrdSessionJob::AccessCodeCallback success_callback,
    DeviceCommandStartCrdSessionJob::ErrorCallback error_callback) {
  received_session_parameters_ = parameters;

  if (access_code_success_) {
    std::move(success_callback).Run(kTestAccessCode);
  } else {
    std::move(error_callback)
        .Run(DeviceCommandStartCrdSessionJob::FAILURE_CRD_HOST_ERROR,
             std::string());
  }
}

struct Result {
  RemoteCommandJob::Status status;
  std::string payload;
};

// Convenience class that makes it easier to build a |base::Value::Dict|.
class DictBuilder {
 public:
  template <typename T>
  DictBuilder& Set(base::StringPiece key, T value) {
    dict_.Set(key, std::move(value));
    return *this;
  }

  std::string ToString() const {
    std::string result;
    base::JSONWriter::Write(dict_, &result);
    return result;
  }

 private:
  base::Value::Dict dict_;
};

}  // namespace

class DeviceCommandStartCrdSessionJobTest : public ash::DeviceSettingsTestBase {
 public:
  DeviceCommandStartCrdSessionJobTest()
      : ash::DeviceSettingsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // ash::DeviceSettingsTestBase implementation:
  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    user_activity_detector_ = std::make_unique<ui::UserActivityDetector>();
    arc_kiosk_app_manager_ = std::make_unique<ash::ArcKioskAppManager>();
    web_kiosk_app_manager_ = std::make_unique<ash::WebKioskAppManager>();

    // SystemSaltGetter is used by the token service.
    chromeos::SystemSaltGetter::Initialize();
    DeviceOAuth2TokenServiceFactory::Initialize(
        test_url_loader_factory_.GetSafeWeakWrapper(), &local_state_);
    RegisterLocalState(local_state_.registry());
  }

  void TearDown() override {
    DeviceOAuth2TokenServiceFactory::Shutdown();
    chromeos::SystemSaltGetter::Shutdown();

    web_kiosk_app_manager_.reset();
    arc_kiosk_app_manager_.reset();

    DeviceSettingsTestBase::TearDown();
  }

  Result RunJobAndWaitForResult(const DictBuilder& payload = DictBuilder()) {
    bool launched = InitializeAndRunJob(payload);
    EXPECT_TRUE(launched) << "Failed to launch the job";
    // Do not wait for the result if the job was never launched in the first
    // place.
    if (!launched)
      return Result{RemoteCommandJob::Status::NOT_INITIALIZED};

    return future_result_.Get();
  }

  // Create an empty payload builder.
  DictBuilder Payload() const { return DictBuilder(); }

  std::string CreateSuccessPayload(const std::string& access_code);
  std::string CreateErrorPayload(ResultCode result_code,
                                 const std::string& error_message);
  std::string CreateNotIdlePayload(int idle_time_in_sec);

  void LogInAsManagedGuestSessionUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddPublicAccountUser(account_id);
    user_manager().LoginUser(account_id);
  }

  void LogInAsRegularUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddUser(account_id);
    user_manager().LoginUser(account_id);
  }

  void LogInAsAffiliatedUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddUserWithAffiliation(account_id, /*is_affiliated=*/true);
    user_manager().LoginUser(account_id);
  }

  void LogInAsGuestUser() {
    const user_manager::User* user = user_manager().AddGuestUser();
    user_manager().LoginUser(user->GetAccountId());
  }

  void LogInAsKioskAppUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddKioskAppUser(account_id);
    user_manager().LoginUser(account_id);
  }

  void LogInAsArcKioskAppUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddArcKioskAppUser(account_id);
    user_manager().LoginUser(account_id);
  }

  void LogInAsWebKioskAppUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddWebKioskAppUser(account_id);
    user_manager().LoginUser(account_id);
  }

  void LogInAsAutoLaunchedKioskAppUser() {
    LogInAsKioskAppUser();
    ash::KioskAppManager::Get()
        ->set_current_app_was_auto_launched_with_zero_delay_for_testing(true);
  }

  void LogInAsManuallyLaunchedKioskAppUser() {
    LogInAsKioskAppUser();
    ash::KioskAppManager::Get()
        ->set_current_app_was_auto_launched_with_zero_delay_for_testing(false);
  }

  void SetDeviceIdleTime(int idle_time_in_sec) {
    user_activity_detector_->set_last_activity_time_for_test(
        base::TimeTicks::Now() - base::Seconds(idle_time_in_sec));
  }

  void SetOAuthToken(std::string value) { oauth_token_ = value; }

  void SetRobotAccountUserName(const std::string& user_name) {
    DeviceOAuth2TokenService* token_service =
        DeviceOAuth2TokenServiceFactory::Get();
    token_service->set_robot_account_id_for_testing(CoreAccountId(user_name));
  }

  void ClearOAuthToken() { oauth_token_ = absl::nullopt; }

  void DeleteUserActivityDetector() { user_activity_detector_ = nullptr; }
  void DeleteUserManager() { user_manager_enabler_ = nullptr; }

  StubCrdHostDelegate& crd_host_delegate() { return crd_host_delegate_; }
  DeviceCommandStartCrdSessionJob& job() { return job_; }

  bool InitializeJob(const DictBuilder& payload) {
    bool success = job().Init(
        base::TimeTicks::Now(),
        GenerateCommandProto(kUniqueID, base::TimeDelta(), payload.ToString()),
        em::SignedData());

    if (oauth_token_)
      job().SetOAuthTokenForTest(oauth_token_.value());

    if (success) {
      EXPECT_EQ(kUniqueID, job().unique_id());
      EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job().status());
    }
    return success;
  }

  // Initialize and run the remote command job.
  // The result will be stored in |future_result_|.
  bool InitializeAndRunJob(const DictBuilder& payload) {
    bool success = InitializeJob(payload);
    EXPECT_TRUE(success) << "Failed to initialize the job";
    if (!success)
      return false;

    bool launched = job().Run(
        base::Time::Now(), base::TimeTicks::Now(),
        base::BindOnce(&DeviceCommandStartCrdSessionJobTest::OnJobFinished,
                       base::Unretained(this)));
    return launched;
  }

 private:
  ash::FakeChromeUserManager& user_manager() { return *user_manager_; }

  // Callback invoked when the remote command job finished.
  void OnJobFinished() {
    std::string payload =
        job().GetResultPayload() ? *job().GetResultPayload() : "<nullptr>";

    future_result_.SetValue(Result{job().status(), payload});
  }

  std::unique_ptr<ash::ArcKioskAppManager> arc_kiosk_app_manager_;
  std::unique_ptr<ash::WebKioskAppManager> web_kiosk_app_manager_;

  absl::optional<std::string> oauth_token_ = kTestOAuthToken;

  // Automatically installed as a singleton upon creation.
  std::unique_ptr<ui::UserActivityDetector> user_activity_detector_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple local_state_;

  StubCrdHostDelegate crd_host_delegate_;
  DeviceCommandStartCrdSessionJob job_{&crd_host_delegate_};

  // Future value that will be populated with the result once the remote command
  // job is completed.
  TestFuture<Result> future_result_;
};

std::string DeviceCommandStartCrdSessionJobTest::CreateSuccessPayload(
    const std::string& access_code) {
  return DictBuilder()
      .Set(kResultCodeFieldName, DeviceCommandStartCrdSessionJob::SUCCESS)
      .Set(kResultAccessCodeFieldName, access_code)
      .ToString();
}

std::string DeviceCommandStartCrdSessionJobTest::CreateErrorPayload(
    ResultCode result_code,
    const std::string& error_message = "") {
  DictBuilder builder;
  builder.Set(kResultCodeFieldName, result_code);
  if (!error_message.empty())
    builder.Set(kResultMessageFieldName, error_message);
  return builder.ToString();
}

std::string DeviceCommandStartCrdSessionJobTest::CreateNotIdlePayload(
    int idle_time_in_sec) {
  return DictBuilder()
      .Set(kResultCodeFieldName,
           DeviceCommandStartCrdSessionJob::FAILURE_NOT_IDLE)
      .Set(kResultLastActivityFieldName, idle_time_in_sec)
      .ToString();
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedIfAccessTokenCanBeFetched) {
  LogInAsAutoLaunchedKioskAppUser();

  SetOAuthToken(kTestOAuthToken);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldTerminateActiveSessionAndThenSucceed) {
  LogInAsAutoLaunchedKioskAppUser();

  crd_host_delegate().SetHasActiveSession(true);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_TRUE(crd_host_delegate().IsActiveSessionTerminated());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfOAuthTokenServiceIsNotRunning) {
  DeviceOAuth2TokenServiceFactory::Shutdown();

  crd_host_delegate().SetHasActiveSession(true);

  EXPECT_ERROR(RunJobAndWaitForResult(),
               DeviceCommandStartCrdSessionJob::FAILURE_SERVICES_NOT_READY);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfUserActivityDetectorIsNotRunning) {
  DeleteUserActivityDetector();

  EXPECT_ERROR(RunJobAndWaitForResult(),
               DeviceCommandStartCrdSessionJob::FAILURE_SERVICES_NOT_READY);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfUserManagerIsNotRunning) {
  DeleteUserManager();

  EXPECT_ERROR(RunJobAndWaitForResult(),
               DeviceCommandStartCrdSessionJob::FAILURE_SERVICES_NOT_READY);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailForGuestUser) {
  LogInAsGuestUser();

  EXPECT_ERROR(RunJobAndWaitForResult(),
               DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailForRegularUser) {
  LogInAsRegularUser();

  EXPECT_ERROR(RunJobAndWaitForResult(),
               DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForManuallyLaunchedKioskUser) {
  LogInAsKioskAppUser();

  ash::KioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(false);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForAutoLaunchedKioskUser) {
  LogInAsKioskAppUser();
  ash::KioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(true);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForManuallyLaunchedArcKioskUser) {
  SetOAuthToken(kTestOAuthToken);

  LogInAsArcKioskAppUser();
  ash::ArcKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(false);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForAutoLaunchedArcKioskUser) {
  LogInAsArcKioskAppUser();
  ash::ArcKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(true);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForManuallyLaunchedWebKioskUser) {
  LogInAsWebKioskAppUser();
  ash::WebKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(false);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForAutoLaunchedWebKioskUser) {
  LogInAsWebKioskAppUser();
  ash::WebKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(true);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfDeviceIdleTimeIsLessThanIdlenessCutoffValue) {
  LogInAsAutoLaunchedKioskAppUser();

  const int device_idle_time_in_sec = 9;
  const int idleness_cutoff_in_sec = 10;

  SetDeviceIdleTime(device_idle_time_in_sec);

  Result result = RunJobAndWaitForResult(
      Payload().Set("idlenessCutoffSec", idleness_cutoff_in_sec));
  EXPECT_EQ(result.status, RemoteCommandJob::Status::FAILED);
  EXPECT_EQ(result.payload, CreateNotIdlePayload(device_idle_time_in_sec));
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedIfDeviceIdleTimeIsMoreThanIdlenessCutoffValue) {
  LogInAsAutoLaunchedKioskAppUser();

  const int device_idle_time_in_sec = 10;
  const int idleness_cutoff_in_sec = 9;

  SetDeviceIdleTime(device_idle_time_in_sec);

  EXPECT_SUCCESS(RunJobAndWaitForResult(
      Payload().Set("idlenessCutoffSec", idleness_cutoff_in_sec)));
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldCheckUserTypeBeforeDeviceIdleTime) {
  // If we were to check device idle time first, the remote admin would
  // still be asked to acknowledge the user's presence, even if they are not
  // allowed to start a CRD connection anyway.
  LogInAsRegularUser();

  const int device_idle_time_in_sec = 9;
  const int idleness_cutoff_in_sec = 10;

  SetDeviceIdleTime(device_idle_time_in_sec);

  EXPECT_ERROR(RunJobAndWaitForResult(
                   Payload().Set("idlenessCutoffSec", idleness_cutoff_in_sec)),
               DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfWeCantFetchTheOAuthToken) {
  LogInAsAutoLaunchedKioskAppUser();
  ClearOAuthToken();

  EXPECT_ERROR(RunJobAndWaitForResult(),
               DeviceCommandStartCrdSessionJob::FAILURE_NO_OAUTH_TOKEN,
               kTestNoOAuthTokenReason);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailIfCrdHostReportsAnError) {
  LogInAsAutoLaunchedKioskAppUser();

  crd_host_delegate().MakeAccessCodeFetchFail();

  EXPECT_ERROR(RunJobAndWaitForResult(),
               DeviceCommandStartCrdSessionJob::FAILURE_CRD_HOST_ERROR);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldPassOAuthTokenToDelegate) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken("the-oauth-token");

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_EQ("the-oauth-token",
            crd_host_delegate().session_parameters().oauth_token);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassRobotAccountNameToDelegate) {
  LogInAsAutoLaunchedKioskAppUser();

  SetRobotAccountUserName("robot-account");

  EXPECT_SUCCESS(RunJobAndWaitForResult());

  EXPECT_EQ("robot-account",
            crd_host_delegate().session_parameters().user_name);
}

TEST_F(
    DeviceCommandStartCrdSessionJobTest,
    ShouldPassTerminateUponInputTrueToDelegateForAutolaunchedKioskIfAckedUserPresenceSetFalse) {
  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("acked_user_presence", false)));

  EXPECT_EQ(true,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(
    DeviceCommandStartCrdSessionJobTest,
    ShouldPassTerminateUponInputFalseToDelegateForAutolaunchedKioskIfAckedUserPresenceSetTrue) {
  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("ackedUserPresence", true)));

  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(
    DeviceCommandStartCrdSessionJobTest,
    ShouldPassTerminateUponInputTrueToDelegateForManuallylaunchedKioskIfAckedUserPresenceSetFalse) {
  LogInAsManuallyLaunchedKioskAppUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("acked_user_presence", false)));

  EXPECT_EQ(true,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(
    DeviceCommandStartCrdSessionJobTest,
    ShouldPassTerminateUponInputFalseToDelegateForManuallyLaunchedKioskIfAckedUserPresenceSetTrue) {
  LogInAsManuallyLaunchedKioskAppUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("ackedUserPresence", true)));

  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassShowConfirmationDialogFalseToDelegateForKioskUser) {
  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());

  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailIfNoUserIsLoggedIn) {
  EXPECT_ERROR(RunJobAndWaitForResult(),
               DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldSucceedForManagedGuestUser) {
  LogInAsManagedGuestSessionUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldSucceedForAffiliatedUser) {
  LogInAsAffiliatedUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassShowConfirmationDialogTrueToDelegateForManagedGuestUser) {
  LogInAsManagedGuestSessionUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_EQ(true,
            crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassShowConfirmationDialogTrueToDelegateForAffiliatedUser) {
  LogInAsAffiliatedUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_EQ(true,
            crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldNeverSendTerminateUponInputTrueToDelegateForAffiliatedUser) {
  LogInAsAffiliatedUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("ackedUserPresense", false)));
  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldNeverSendTerminateUponInputTrueToDelegateForManagedGuestUser) {
  LogInAsManagedGuestSessionUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("ackedUserPresense", false)));
  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendSuccessUmaLogWhenAutoLaunchedKioskConnects) {
  base::HistogramTester histogram_tester;

  LogInAsAutoLaunchedKioskAppUser();
  crd_host_delegate().SetHasActiveSession(true);
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType",
      UmaSessionType::kAutoLaunchedKiosk, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendSuccessUmaLogWhenManuallyLaunchedKioskConnects) {
  base::HistogramTester histogram_tester;

  LogInAsManuallyLaunchedKioskAppUser();
  crd_host_delegate().SetHasActiveSession(true);
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType",
      UmaSessionType::kManuallyLaunchedKiosk, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendSuccessUmaLogWhenAffiliatedUserConnects) {
  base::HistogramTester histogram_tester;

  LogInAsAffiliatedUser();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType",
      UmaSessionType::kAffiliatedUser, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendSuccessUmaLogWhenManagedGuestSessionConnects) {
  base::HistogramTester histogram_tester;

  LogInAsManagedGuestSessionUser();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType",
      UmaSessionType::kManagedGuestSession, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogWhenDeviceNotReady) {
  base::HistogramTester histogram_tester;

  DeviceOAuth2TokenServiceFactory::Shutdown();

  crd_host_delegate().SetHasActiveSession(true);

  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_SERVICES_NOT_READY, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogWhenUserTypeIsNotSupported) {
  base::HistogramTester histogram_tester;

  LogInAsRegularUser();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_UNSUPPORTED_USER_TYPE, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogWhenDeviceIsNotIdle) {
  base::HistogramTester histogram_tester;
  LogInAsAutoLaunchedKioskAppUser();

  const int device_idle_time_in_sec = 9;
  const int idleness_cutoff_in_sec = 10;

  SetDeviceIdleTime(device_idle_time_in_sec);
  RunJobAndWaitForResult(
      Payload().Set("idlenessCutoffSec", idleness_cutoff_in_sec));

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::FAILURE_NOT_IDLE,
      1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogFailureNoAuthToken) {
  base::HistogramTester histogram_tester;
  LogInAsAffiliatedUser();

  ClearOAuthToken();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_NO_OAUTH_TOKEN, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogFailureCrdHostError) {
  base::HistogramTester histogram_tester;
  LogInAsAutoLaunchedKioskAppUser();

  crd_host_delegate().MakeAccessCodeFetchFail();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_CRD_HOST_ERROR, 1);
}

class DeviceCommandStartCrdSessionJobCurtainSessionTest
    : public DeviceCommandStartCrdSessionJobTest {
 public:
  void EnableFeature(const base::Feature& feature) {
    feature_.InitAndEnableFeature(feature);
  }

  void DisableFeature(const base::Feature& feature) {
    feature_.InitAndDisableFeature(feature);
  }

 private:
  base::test::ScopedFeatureList feature_;
};

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldUseCurtainLocalUserSessionFalseIfFeatureIsDisabled) {
  DisableFeature(kEnableCrdAdminRemoteAccess);

  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_FALSE(
      crd_host_delegate().session_parameters().curtain_local_user_session);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldDefaultCurtainLocalUserSessionToFalseIfUnspecifiedInPayload) {
  EnableFeature(kEnableCrdAdminRemoteAccess);
  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult(Payload()));
  EXPECT_FALSE(
      crd_host_delegate().session_parameters().curtain_local_user_session);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldRejectCurtainLocalUserSessionTrueInPayloadIfFeatureIsDisabled) {
  DisableFeature(kEnableCrdAdminRemoteAccess);

  bool success = InitializeJob(Payload().Set("curtainLocalUserSession", true));

  EXPECT_FALSE(success);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForGuestUser) {
  EnableFeature(kEnableCrdAdminRemoteAccess);

  LogInAsGuestUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForManagedGuestSessionUser) {
  EnableFeature(kEnableCrdAdminRemoteAccess);

  LogInAsManagedGuestSessionUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForRegularUser) {
  EnableFeature(kEnableCrdAdminRemoteAccess);

  LogInAsRegularUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForAffiliatedUser) {
  EnableFeature(kEnableCrdAdminRemoteAccess);

  LogInAsAffiliatedUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForKioskUserWithoutAutoLaunch) {
  EnableFeature(kEnableCrdAdminRemoteAccess);

  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForKioskUserWithAutoLaunch) {
  EnableFeature(kEnableCrdAdminRemoteAccess);

  LogInAsKioskAppUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldSucceedIfNoUserIsLoggedIn) {
  EnableFeature(kEnableCrdAdminRemoteAccess);

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)));
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldSetCurtainLocalUserSessionTrue) {
  EnableFeature(kEnableCrdAdminRemoteAccess);

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)));
  EXPECT_TRUE(
      crd_host_delegate().session_parameters().curtain_local_user_session);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldSetCurtainLocalUserSessionFalse) {
  EnableFeature(kEnableCrdAdminRemoteAccess);

  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", false)));
  EXPECT_FALSE(
      crd_host_delegate().session_parameters().curtain_local_user_session);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldNotTerminateUponInput) {
  EnableFeature(kEnableCrdAdminRemoteAccess);

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload()
                                 .Set("curtainLocalUserSession", true)
                                 // This would enable terminate upon input in
                                 // a non-curtained job.
                                 .Set("ackedUserPresense", false)));
  EXPECT_FALSE(crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldNotShowConfirmationDialog) {
  EnableFeature(kEnableCrdAdminRemoteAccess);

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)));
  EXPECT_FALSE(
      crd_host_delegate().session_parameters().show_confirmation_dialog);
}

}  // namespace policy
