// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_start_crd_session_job.h"

#include <map>
#include <utility>
#include <vector>

#include "ash/components/cryptohome/system_salt_getter.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

using ::base::test::TestFuture;
using ResultCode = DeviceCommandStartCrdSessionJob::ResultCode;
using UmaSessionType = DeviceCommandStartCrdSessionJob::UmaSessionType;
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

constexpr char kIdlenessCutoffFieldName[] = "idlenessCutoffSec";
constexpr char kAckedUserPresenceFieldName[] = "ackedUserPresence";

// Macro expecting success. We are using a macro because a function would
// report any error against the line in the function, and not against the
// place where EXPECT_SUCCESS is called.
#define EXPECT_SUCCESS(result_)                                       \
  ({                                                                  \
    EXPECT_EQ(result.status, RemoteCommandJob::Status::SUCCEEDED);    \
    EXPECT_EQ(result.payload, CreateSuccessPayload(kTestAccessCode)); \
  })

// Macro expecting error. We are using a macro because a function would
// report any error against the line in the function, and not against the
// place where EXPECT_ERROR is called.
#define EXPECT_ERROR(result_, error_code, ...)                                \
  ({                                                                          \
    EXPECT_EQ(result.status, RemoteCommandJob::Status::FAILED);               \
    EXPECT_EQ(result.payload, CreateErrorPayload(error_code, ##__VA_ARGS__)); \
  })

em::RemoteCommand GenerateCommandProto(RemoteCommandJob::UniqueIDType unique_id,
                                       base::TimeDelta age_of_command,
                                       base::TimeDelta idleness_cutoff,
                                       bool acked_user_presence) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_DEVICE_START_CRD_SESSION);
  command_proto.set_command_id(unique_id);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());

  std::string payload;
  base::Value root_dict(base::Value::Type::DICTIONARY);
  root_dict.SetKey(kIdlenessCutoffFieldName,
                   base::Value((int)idleness_cutoff.InSeconds()));
  root_dict.SetKey(kAckedUserPresenceFieldName,
                   base::Value(acked_user_presence));
  base::JSONWriter::Write(root_dict, &payload);
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

}  // namespace

class DeviceCommandStartCrdSessionJobTest : public ash::DeviceSettingsTestBase {
 public:
  DeviceCommandStartCrdSessionJobTest()
      : ash::DeviceSettingsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // ash::DeviceSettingsTestBase implementation:
  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    test_start_time_ = base::TimeTicks::Now();

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

  Result RunJobAndWaitForResult() {
    InitializeAndRunJob();
    return future_result_.Get();
  }

  std::string CreateSuccessPayload(const std::string& access_code);
  std::string CreateErrorPayload(ResultCode result_code,
                                 const std::string& error_message);
  std::string CreateNotIdlePayload(base::TimeDelta idleness);

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

  void SetDeviceIdleTime(base::TimeDelta idle_time) {
    user_activity_detector_->set_last_activity_time_for_test(
        base::TimeTicks::Now() - idle_time);
  }

  void SetIdlenessCutoff(base::TimeDelta value) { idleness_cutoff_ = value; }

  void SetOAuthToken(std::string value) { oauth_token_ = value; }

  void SetAckedUserPresence(bool value) { acked_user_presence_ = value; }

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

 private:
  ash::FakeChromeUserManager& user_manager() { return *user_manager_; }

