// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_start_crd_session_job.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/fake_cros_network_config.h"
#include "chrome/browser/ash/policy/remote_commands/user_session_type_test_util.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "extensions/common/value_builder.h"
#include "remoting/host/chromeos/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

using base::test::RepeatingTestFuture;
using base::test::TestFuture;
using extensions::DictionaryBuilder;
using test::TestSessionType;
using ResultCode = DeviceCommandStartCrdSessionJob::ResultCode;
using UmaSessionType = DeviceCommandStartCrdSessionJob::UmaSessionType;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::OncSource;
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

// Macro expecting success. We are using a macro because a function would
// report any error against the line in the function, and not against the
// place where EXPECT_SUCCESS is called.
#define EXPECT_SUCCESS(statement_)                                          \
  ({                                                                        \
    auto result_ = statement_;                                              \
    EXPECT_EQ(result_.status, RemoteCommandJob::Status::SUCCEEDED);         \
    EXPECT_THAT(result_.payload,                                            \
                base::test::IsJson(CreateSuccessPayload(kTestAccessCode))); \
  })

// Macro expecting error. We are using a macro because a function would
// report any error against the line in the function, and not against the
// place where EXPECT_ERROR is called.
#define EXPECT_ERROR(statement_, error_code, ...)                       \
  ({                                                                    \
    auto result_ = statement_;                                          \
    EXPECT_EQ(result_.status, RemoteCommandJob::Status::FAILED);        \
    EXPECT_THAT(result_.payload, base::test::IsJson(CreateErrorPayload( \
                                     error_code, ##__VA_ARGS__)));      \
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
        .Run(ResultCode::FAILURE_CRD_HOST_ERROR, std::string());
  }
}

test::NetworkBuilder CreateNetwork(NetworkType type = NetworkType::kWiFi) {
  return test::NetworkBuilder(type);
}

// Returns true if the given session type supports a 'remote support' session.
bool SupportsRemoteSupport(TestSessionType user_session_type) {
  switch (user_session_type) {
    case TestSessionType::kManuallyLaunchedArcKioskSession:
    case TestSessionType::kManuallyLaunchedWebKioskSession:
    case TestSessionType::kManuallyLaunchedKioskSession:
    case TestSessionType::kAutoLaunchedArcKioskSession:
    case TestSessionType::kAutoLaunchedWebKioskSession:
    case TestSessionType::kAutoLaunchedKioskSession:
    case TestSessionType::kManagedGuestSession:
    case TestSessionType::kAffiliatedUserSession:
      return true;

    case TestSessionType::kGuestSession:
    case TestSessionType::kUnaffiliatedUserSession:
    case TestSessionType::kNoSession:
      return false;
  }
}

// Returns true if the given session type supports a 'remote access' session.
bool SupportsRemoteAccess(TestSessionType user_session_type) {
  switch (user_session_type) {
    case TestSessionType::kNoSession:
      return true;

    case TestSessionType::kManuallyLaunchedArcKioskSession:
    case TestSessionType::kManuallyLaunchedWebKioskSession:
    case TestSessionType::kManuallyLaunchedKioskSession:
    case TestSessionType::kAutoLaunchedArcKioskSession:
    case TestSessionType::kAutoLaunchedWebKioskSession:
    case TestSessionType::kAutoLaunchedKioskSession:
    case TestSessionType::kManagedGuestSession:
    case TestSessionType::kAffiliatedUserSession:
    case TestSessionType::kGuestSession:
    case TestSessionType::kUnaffiliatedUserSession:
      return false;
  }
}

struct Result {
  RemoteCommandJob::Status status;
  std::string payload;
};

class MockCrosNetworkConfig : public FakeCrosNetworkConfigBase {
 public:
  MockCrosNetworkConfig() = default;
  MockCrosNetworkConfig(const MockCrosNetworkConfig&) = delete;
  MockCrosNetworkConfig& operator=(const MockCrosNetworkConfig&) = delete;
  ~MockCrosNetworkConfig() override = default;

  MOCK_METHOD(void,
              GetNetworkStateList,
              (chromeos::network_config::mojom::NetworkFilterPtr filter,
               GetNetworkStateListCallback callback));
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
    ash::SystemSaltGetter::Initialize();
    DeviceOAuth2TokenServiceFactory::Initialize(
        test_url_loader_factory_.GetSafeWeakWrapper(), &local_state_);
    // The token service also requires local state.
    RegisterLocalState(local_state_.registry());
  }

  void TearDown() override {
    DeviceOAuth2TokenServiceFactory::Shutdown();
    ash::SystemSaltGetter::Shutdown();

    web_kiosk_app_manager_.reset();
    arc_kiosk_app_manager_.reset();

    DeviceSettingsTestBase::TearDown();
  }

  // Create an empty payload builder.
  DictionaryBuilder Payload() const { return DictionaryBuilder(); }

  std::string CreateSuccessPayload(const std::string& access_code);
  std::string CreateErrorPayload(ResultCode result_code,
                                 const std::string& error_message);
  std::string CreateNotIdlePayload(int idle_time_in_sec);

  void LogInAsKioskUser() {
    test::StartSessionOfType(TestSessionType::kAutoLaunchedWebKioskSession,
                             user_manager());
  }

  void LogInAsRegularUser() {
    test::StartSessionOfType(TestSessionType::kUnaffiliatedUserSession,
                             user_manager());
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

  StubCrdHostDelegate& crd_host_delegate() { return crd_host_delegate_; }

  Result RunJobAndWaitForResult(
      const DictionaryBuilder& payload = DictionaryBuilder()) {
    DeviceCommandStartCrdSessionJob job{&crd_host_delegate_};

    bool initialized = InitializeJob(job, payload);
    if (!initialized) {
      ADD_FAILURE() << "Failed to initialize job";
      return Result{};
    }

    base::test::TestFuture<void> done_signal_;
    RunJob(job, done_signal_.GetCallback());
    EXPECT_TRUE(done_signal_.Wait());

    std::string response_payload =
        job.GetResultPayload() ? *job.GetResultPayload() : "{}";
    return Result{job.status(), response_payload};
  }

  bool InitializeJob(DeviceCommandStartCrdSessionJob& job,
                     const DictionaryBuilder& payload = DictionaryBuilder()) {
    bool success = job.Init(
        base::TimeTicks::Now(),
        GenerateCommandProto(kUniqueID, base::TimeDelta(), payload.ToJSON()),
        em::SignedData());

    if (oauth_token_) {
      job.SetOAuthTokenForTest(oauth_token_.value());
    }

    if (success) {
      EXPECT_EQ(kUniqueID, job.unique_id());
      EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job.status());
    }
    return success;
  }

  void RunJob(DeviceCommandStartCrdSessionJob& job,
              base::OnceClosure on_done_closure = base::OnceClosure()) {
    bool launched = job.Run(base::Time::Now(), base::TimeTicks::Now(),
                            std::move(on_done_closure));
    ASSERT_TRUE(launched);
    return;
  }

  test::FakeCrosNetworkConfig& fake_cros_network_config() {
    return fake_cros_network_config_;
  }

  ash::FakeChromeUserManager& user_manager() { return *user_manager_; }

 private:
  std::unique_ptr<ash::ArcKioskAppManager> arc_kiosk_app_manager_;
  std::unique_ptr<ash::WebKioskAppManager> web_kiosk_app_manager_;

  absl::optional<std::string> oauth_token_ = kTestOAuthToken;

  // Automatically installed as a singleton upon creation.
  std::unique_ptr<ui::UserActivityDetector> user_activity_detector_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple local_state_;

  StubCrdHostDelegate crd_host_delegate_;

  test::ScopedFakeCrosNetworkConfig fake_cros_network_config_;
};

// Fixture for tests parameterized over the possible session types
// (`TestSessionType`).
class DeviceCommandStartCrdSessionJobTestParameterized
    : public DeviceCommandStartCrdSessionJobTest,
      public ::testing::WithParamInterface<test::TestSessionType> {};

std::string DeviceCommandStartCrdSessionJobTest::CreateSuccessPayload(
    const std::string& access_code) {
  return DictionaryBuilder()
      .Set(kResultCodeFieldName, static_cast<int>(ResultCode::SUCCESS))
      .Set(kResultAccessCodeFieldName, access_code)
      .ToJSON();
}

std::string DeviceCommandStartCrdSessionJobTest::CreateErrorPayload(
    ResultCode result_code,
    const std::string& error_message = "") {
  DictionaryBuilder builder;
  builder.Set(kResultCodeFieldName, static_cast<int>(result_code));
  if (!error_message.empty()) {
    builder.Set(kResultMessageFieldName, error_message);
  }
  return builder.ToJSON();
}

std::string DeviceCommandStartCrdSessionJobTest::CreateNotIdlePayload(
    int idle_time_in_sec) {
  return DictionaryBuilder()
      .Set(kResultCodeFieldName, static_cast<int>(ResultCode::FAILURE_NOT_IDLE))
      .Set(kResultLastActivityFieldName, idle_time_in_sec)
      .ToJSON();
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedIfAccessTokenCanBeFetched) {
  LogInAsKioskUser();

  SetOAuthToken(kTestOAuthToken);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldTerminateActiveSessionAndThenSucceed) {
  LogInAsKioskUser();

  crd_host_delegate().SetHasActiveSession(true);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_TRUE(crd_host_delegate().IsActiveSessionTerminated());
}

TEST_P(DeviceCommandStartCrdSessionJobTestParameterized,
       TestRemoteSupportSessions) {
  TestSessionType user_session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(user_session_type)));

  StartSessionOfType(user_session_type, user_manager());
  Result result = RunJobAndWaitForResult();

  bool is_supported = [&]() {
    switch (user_session_type) {
      case TestSessionType::kManuallyLaunchedArcKioskSession:
      case TestSessionType::kManuallyLaunchedWebKioskSession:
      case TestSessionType::kManuallyLaunchedKioskSession:
      case TestSessionType::kAutoLaunchedArcKioskSession:
      case TestSessionType::kAutoLaunchedWebKioskSession:
      case TestSessionType::kAutoLaunchedKioskSession:
      case TestSessionType::kManagedGuestSession:
      case TestSessionType::kAffiliatedUserSession:
        return true;

      case TestSessionType::kGuestSession:
      case TestSessionType::kUnaffiliatedUserSession:
      case TestSessionType::kNoSession:
        return false;
    }
  }();

  if (is_supported) {
    EXPECT_SUCCESS(result);
  } else {
    EXPECT_ERROR(result, ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
  }
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfDeviceIdleTimeIsLessThanIdlenessCutoffValue) {
  LogInAsKioskUser();

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
  LogInAsKioskUser();

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
               ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfWeCantFetchTheOAuthToken) {
  LogInAsKioskUser();
  ClearOAuthToken();

  EXPECT_ERROR(RunJobAndWaitForResult(), ResultCode::FAILURE_NO_OAUTH_TOKEN,
               kTestNoOAuthTokenReason);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailIfCrdHostReportsAnError) {
  LogInAsKioskUser();

  crd_host_delegate().MakeAccessCodeFetchFail();

  EXPECT_ERROR(RunJobAndWaitForResult(), ResultCode::FAILURE_CRD_HOST_ERROR);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldPassOAuthTokenToDelegate) {
  LogInAsKioskUser();
  SetOAuthToken("the-oauth-token");

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_EQ("the-oauth-token",
            crd_host_delegate().session_parameters().oauth_token);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassRobotAccountNameToDelegate) {
  LogInAsKioskUser();

  SetRobotAccountUserName("robot-account");

  EXPECT_SUCCESS(RunJobAndWaitForResult());

  EXPECT_EQ("robot-account",
            crd_host_delegate().session_parameters().user_name);
}

TEST_P(DeviceCommandStartCrdSessionJobTestParameterized,
       TestTerminateUponInputForRemoteSupportWithAckedUserPresenceFalse) {
  TestSessionType user_session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(user_session_type)));

  if (!SupportsRemoteSupport(user_session_type)) {
    return;
  }

  StartSessionOfType(user_session_type, user_manager());
  Result result =
      RunJobAndWaitForResult(Payload().Set("ackedUserPresence", false));

  bool terminate_upon_input = [&]() {
    switch (user_session_type) {
      case TestSessionType::kManuallyLaunchedArcKioskSession:
      case TestSessionType::kManuallyLaunchedWebKioskSession:
      case TestSessionType::kManuallyLaunchedKioskSession:
      case TestSessionType::kAutoLaunchedArcKioskSession:
      case TestSessionType::kAutoLaunchedWebKioskSession:
      case TestSessionType::kAutoLaunchedKioskSession:
        return true;

      case TestSessionType::kManagedGuestSession:
      case TestSessionType::kAffiliatedUserSession:
        return false;

      case TestSessionType::kGuestSession:
      case TestSessionType::kUnaffiliatedUserSession:
      case TestSessionType::kNoSession:
        // Unsupported session types
        NOTREACHED();
        return false;
    }
  }();

  EXPECT_SUCCESS(result);
  EXPECT_EQ(terminate_upon_input,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_P(DeviceCommandStartCrdSessionJobTestParameterized,
       TestTerminateUponInputForRemoteSupportWithAckedUserPresenceTrue) {
  TestSessionType user_session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(user_session_type)));

  if (!SupportsRemoteSupport(user_session_type)) {
    return;
  }

  StartSessionOfType(user_session_type, user_manager());
  Result result =
      RunJobAndWaitForResult(Payload().Set("ackedUserPresence", true));

  // If the user presence is acknowledged we never need to terminate upon user
  // input.
  const bool terminate_upon_input = false;

  EXPECT_SUCCESS(result);
  EXPECT_EQ(terminate_upon_input,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_P(DeviceCommandStartCrdSessionJobTestParameterized,
       TestShowConfirmationDialogForRemoteSupport) {
  TestSessionType user_session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(user_session_type)));

  if (!SupportsRemoteSupport(user_session_type)) {
    return;
  }

  StartSessionOfType(user_session_type, user_manager());
  Result result =
      RunJobAndWaitForResult(Payload().Set("ackedUserPresence", true));

  bool show_confirmation_dialog = [&]() {
    switch (user_session_type) {
      case TestSessionType::kManuallyLaunchedArcKioskSession:
      case TestSessionType::kManuallyLaunchedWebKioskSession:
      case TestSessionType::kManuallyLaunchedKioskSession:
      case TestSessionType::kAutoLaunchedArcKioskSession:
      case TestSessionType::kAutoLaunchedWebKioskSession:
      case TestSessionType::kAutoLaunchedKioskSession:
        return false;

      case TestSessionType::kManagedGuestSession:
      case TestSessionType::kAffiliatedUserSession:
        return true;

      case TestSessionType::kGuestSession:
      case TestSessionType::kUnaffiliatedUserSession:
      case TestSessionType::kNoSession:
        // Unsupported session types
        NOTREACHED();
        return false;
    }
  }();

  EXPECT_SUCCESS(result);
  EXPECT_EQ(show_confirmation_dialog,
            crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_P(DeviceCommandStartCrdSessionJobTestParameterized,
       ShouldSendSuccessUmaLogsForRemoteSupport) {
  TestSessionType user_session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(user_session_type)));

  if (!SupportsRemoteSupport(user_session_type)) {
    return;
  }

  base::HistogramTester histogram_tester;

  StartSessionOfType(user_session_type, user_manager());
  RunJobAndWaitForResult();

  UmaSessionType expected_session_type = [&]() {
    switch (user_session_type) {
      case TestSessionType::kManuallyLaunchedArcKioskSession:
      case TestSessionType::kManuallyLaunchedWebKioskSession:
      case TestSessionType::kManuallyLaunchedKioskSession:
        return UmaSessionType::kManuallyLaunchedKiosk;
      case TestSessionType::kAutoLaunchedArcKioskSession:
      case TestSessionType::kAutoLaunchedWebKioskSession:
      case TestSessionType::kAutoLaunchedKioskSession:
        return UmaSessionType::kAutoLaunchedKiosk;

      case TestSessionType::kManagedGuestSession:
        return UmaSessionType::kManagedGuestSession;

      case TestSessionType::kAffiliatedUserSession:
        return UmaSessionType::kAffiliatedUser;

      case TestSessionType::kGuestSession:
      case TestSessionType::kUnaffiliatedUserSession:
      case TestSessionType::kNoSession:
        // Unsupported session types
        NOTREACHED();
        return UmaSessionType::kMaxValue;
    }
  }();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType", expected_session_type,
      1);
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
  LogInAsKioskUser();

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
  LogInAsKioskUser();

  ClearOAuthToken();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_NO_OAUTH_TOKEN, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogFailureCrdHostError) {
  base::HistogramTester histogram_tester;
  LogInAsKioskUser();

  crd_host_delegate().MakeAccessCodeFetchFail();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_CRD_HOST_ERROR, 1);
}