  void InitializeJob() {
    bool success =
        job().Init(base::TimeTicks::Now(),
                   GenerateCommandProto(
                       kUniqueID, base::TimeTicks::Now() - test_start_time_,
                       idleness_cutoff_, acked_user_presence_),
                   em::SignedData());

    if (oauth_token_)
      job().SetOAuthTokenForTest(oauth_token_.value());

    EXPECT_TRUE(success);
    EXPECT_EQ(kUniqueID, job().unique_id());
    EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job().status());
  }

  // Initialize and run the remote command job.
  // The result will be stored in |future_result_|.
  void InitializeAndRunJob() {
    InitializeJob();
    bool launched = job().Run(
        base::Time::Now(), base::TimeTicks::Now(),
        base::BindOnce(&DeviceCommandStartCrdSessionJobTest::OnJobFinished,
                       base::Unretained(this)));
    EXPECT_TRUE(launched);
  }

  // Callback invoked when the remote command job finished.
  void OnJobFinished() {
    std::string payload =
        job().GetResultPayload() ? *job().GetResultPayload() : "<nullptr>";

    future_result_.SetValue(Result{job().status(), payload});
  }

  std::unique_ptr<ash::ArcKioskAppManager> arc_kiosk_app_manager_;
  std::unique_ptr<ash::WebKioskAppManager> web_kiosk_app_manager_;

  absl::optional<std::string> oauth_token_;
  base::TimeDelta idleness_cutoff_ = base::Seconds(30);
  bool acked_user_presence_ = false;

  // Automatically installed as a singleton upon creation.
  std::unique_ptr<ui::UserActivityDetector> user_activity_detector_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple local_state_;

  StubCrdHostDelegate crd_host_delegate_;
  DeviceCommandStartCrdSessionJob job_{&crd_host_delegate_};

  // Future value that will be populated with the result once the remote command
  // job is completed.
  TestFuture<Result> future_result_;

  base::TimeTicks test_start_time_;
};

std::string DeviceCommandStartCrdSessionJobTest::CreateSuccessPayload(
    const std::string& access_code) {
  std::string payload;
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey(kResultCodeFieldName,
              base::Value(DeviceCommandStartCrdSessionJob::SUCCESS));
  root.SetKey(kResultAccessCodeFieldName, base::Value(access_code));
  base::JSONWriter::Write(root, &payload);
  return payload;
}

std::string DeviceCommandStartCrdSessionJobTest::CreateErrorPayload(
    ResultCode result_code,
    const std::string& error_message = "") {
  std::string payload;
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey(kResultCodeFieldName, base::Value(result_code));
  if (!error_message.empty())
    root.SetKey(kResultMessageFieldName, base::Value(error_message));
  base::JSONWriter::Write(root, &payload);
  return payload;
}

std::string DeviceCommandStartCrdSessionJobTest::CreateNotIdlePayload(
    base::TimeDelta idleness) {
  std::string payload;
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey(kResultCodeFieldName,
              base::Value(DeviceCommandStartCrdSessionJob::FAILURE_NOT_IDLE));
  root.SetKey(kResultLastActivityFieldName,
              base::Value(static_cast<int>(idleness.InSeconds())));
  base::JSONWriter::Write(root, &payload);
  return payload;
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedIfAccessTokenCanBeFetched) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);

  Result result = RunJobAndWaitForResult();

  EXPECT_SUCCESS(result);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldTerminateActiveSessionAndThenSucceed) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);

  crd_host_delegate().SetHasActiveSession(true);

  Result result = RunJobAndWaitForResult();

  EXPECT_SUCCESS(result);
  EXPECT_TRUE(crd_host_delegate().IsActiveSessionTerminated());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfOAuthTokenServiceIsNotRunning) {
  DeviceOAuth2TokenServiceFactory::Shutdown();

  crd_host_delegate().SetHasActiveSession(true);

  Result result = RunJobAndWaitForResult();

  EXPECT_ERROR(result,
               DeviceCommandStartCrdSessionJob::FAILURE_SERVICES_NOT_READY);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfUserActivityDetectorIsNotRunning) {
  DeleteUserActivityDetector();

  Result result = RunJobAndWaitForResult();

  EXPECT_ERROR(result,
               DeviceCommandStartCrdSessionJob::FAILURE_SERVICES_NOT_READY);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfUserManagerIsNotRunning) {
  DeleteUserManager();

  Result result = RunJobAndWaitForResult();

  EXPECT_ERROR(result,
               DeviceCommandStartCrdSessionJob::FAILURE_SERVICES_NOT_READY);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailForGuestUser) {
  LogInAsGuestUser();

  Result result = RunJobAndWaitForResult();

  EXPECT_ERROR(result,
               DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailForRegularUser) {
  LogInAsRegularUser();

  Result result = RunJobAndWaitForResult();

  EXPECT_ERROR(result,
               DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailForKioskUserWithoutAutoLaunch) {
  LogInAsKioskAppUser();

  ash::KioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(false);

  Result result = RunJobAndWaitForResult();

  EXPECT_ERROR(result,
               DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForKioskUserWithZeroDelayAutoLaunch) {
  SetOAuthToken(kTestOAuthToken);

  LogInAsKioskAppUser();
  ash::KioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(true);

  Result result = RunJobAndWaitForResult();

  EXPECT_SUCCESS(result);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailForArcKioskUserWithoutAutoLaunch) {
  SetOAuthToken(kTestOAuthToken);

  LogInAsArcKioskAppUser();
  ash::ArcKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(false);

  Result result = RunJobAndWaitForResult();

  EXPECT_ERROR(result,
               DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForArcKioskUserWithZeroDelayAutoLaunch) {
  SetOAuthToken(kTestOAuthToken);

  LogInAsArcKioskAppUser();
  ash::ArcKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(true);

  Result result = RunJobAndWaitForResult();

  EXPECT_SUCCESS(result);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailForWebKioskUserWithoutAutoLaunch) {
  SetOAuthToken(kTestOAuthToken);

  LogInAsWebKioskAppUser();
  ash::WebKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(false);

  Result result = RunJobAndWaitForResult();

  EXPECT_ERROR(result,
               DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForWebKioskUserWithZeroDelayAutoLaunch) {
  SetOAuthToken(kTestOAuthToken);

  LogInAsWebKioskAppUser();
  ash::WebKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(true);

  Result result = RunJobAndWaitForResult();

  EXPECT_SUCCESS(result);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfDeviceIdleTimeIsLessThanIdlenessCutoffValue) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);

  const auto idleness_cutoff = base::Seconds(10);
  const auto device_idle_time = base::Seconds(9);

  SetDeviceIdleTime(device_idle_time);
  SetIdlenessCutoff(idleness_cutoff);

  Result result = RunJobAndWaitForResult();

  EXPECT_EQ(result.status, RemoteCommandJob::Status::FAILED);
  EXPECT_EQ(result.payload, CreateNotIdlePayload(device_idle_time));
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedIfDeviceIdleTimeIsMoreThanIdlenessCutoffValue) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);

  const auto idleness_cutoff = base::Seconds(10);
  const auto device_idle_time = base::Seconds(11);

  SetDeviceIdleTime(device_idle_time);
  SetIdlenessCutoff(idleness_cutoff);

  Result result = RunJobAndWaitForResult();

  EXPECT_SUCCESS(result);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfWeCantFetchTheOAuthToken) {
  LogInAsAutoLaunchedKioskAppUser();
  ClearOAuthToken();

  Result result = RunJobAndWaitForResult();

  EXPECT_ERROR(result, DeviceCommandStartCrdSessionJob::FAILURE_NO_OAUTH_TOKEN,
               kTestNoOAuthTokenReason);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailIfCrdHostReportsAnError) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);

  crd_host_delegate().MakeAccessCodeFetchFail();

  Result result = RunJobAndWaitForResult();

  EXPECT_ERROR(result, DeviceCommandStartCrdSessionJob::FAILURE_CRD_HOST_ERROR);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldPassOAuthTokenToDelegate) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken("the-oauth-token");

  Result result = RunJobAndWaitForResult();
  EXPECT_SUCCESS(result);

  EXPECT_EQ("the-oauth-token",
            crd_host_delegate().session_parameters().oauth_token);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassRobotAccountNameToDelegate) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);

  SetRobotAccountUserName("robot-account");

  Result result = RunJobAndWaitForResult();
  EXPECT_SUCCESS(result);

  EXPECT_EQ("robot-account",
            crd_host_delegate().session_parameters().user_name);
}