class DeviceCommandStartCrdSessionJobRemoteAccessTest
    : public DeviceCommandStartCrdSessionJobTest {
 public:
  void SetUp() override {
    EnableFeature(kEnableCrdAdminRemoteAccess);
    DeviceCommandStartCrdSessionJobTest::SetUp();
  }

  void EnableFeature(const base::Feature& feature) {
    feature_.Reset();
    feature_.InitAndEnableFeature(feature);
  }

  void DisableFeature(const base::Feature& feature) {
    feature_.Reset();
    feature_.InitAndDisableFeature(feature);
  }

  // Return a `RemoteCommand` payload that would start a remote access session.
  DictionaryBuilder RemoteAccessPayload() {
    return DictionaryBuilder(
        Payload()
            .Set("crdSessionType", CrdSessionType::REMOTE_ACCESS_SESSION)
            .BuildDict());
  }

  void AddActiveManagedNetwork() {
    fake_cros_network_config().AddActiveNetwork(
        CreateNetwork(NetworkType::kWiFi)
            .SetOncSource(OncSource::kDevicePolicy));
  }

 private:
  base::test::ScopedFeatureList feature_;
};

// Fixture for tests parameterized over the possible session types
// (`TestSessionType`).
class DeviceCommandStartCrdSessionJobRemoteAccessTestParameterized
    : public DeviceCommandStartCrdSessionJobRemoteAccessTest,
      public ::testing::WithParamInterface<test::TestSessionType> {};

TEST_P(DeviceCommandStartCrdSessionJobRemoteAccessTestParameterized,
       ShouldUseRemoteSupportIfCrdSessionTypeIsUnspecified) {
  TestSessionType user_session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(user_session_type)));
  StartSessionOfType(user_session_type, user_manager());

  auto payload_without_crd_session_type = Payload();

  Result result = RunJobAndWaitForResult(payload_without_crd_session_type);

  if (SupportsRemoteSupport(user_session_type)) {
    EXPECT_SUCCESS(result);
    // Ensure the session a remote support session (= not curtained off).
    EXPECT_FALSE(
        crd_host_delegate().session_parameters().curtain_local_user_session);
  } else {
    EXPECT_ERROR(result, ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
  }
}

TEST_P(DeviceCommandStartCrdSessionJobRemoteAccessTestParameterized,
       ShouldUseRemoteSupportIfRequestedInPayload) {
  TestSessionType user_session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(user_session_type)));
  StartSessionOfType(user_session_type, user_manager());

  Result result = RunJobAndWaitForResult(
      Payload().Set("crdSessionType", CrdSessionType::REMOTE_SUPPORT_SESSION));

  if (SupportsRemoteSupport(user_session_type)) {
    EXPECT_SUCCESS(result);
    // Ensure the session a remote support session (= not curtained off).
    EXPECT_FALSE(
        crd_host_delegate().session_parameters().curtain_local_user_session);
  } else {
    EXPECT_ERROR(result, ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
  }
}