TEST_F(
    DeviceCommandStartCrdSessionJobTest,
    ShouldPassTerminateUponInputTrueToDelegateForKioskUserIfAckedUserPresenceSetFalse) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);

  SetAckedUserPresence(false);

  Result result = RunJobAndWaitForResult();
  EXPECT_SUCCESS(result);

  EXPECT_EQ(true,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(
    DeviceCommandStartCrdSessionJobTest,
    ShouldPassTerminateUponInputFalseToDelegateForKioskUserIfAckedUserPresenceSetTrue) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);

  SetAckedUserPresence(true);

  Result result = RunJobAndWaitForResult();
  EXPECT_SUCCESS(result);

  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassShowConfirmationDialogFalseToDelegateForKioskUser) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);

  Result result = RunJobAndWaitForResult();
  EXPECT_SUCCESS(result);

  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailIfNoUserIsLoggedIn) {
  Result result = RunJobAndWaitForResult();

  EXPECT_ERROR(result,
               DeviceCommandStartCrdSessionJob::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldSucceedForManagedGuestUser) {
  SetOAuthToken(kTestOAuthToken);

  LogInAsManagedGuestSessionUser();
  Result result = RunJobAndWaitForResult();

  EXPECT_SUCCESS(result);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldSucceedForAffiliatedUser) {
  SetOAuthToken(kTestOAuthToken);

  LogInAsAffiliatedUser();
  Result result = RunJobAndWaitForResult();

  EXPECT_SUCCESS(result);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassShowConfirmationDialogTrueToDelegateForManagedGuestUser) {
  LogInAsManagedGuestSessionUser();
  SetOAuthToken(kTestOAuthToken);

  Result result = RunJobAndWaitForResult();
  EXPECT_SUCCESS(result);

  EXPECT_EQ(true,
            crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassShowConfirmationDialogTrueToDelegateForAffiliatedUser) {
  LogInAsAffiliatedUser();
  SetOAuthToken(kTestOAuthToken);

  Result result = RunJobAndWaitForResult();
  EXPECT_SUCCESS(result);

  EXPECT_EQ(true,
            crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldNeverSendTerminateUponInputTrueToDelegateForAffiliatedUser) {
  LogInAsAffiliatedUser();
  SetOAuthToken(kTestOAuthToken);

  SetAckedUserPresence(false);

  Result result = RunJobAndWaitForResult();
  EXPECT_SUCCESS(result);

  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldNeverSendTerminateUponInputTrueToDelegateForManagedGuestUser) {
  LogInAsManagedGuestSessionUser();
  SetOAuthToken(kTestOAuthToken);

  SetAckedUserPresence(false);

  Result result = RunJobAndWaitForResult();
  EXPECT_SUCCESS(result);

  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendSuccessUmaLogWhenAutoLaunchKioskConnects) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);

  crd_host_delegate().SetHasActiveSession(true);
  base::HistogramTester histogram_tester;

  Result result = RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType",
      UmaSessionType::kAutoLaunchedKiosk, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendSuccessUmaLogWhenAffiliatedUserConnects) {
  LogInAsAffiliatedUser();
  SetOAuthToken(kTestOAuthToken);

  base::HistogramTester histogram_tester;

  Result result = RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType",
      UmaSessionType::kAffiliatedUser, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendSuccessUmaLogWhenManagedGuestSessionConnects) {
  LogInAsManagedGuestSessionUser();
  SetOAuthToken(kTestOAuthToken);

  base::HistogramTester histogram_tester;

  Result result = RunJobAndWaitForResult();

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

  Result result = RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_SERVICES_NOT_READY, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogWhenUserTypeIsNotSupported) {
  LogInAsRegularUser();

  base::HistogramTester histogram_tester;

  Result result = RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_UNSUPPORTED_USER_TYPE, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogWhenDeviceIsNotIdle) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);

  const auto idleness_cutoff = base::Seconds(10);
  const auto device_idle_time = base::Seconds(9);

  SetDeviceIdleTime(device_idle_time);
  SetIdlenessCutoff(idleness_cutoff);

  base::HistogramTester histogram_tester;

  Result result = RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::FAILURE_NOT_IDLE,
      1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogFailureNoAuthToken) {
  LogInAsAffiliatedUser();

  base::HistogramTester histogram_tester;

  Result result = RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_NO_OAUTH_TOKEN, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogFailureCrdHostError) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken(kTestOAuthToken);
  base::HistogramTester histogram_tester;

  crd_host_delegate().MakeAccessCodeFetchFail();

  Result result = RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_CRD_HOST_ERROR, 1);
}

}  // namespace policy