TEST_P(DeviceCommandStartCrdSessionJobRemoteAccessTestParameterized,
       ShouldUseRemoteAccessIfRequestedInPayload) {
  TestSessionType user_session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(user_session_type)));
  StartSessionOfType(user_session_type, user_manager());
  AddActiveManagedNetwork();

  Result result = RunJobAndWaitForResult(
      Payload().Set("crdSessionType", CrdSessionType::REMOTE_ACCESS_SESSION));

  if (SupportsRemoteAccess(user_session_type)) {
    EXPECT_SUCCESS(result);
    // Ensure the session a remote access session (= curtained off).
    EXPECT_TRUE(
        crd_host_delegate().session_parameters().curtain_local_user_session);
  } else {
    EXPECT_ERROR(result, ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
  }
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTestParameterized,
       ShouldUseRemoteSupportIfFeatureIsDisabled) {
  DisableFeature(kEnableCrdAdminRemoteAccess);

  LogInAsKioskUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_FALSE(
      crd_host_delegate().session_parameters().curtain_local_user_session);
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldRejectCrdSessionTypeInPayloadIfFeatureIsDisabled) {
  DisableFeature(kEnableCrdAdminRemoteAccess);

  DeviceCommandStartCrdSessionJob job{&crd_host_delegate()};
  bool success = InitializeJob(
      job,
      Payload().Set("crdSessionType", CrdSessionType::REMOTE_ACCESS_SESSION));

  EXPECT_FALSE(success);
}

TEST_P(DeviceCommandStartCrdSessionJobRemoteAccessTestParameterized,
       RemoteAccessShouldFailForUnsupportedSessionTypes) {
  TestSessionType user_session_type = GetParam();
  SCOPED_TRACE(base::StringPrintf("Testing session type %s",
                                  SessionTypeToString(user_session_type)));

  bool is_supported = [&]() {
    switch (user_session_type) {
      case TestSessionType::kNoSession:
        return true;

      case TestSessionType::kManuallyLaunchedArcKioskSession:
      case TestSessionType::kManuallyLaunchedWebKioskSession:
      case TestSessionType::kManuallyLaunchedKioskSession:
      case TestSessionType::kAutoLaunchedArcKioskSession:
      case TestSessionType::kAutoLaunchedWebKioskSession:
      case TestSessionType::kAutoLaunchedKioskSession:
      case TestSessionType::kManagedGuestSession:
      case TestSessionType::kAffiliatedUserSession:
      case TestSessionType::kGuestSession:
      case TestSessionType::kUnaffiliatedUserSession:
        return false;
    }
  }();

  if (is_supported) {
    // This test is only about the cases where remote access is not supported.
    return;
  }

  StartSessionOfType(user_session_type, user_manager());
  Result result = RunJobAndWaitForResult(RemoteAccessPayload());

  EXPECT_ERROR(result, ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldSucceedIfNoUserIsLoggedIn) {
  AddActiveManagedNetwork();

  EXPECT_SUCCESS(RunJobAndWaitForResult(RemoteAccessPayload()));
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldNotTerminateUponInput) {
  AddActiveManagedNetwork();

  EXPECT_SUCCESS(RunJobAndWaitForResult(
      // This would enable terminate upon input in a Remote Support job.
      RemoteAccessPayload().Set("ackedUserPresense", false)));
  EXPECT_FALSE(crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldNotShowConfirmationDialog) {
  AddActiveManagedNetwork();

  EXPECT_SUCCESS(RunJobAndWaitForResult(RemoteAccessPayload()));
  EXPECT_FALSE(
      crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldRejectRequestIfThereAreNoActiveNetworks) {
  fake_cros_network_config().ClearActiveNetworks();

  EXPECT_ERROR(RunJobAndWaitForResult(RemoteAccessPayload()),
               ResultCode::FAILURE_UNMANAGED_ENVIRONMENT);
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldRejectRequestIfOnlyUnmanagedNetworksAreAvailable) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kWiFi).SetOncSource(OncSource::kNone),
      CreateNetwork(NetworkType::kEthernet).SetOncSource(OncSource::kNone),
      CreateNetwork(NetworkType::kTether).SetOncSource(OncSource::kNone),
      CreateNetwork(NetworkType::kVPN).SetOncSource(OncSource::kNone),
  });

  EXPECT_ERROR(RunJobAndWaitForResult(RemoteAccessPayload()),
               ResultCode::FAILURE_UNMANAGED_ENVIRONMENT);
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldRejectRequestIfTheOnlyManagedNetworkIsCellular) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kCellular)
          .SetOncSource(OncSource::kDevicePolicy),
  });

  EXPECT_ERROR(RunJobAndWaitForResult(RemoteAccessPayload()),
               ResultCode::FAILURE_UNMANAGED_ENVIRONMENT);
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldAllowRequestIfManagedWifiNetworkIsAvailable) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kWiFi).SetOncSource(OncSource::kDevicePolicy),
  });

  EXPECT_SUCCESS(RunJobAndWaitForResult(RemoteAccessPayload()));
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldAllowRequestIfManagedEthernetNetworkIsAvailable) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kEthernet)
          .SetOncSource(OncSource::kDevicePolicy),
  });

  EXPECT_SUCCESS(RunJobAndWaitForResult(RemoteAccessPayload()));
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldAllowRequestIfManagedTetherNetworkIsAvailable) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kTether)
          .SetOncSource(OncSource::kDevicePolicy),
  });

  EXPECT_SUCCESS(RunJobAndWaitForResult(RemoteAccessPayload()));
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldAllowRequestIfManagedVpnNetworkIsAvailable) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kVPN).SetOncSource(OncSource::kDevicePolicy),
  });

  EXPECT_SUCCESS(RunJobAndWaitForResult(RemoteAccessPayload()));
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldNotOnlyLookAtFirstNetwork) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork().SetOncSource(OncSource::kNone),
      CreateNetwork().SetOncSource(OncSource::kDevicePolicy),
      CreateNetwork().SetOncSource(OncSource::kNone),
  });

  EXPECT_SUCCESS(RunJobAndWaitForResult(RemoteAccessPayload()));
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldOnlyAllowPolicyOncSources) {
  for (auto [source, is_allowed] : {
           std::make_pair(OncSource::kNone, /*is_allowed=*/false),
           std::make_pair(OncSource::kDevice, /*is_allowed=*/false),
           std::make_pair(OncSource::kUser, /*is_allowed=*/false),
           std::make_pair(OncSource::kDevicePolicy, /*is_allowed=*/true),
           std::make_pair(OncSource::kUserPolicy, /*is_allowed=*/true),
       }) {
    fake_cros_network_config().SetActiveNetworks({
        CreateNetwork().SetOncSource(source),
    });

    auto expected_result = is_allowed ? RemoteCommandJob::Status::SUCCEEDED
                                      : RemoteCommandJob::Status::FAILED;
    auto actual_result = RunJobAndWaitForResult(RemoteAccessPayload()).status;
    EXPECT_EQ(actual_result, expected_result);
  }
}

TEST_F(DeviceCommandStartCrdSessionJobRemoteAccessTest,
       ShouldOnlyFetchTheActiveNetworks) {
  MockCrosNetworkConfig network_config_mock;
  chromeos::network_config::OverrideInProcessInstanceForTesting(
      &network_config_mock);

  TestFuture<chromeos::network_config::mojom::NetworkFilterPtr,
             MockCrosNetworkConfig::GetNetworkStateListCallback>
      get_network_state_future;
  EXPECT_CALL(network_config_mock, GetNetworkStateList)
      .WillOnce([&](auto filter, auto callback) {
        get_network_state_future.SetValue(std::move(filter),
                                          std::move(callback));
      });

  DeviceCommandStartCrdSessionJob job{&crd_host_delegate()};
  InitializeJob(job, RemoteAccessPayload());
  RunJob(job);

  auto [filter, callback] = get_network_state_future.Take();
  EXPECT_EQ(filter->filter,
            chromeos::network_config::mojom::FilterType::kActive);
  EXPECT_EQ(filter->network_type,
            chromeos::network_config::mojom::NetworkType::kAll);
  EXPECT_EQ(filter->limit, chromeos::network_config::mojom::kNoLimit);

  // We must invoke the callback to satisfy the Mojom contract
  std::move(callback).Run({});
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceCommandStartCrdSessionJobTestParameterized,
    ::testing::Values(TestSessionType::kManuallyLaunchedArcKioskSession,
                      TestSessionType::kManuallyLaunchedWebKioskSession,
                      TestSessionType::kManuallyLaunchedKioskSession,
                      TestSessionType::kAutoLaunchedArcKioskSession,
                      TestSessionType::kAutoLaunchedWebKioskSession,
                      TestSessionType::kAutoLaunchedKioskSession,
                      TestSessionType::kManagedGuestSession,
                      TestSessionType::kGuestSession,
                      TestSessionType::kAffiliatedUserSession,
                      TestSessionType::kUnaffiliatedUserSession,
                      TestSessionType::kNoSession));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceCommandStartCrdSessionJobRemoteAccessTestParameterized,
    ::testing::Values(TestSessionType::kManuallyLaunchedArcKioskSession,
                      TestSessionType::kManuallyLaunchedWebKioskSession,
                      TestSessionType::kManuallyLaunchedKioskSession,
                      TestSessionType::kAutoLaunchedArcKioskSession,
                      TestSessionType::kAutoLaunchedWebKioskSession,
                      TestSessionType::kAutoLaunchedKioskSession,
                      TestSessionType::kManagedGuestSession,
                      TestSessionType::kGuestSession,
                      TestSessionType::kAffiliatedUserSession,
                      TestSessionType::kUnaffiliatedUserSession,
                      TestSessionType::kNoSession));

}  // namespace policy
