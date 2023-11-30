// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_admin_session_controller.h"

#include <optional>
#include <string>
#include <tuple>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/strings/to_string.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/ui/mock_login_display_host.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/mojom/remote_support.mojom.h"
#include "remoting/protocol/errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0);

namespace policy {

namespace {

using SessionParameters = StartCrdSessionJobDelegate::SessionParameters;
using StartSupportSessionCallback =
    crosapi::mojom::Remoting::StartSupportSessionCallback;

using base::test::TestFuture;
using remoting::SessionId;
using remoting::features::kEnableCrdAdminRemoteAccessV2;
using remoting::mojom::StartSupportSessionResponse;
using remoting::mojom::StartSupportSessionResponsePtr;
using remoting::mojom::SupportHostObserver;
using remoting::mojom::SupportSessionParamsPtr;
using remoting::protocol::ErrorCode;

constexpr char kTestUserName[] = "test-username";
const SessionId kValidSessionId{678};

// Returns a valid response that can be sent to a `StartSupportSessionCallback`.
StartSupportSessionResponsePtr AnyResponse() {
  // Note we return an error response as the success response requires us to
  // bind an observer (`SupportHostObserver`).
  return StartSupportSessionResponse::NewSupportSessionError(
      remoting::mojom::StartSupportSessionError::kExistingAdminSession);
}

// Helper action that can be used in `EXPECT_CALL(...).WillOnce(<action>)`
// and which will store the `SupportSessionParamsPtr` argument in the given
// output parameter.
//
// Note this is very similar to ::testing::SaveArg(), but we can't use that
// because:
//     1) ::testing::SaveArg() does not support move-only arguments.
//     2) the callback must be invoked (because that's required by mojom).
auto SaveParamAndInvokeCallback(SupportSessionParamsPtr* output) {
  return [output](SupportSessionParamsPtr params,
                  const remoting::ChromeOsEnterpriseParams& enterprise_params,
                  StartSupportSessionCallback callback) {
    *output = std::move(params);
    std::move(callback).Run(AnyResponse());
  };
}

auto SaveParamAndInvokeCallback(remoting::ChromeOsEnterpriseParams* output) {
  return [output](SupportSessionParamsPtr params,
                  const remoting::ChromeOsEnterpriseParams& enterprise_params,
                  StartSupportSessionCallback callback) {
    *output = enterprise_params;
    std::move(callback).Run(AnyResponse());
  };
}

auto ReplyWithSessionId(std::optional<SessionId> id) {
  return [id](auto callback) { std::move(callback).Run(id); };
}

class RemotingServiceMock
    : public CrdAdminSessionController::RemotingServiceProxy {
 public:
  RemotingServiceMock() {
    ON_CALL(*this, StartSession)
        .WillByDefault(
            [&](SupportSessionParamsPtr,
                const remoting::ChromeOsEnterpriseParams& enterprise_params,
                StartSupportSessionCallback callback) {
              // A mojom callback *must* be called, so we will call it here
              // by default.
              std::move(callback).Run(AnyResponse());
            });
  }
  RemotingServiceMock(const RemotingServiceMock&) = delete;
  RemotingServiceMock& operator=(const RemotingServiceMock&) = delete;
  ~RemotingServiceMock() override = default;

  MOCK_METHOD(void,
              StartSession,
              (SupportSessionParamsPtr params,
               const remoting::ChromeOsEnterpriseParams& enterprise_params,
               StartSessionCallback callback));
  MOCK_METHOD(void, GetReconnectableSessionId, (SessionIdCallback));
  MOCK_METHOD(void,
              ReconnectToSession,
              (remoting::SessionId, const std::string&, StartSessionCallback));
};

// Wrapper around the `RemotingServiceMock`, solving the lifetime issue
// where this wrapper is owned by the `CrdAdminSessionController`, but we want
// to be able to access the `RemotingServiceMock` from our tests.
class RemotingServiceWrapper
    : public CrdAdminSessionController::RemotingServiceProxy {
 public:
  explicit RemotingServiceWrapper(RemotingServiceProxy* implementation)
      : implementation_(*implementation) {}
  RemotingServiceWrapper(const RemotingServiceWrapper&) = delete;
  RemotingServiceWrapper& operator=(const RemotingServiceWrapper&) = delete;
  ~RemotingServiceWrapper() override = default;

  void StartSession(SupportSessionParamsPtr params,
                    const remoting::ChromeOsEnterpriseParams& enterprise_params,
                    StartSessionCallback callback) override {
    implementation_->StartSession(std::move(params), enterprise_params,
                                  std::move(callback));
  }

  void GetReconnectableSessionId(SessionIdCallback callback) override {
    implementation_->GetReconnectableSessionId(std::move(callback));
  }

  void ReconnectToSession(SessionId session_id,
                          const std::string& oauth_access_token,
                          StartSessionCallback callback) override {
    implementation_->ReconnectToSession(session_id, oauth_access_token,
                                        std::move(callback));
  }

 private:
  const raw_ref<RemotingServiceProxy, ExperimentalAsh> implementation_;
};

// Represents the response to the CRD host request, which is
// either an access code or an error message.
class Response {
 public:
  static Response Success(const std::string& access_code) {
    return Response(access_code);
  }

  static Response Error(ExtendedStartCrdSessionResultCode result_code,
                        const std::string& error_message) {
    return Response(result_code, error_message);
  }

  Response(Response&&) = default;
  Response& operator=(Response&&) = default;
  ~Response() = default;

  bool HasAccessCode() const { return access_code_.has_value(); }
  bool HasError() const { return error_message_.has_value(); }

  std::string error_message() const {
    return error_message_.value_or("<no error received>");
  }

  ExtendedStartCrdSessionResultCode result_code() const {
    return result_code_.value_or(ExtendedStartCrdSessionResultCode::kSuccess);
  }

  std::string access_code() const {
    return access_code_.value_or("<no access code received>");
  }

 private:
  explicit Response(const std::string& access_code)
      : access_code_(access_code) {}
  Response(ExtendedStartCrdSessionResultCode result_code,
           const std::string& error_message)
      : result_code_(result_code), error_message_(error_message) {}

  std::optional<std::string> access_code_;
  std::optional<ExtendedStartCrdSessionResultCode> result_code_;
  std::optional<std::string> error_message_;
};

// Wrapper to return the `BrowserTaskEnvironment` as its base class
// `TaskEnvironment`. Without this the compiler takes the wrong constructor
// of `AshTestBase` and compilation fails.
std::unique_ptr<base::test::TaskEnvironment> CreateTaskEnvironment(
    base::test::TaskEnvironment::TimeSource time_source) {
  return std::make_unique<content::BrowserTaskEnvironment>(time_source);
}

}  // namespace

// A test class used for testing the `CrdAdminSessionController` class.
// The value is used to verify the correct delivery of individual boolean fields
// of `ChromeOsEnterpriseParams`.
class CrdAdminSessionControllerTest : public ash::AshTestBase {
 public:
  CrdAdminSessionControllerTest()
      : ash::AshTestBase(CreateTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)),
        local_state_(TestingBrowserProcess::GetGlobal()) {}
  CrdAdminSessionControllerTest(const CrdAdminSessionControllerTest&) = delete;
  CrdAdminSessionControllerTest& operator=(
      const CrdAdminSessionControllerTest&) = delete;
  ~CrdAdminSessionControllerTest() override = default;

  RemotingServiceMock& remoting_service() { return remoting_service_; }
  CrdAdminSessionController& session_controller() {
    return session_controller_;
  }
  StartCrdSessionJobDelegate& delegate() {
    return session_controller_.GetDelegate();
  }

  auto success_callback() {
    return base::BindOnce(
        [](base::OnceCallback<void(Response)> setter,
           const std::string& access_code) {
          std::move(setter).Run(Response::Success(access_code));
        },
        result_.GetCallback());
  }

  auto error_callback() {
    return base::BindOnce(
        [](base::OnceCallback<void(Response)> setter,
           ExtendedStartCrdSessionResultCode result_code,
           const std::string& error_message) {
          std::move(setter).Run(Response::Error(result_code, error_message));
        },
        result_.GetCallback());
  }

  auto session_finished_callback() {
    return base::BindOnce(
        [](base::OnceCallback<void(base::TimeDelta)> setter,
           base::TimeDelta session_duration) {
          std::move(setter).Run(session_duration);
        },
        session_finish_result_.GetCallback());
  }

  // Waits until either the success or error callback is invoked,
  // and returns the response.
  Response WaitForResponse() { return result_.Take(); }

  base::TimeDelta WaitForSessionFinishResult() {
    return session_finish_result_.Take();
  }

  // Calls StartCrdHostAndGetCode() and waits until the `SupportHostObserver` is
  // bound. This observer is used by the CRD host code to inform our delegate of
  // status updates, and is returned by this method so we can spoof these status
  // updates during our tests.
  SupportHostObserver& StartCrdHostAndBindObserver(
      SessionParameters session_parameters = SessionParameters{}) {
    EXPECT_CALL(remoting_service(), StartSession)
        .WillOnce(
            [&](SupportSessionParamsPtr params,
                const remoting::ChromeOsEnterpriseParams& enterprise_params,
                StartSupportSessionCallback callback) {
              std::move(callback).Run(
                  StartSupportSessionResponse::NewObserver(BindObserver()));
            });

    delegate().StartCrdHostAndGetCode(session_parameters, success_callback(),
                                      error_callback(),
                                      session_finished_callback());

    EXPECT_TRUE(observer_.is_bound()) << "StartSession() was not called";
    return *observer_;
  }

  void Init(CrdAdminSessionController& controller) {
    TestFuture<void> done_signal;
    controller.Init(&local_state(), done_signal.GetCallback());
    ASSERT_TRUE(done_signal.Wait());
  }

  void InitWithNoReconnectableSession(CrdAdminSessionController& controller) {
    EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
        .WillOnce(ReplyWithSessionId(std::nullopt));

    Init(controller);

    ASSERT_FALSE(controller.GetDelegate().HasActiveSession());
  }

  void TerminateActiveSession() { delegate().TerminateSession(); }

  void SimilateClientConnects(SupportHostObserver& observer) {
    // The code expects the access code before a client can connect.
    observer.OnHostStateReceivedAccessCode("code", base::Days(1));
    observer.OnHostStateConnected(kTestUserName);
    FlushForTesting(observer);
    ASSERT_TRUE(delegate().HasActiveSession());
  }

  void SimulateLoginScreenIsVisible() {
    // Notifies the observers that the login screen is visible and ensure the
    // `RemoteActivityNotificationController::Init()` is called.
    session_manager().NotifyLoginOrLockScreenVisible();
  }

  void SimulateRestart() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kFirstExecAfterBoot);
  }

  const aura::Window& GetLockScreenContainersContainer() {
    return CHECK_DEREF(ash::Shell::Get()->GetPrimaryRootWindow()->GetChildById(
        ash::kShellWindowId_LockScreenContainersContainer));
  }

  ash::MockLoginDisplayHost& login_display_host() {
    return mock_login_display_host_;
  }

  void FlushForTesting(SupportHostObserver& observer) {
    CHECK_EQ(&observer, observer_.get());
    observer_.FlushForTesting();
  }

  mojo::PendingReceiver<SupportHostObserver> BindObserver() {
    return observer_.BindNewPipeAndPassReceiver();
  }

  void UnbindMojomConnection(SupportHostObserver& observer) {
    CHECK_EQ(&observer, observer_.get());
    observer_.reset();
  }

  void DisableFeature(const base::Feature& feature) {
    feature_.Reset();
    feature_.InitAndDisableFeature(feature);
  }

  void EnableFeature(const base::Feature& feature) {
    feature_.Reset();
    feature_.InitAndEnableFeature(feature);
  }

  bool GetPref(const char* pref_name) {
    return local_state().GetBoolean(pref_name);
  }

  void SetPref(const char* pref_name, bool value) {
    local_state().SetBoolean(pref_name, value);
  }

  void DismissNotification() { SetPref(prefs::kRemoteAdminWasPresent, false); }

  TestingPrefServiceSimple& local_state() { return *local_state_.Get(); }

  session_manager::SessionManager& session_manager() {
    return CHECK_DEREF(session_manager::SessionManager::Get());
  }

 private:
  void TearDown() override {
    session_controller_.Shutdown();
    AshTestBase::TearDown();
  }

  ScopedTestingLocalState local_state_;
  testing::StrictMock<ash::MockLoginDisplayHost> mock_login_display_host_;
  TestFuture<Response> result_;
  TestFuture<base::TimeDelta> session_finish_result_;
  mojo::Remote<SupportHostObserver> observer_;
  testing::StrictMock<RemotingServiceMock> remoting_service_;
  CrdAdminSessionController session_controller_{
      std::make_unique<RemotingServiceWrapper>(&remoting_service_)};
  base::test::ScopedFeatureList feature_;
};

// Fixture for tests parameterized over boolean values.
class CrdAdminSessionControllerTestWithBoolParams
    : public CrdAdminSessionControllerTest,
      public testing::WithParamInterface<bool> {};

TEST_F(CrdAdminSessionControllerTest, ShouldPassOAuthTokenToRemotingService) {
  SessionParameters parameters;
  parameters.oauth_token = "<the-oauth-token>";

  SupportSessionParamsPtr actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  ASSERT_FALSE(actual_parameters.is_null());
  EXPECT_EQ(actual_parameters->oauth_access_token, "<the-oauth-token>");
}

TEST_F(CrdAdminSessionControllerTest, ShouldPassUserNameToRemotingService) {
  SessionParameters parameters;
  parameters.user_name = "<the-user-name>";

  SupportSessionParamsPtr actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  ASSERT_FALSE(actual_parameters.is_null());
  EXPECT_EQ(actual_parameters->user_name, "<the-user-name>");
}

TEST_P(CrdAdminSessionControllerTestWithBoolParams,
       ShouldPassShowConfirmationDialogToRemotingService) {
  SessionParameters parameters;
  parameters.show_confirmation_dialog = GetParam();

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  EXPECT_NE(actual_parameters.suppress_notifications, GetParam());
  EXPECT_NE(actual_parameters.suppress_user_dialogs, GetParam());
}

TEST_P(CrdAdminSessionControllerTestWithBoolParams,
       ShouldPassTerminateUponInputToRemotingService) {
  SessionParameters parameters;
  parameters.terminate_upon_input = GetParam();

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  EXPECT_EQ(actual_parameters.terminate_upon_input, GetParam());
}

TEST_P(CrdAdminSessionControllerTestWithBoolParams,
       ShouldPassAllowReconnectionsToRemotingService) {
  SessionParameters parameters;
  parameters.allow_reconnections = GetParam();

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  EXPECT_EQ(actual_parameters.allow_reconnections, GetParam());
}

TEST_F(CrdAdminSessionControllerTest, ShouldPassAdminEmailToRemotingService) {
  SessionParameters parameters;
  parameters.admin_email = "the.admin@email.com";

  SupportSessionParamsPtr actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  EXPECT_EQ(actual_parameters->authorized_helper, "the.admin@email.com");
}

TEST_P(CrdAdminSessionControllerTestWithBoolParams,
       ShouldPassCurtainLocalUserSessionToRemotingService) {
  SessionParameters parameters;
  parameters.curtain_local_user_session = GetParam();

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  EXPECT_EQ(actual_parameters.curtain_local_user_session, GetParam());
}

TEST_P(CrdAdminSessionControllerTestWithBoolParams,
       ShouldPassAllowTroubleshootingToolsToRemotingService) {
  SessionParameters parameters;
  parameters.allow_troubleshooting_tools = GetParam();

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  EXPECT_EQ(actual_parameters.allow_troubleshooting_tools, GetParam());
}

TEST_P(CrdAdminSessionControllerTestWithBoolParams,
       ShouldPassShowTroubleshootingToolsToRemotingService) {
  SessionParameters parameters;
  parameters.show_troubleshooting_tools = GetParam();

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  EXPECT_EQ(actual_parameters.show_troubleshooting_tools, GetParam());
}

TEST_P(CrdAdminSessionControllerTestWithBoolParams,
       ShouldPassAllowFileTransferToRemotingService) {
  SessionParameters parameters;
  parameters.allow_file_transfer = GetParam();

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  EXPECT_EQ(actual_parameters.allow_file_transfer, GetParam());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldReportErrorIfStartSessionReturnsError) {
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce([](SupportSessionParamsPtr params,
                   const remoting::ChromeOsEnterpriseParams& enterprise_params,
                   StartSupportSessionCallback callback) {
        auto response = StartSupportSessionResponse::NewSupportSessionError(
            remoting::mojom::StartSupportSessionError::kExistingAdminSession);
        std::move(callback).Run(std::move(response));
      });

  delegate().StartCrdHostAndGetCode(SessionParameters{}, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ(ExtendedStartCrdSessionResultCode::kFailureCrdHostError,
            response.result_code());
}

TEST_F(CrdAdminSessionControllerTest, ShouldReturnAccessCode) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("the-access-code", base::Days(1));

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasAccessCode());
  EXPECT_EQ("the-access-code", response.access_code());
}

TEST_F(CrdAdminSessionControllerTest, ShouldReportErrorWhenClientDisconnects) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateDisconnected("the-disconnect-reason");

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("client disconnected", response.error_message());
  EXPECT_EQ(ExtendedStartCrdSessionResultCode::kHostSessionDisconnected,
            response.result_code());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldReportErrorWhenRemotingServiceReportsPolicyError) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnPolicyError();

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("policy error", response.error_message());
  EXPECT_EQ(ExtendedStartCrdSessionResultCode::kFailureHostPolicyError,
            response.result_code());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldReportErrorWhenRemotingServiceReportsInvalidDomainError) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnInvalidDomainError();

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("invalid domain error", response.error_message());
  EXPECT_EQ(ExtendedStartCrdSessionResultCode::kFailureHostInvalidDomainError,
            response.result_code());
}

TEST_F(CrdAdminSessionControllerTest,
       HasActiveSessionShouldBeTrueWhenASessionIsStarted) {
  EXPECT_FALSE(delegate().HasActiveSession());

  StartCrdHostAndBindObserver();

  EXPECT_TRUE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest, ShouldCleanupSessionWhenHostDisconnects) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();
  ASSERT_TRUE(delegate().HasActiveSession());

  observer.OnHostStateDisconnected("disconnect-reason");
  FlushForTesting(observer);

  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldCleanupSessionWhenHostObserverDisconnectsMojom) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();
  ASSERT_TRUE(delegate().HasActiveSession());

  UnbindMojomConnection(observer);
  // At this point we want to use `FlushObserver` so the mojom message about
  // the destruction can be delivered, but we can't since the observer itself is
  // destroyed.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldCleanupSessionWhenWeFailToStartTheHost) {
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce([](SupportSessionParamsPtr params,
                   const remoting::ChromeOsEnterpriseParams& enterprise_params,
                   StartSupportSessionCallback callback) {
        auto response = StartSupportSessionResponse::NewSupportSessionError(
            remoting::mojom::StartSupportSessionError::kExistingAdminSession);
        std::move(callback).Run(std::move(response));
      });

  delegate().StartCrdHostAndGetCode(SessionParameters{}, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  WaitForResponse();

  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldCleanupSessionWhenCallingTerminateSession) {
  StartCrdHostAndBindObserver();
  EXPECT_TRUE(delegate().HasActiveSession());

  delegate().TerminateSession();

  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldNotCrashIfCrdHostSendsMultipleResponses) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("access-code", base::Days(1));
  observer.OnHostStateStarting();
  observer.OnHostStateDisconnected(absl::nullopt);
  observer.OnHostStateDisconnected(absl::nullopt);
  observer.OnHostStateConnected("name");
  observer.OnHostStateError(1);
  observer.OnPolicyError();
  observer.OnInvalidDomainError();

  FlushForTesting(observer);
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldReportSessionTerminationAfterActiveSessionEnds) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();
  constexpr auto duration = base::Seconds(2);

  SimilateClientConnects(observer);
  task_environment()->FastForwardBy(duration);
  observer.OnHostStateDisconnected("the-disconnect-reason");

  base::TimeDelta session_duration = WaitForSessionFinishResult();
  EXPECT_EQ(duration, session_duration);
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldResumeReconnectableSessionDuringInitIfAvailable) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  const SessionId kSessionId{123};
  const std::string kOAuthToken = "oauth-token-for-reconnect";

  session_controller().SetOAuthTokenForTesting(kOAuthToken);

  // First we should query for the reconnectable session id.
  EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
      .WillOnce(ReplyWithSessionId(kSessionId));

  // And next we should use this session id to reconnect.
  EXPECT_CALL(remoting_service(),
              ReconnectToSession(kSessionId, testing::_, testing::_))
      .WillOnce([&](remoting::SessionId, const std::string& oauth_token,
                    StartSupportSessionCallback callback) {
        std::move(callback).Run(
            StartSupportSessionResponse::NewObserver(BindObserver()));
        EXPECT_EQ(oauth_token, kOAuthToken);
      });

  Init(session_controller());

  EXPECT_TRUE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldHandleOauthTokenFailureWhileReconnecting) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  session_controller().ClearOAuthTokenForTesting();

  // First we should query for the reconnectable session id.
  EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
      .WillOnce(ReplyWithSessionId(kValidSessionId));

  // But since there is no oauth token we should never actually reconnect.
  EXPECT_NO_CALLS(remoting_service(), ReconnectToSession);

  Init(session_controller());

  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldNotResumeReconnectableSessionIfUnavailable) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  // First we return nullopt when we query for the reconnectable session id.
  EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
      .WillOnce(ReplyWithSessionId(std::nullopt));

  // Which means we should not attempt to reconnect.
  EXPECT_NO_CALLS(remoting_service(), ReconnectToSession);

  TestFuture<void> done_signal;
  session_controller().Init(&local_state(), done_signal.GetCallback());

  // The `done_signal` should still be invoked.
  ASSERT_TRUE(done_signal.Wait());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldNotHaveActiveSessionIfReconnectableSessionIsUnavailable) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  // Indicate there is no reconnectable session by returning nullopt when we
  // query for the reconnectable session id.
  EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
      .WillOnce(ReplyWithSessionId(std::nullopt));

  Init(session_controller());

  ASSERT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldNotTryToResumeReconnectableSessionIfFeatureIsDisabled) {
  DisableFeature(kEnableCrdAdminRemoteAccessV2);

  EXPECT_NO_CALLS(remoting_service(), GetReconnectableSessionId);
  EXPECT_NO_CALLS(remoting_service(), ReconnectToSession);

  Init(session_controller());
}

TEST_F(
    CrdAdminSessionControllerTest,
    ShouldReportErrorWhenRemotingServiceReportsEnterpriseRemoteSupportDisabledError) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateError(
      remoting::protocol::ErrorCode::DISALLOWED_BY_POLICY);

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("host state error", response.error_message());
  EXPECT_EQ(ExtendedStartCrdSessionResultCode::kFailureDisabledByPolicy,
            response.result_code());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldNotShowActivityNotificationIfDisabledByFeature) {
  DisableFeature(kEnableCrdAdminRemoteAccessV2);
  Init(session_controller());

  SessionParameters parameters;
  parameters.curtain_local_user_session = true;
  SupportHostObserver& observer = StartCrdHostAndBindObserver(parameters);
  observer.OnHostStateConnected(kTestUserName);
  FlushForTesting(observer);

  EXPECT_NO_CALLS(login_display_host(), ShowRemoteActivityNotificationScreen());

  SimulateLoginScreenIsVisible();
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldShowActivityNotificationIfThePreviousSessionWasCurtained) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());

  SessionParameters parameters;
  parameters.curtain_local_user_session = true;
  SupportHostObserver& observer = StartCrdHostAndBindObserver(parameters);
  observer.OnHostStateConnected(kTestUserName);
  FlushForTesting(observer);

  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen())
      .Times(1);

  SimulateLoginScreenIsVisible();
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldNotShowActivityNotificationIfThePreviousSessionWasNotCurtained) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());

  SessionParameters parameters;
  parameters.curtain_local_user_session = false;
  SupportHostObserver& observer = StartCrdHostAndBindObserver(parameters);
  observer.OnHostStateConnected(kTestUserName);
  FlushForTesting(observer);

  EXPECT_NO_CALLS(login_display_host(), ShowRemoteActivityNotificationScreen());

  SimulateLoginScreenIsVisible();
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldShowActivityNotificationAgainIfUserDidNotDismissIt) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());

  SessionParameters parameters;
  parameters.curtain_local_user_session = true;
  SupportHostObserver& observer = StartCrdHostAndBindObserver(parameters);
  observer.OnHostStateConnected(kTestUserName);
  FlushForTesting(observer);

  // The first time the notification is displayed.
  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen())
      .Times(1);
  SimulateLoginScreenIsVisible();

  SimulateRestart();

  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen())
      .Times(1);
  SimulateLoginScreenIsVisible();
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldNotShowActivityNotificationAgainIfUserDismissedIt) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());

  SessionParameters parameters;
  parameters.curtain_local_user_session = true;
  SupportHostObserver& observer = StartCrdHostAndBindObserver(parameters);
  observer.OnHostStateConnected(kTestUserName);
  FlushForTesting(observer);
  TerminateActiveSession();

  // The first time the notification is displayed.
  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen())
      .Times(1);
  SimulateLoginScreenIsVisible();

  DismissNotification();
  SimulateRestart();

  EXPECT_NO_CALLS(login_display_host(), ShowRemoteActivityNotificationScreen());

  SimulateLoginScreenIsVisible();
}

TEST_F(
    CrdAdminSessionControllerTest,
    ShouldShowActivityNotificationAgainIfUserDismissedItDuringACurtainedSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());

  SessionParameters parameters;
  parameters.curtain_local_user_session = true;
  SupportHostObserver& observer = StartCrdHostAndBindObserver(parameters);
  SimilateClientConnects(observer);

  // The first time the notification is displayed.
  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen())
      .Times(1);
  SimulateLoginScreenIsVisible();

  DismissNotification();
  SimulateRestart();

  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen())
      .Times(1);
  SimulateLoginScreenIsVisible();
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldUmaLogErrorWhenRemotingServiceReportsStateError) {
  const std::tuple<ErrorCode, ExtendedStartCrdSessionResultCode> test_cases[] =
      {{ErrorCode::OK, ExtendedStartCrdSessionResultCode::kSuccess},
       {ErrorCode::PEER_IS_OFFLINE,
        ExtendedStartCrdSessionResultCode::kFailurePeerIsOffline},
       {ErrorCode::SESSION_REJECTED,
        ExtendedStartCrdSessionResultCode::kFailureSessionRejected},
       {ErrorCode::INCOMPATIBLE_PROTOCOL,
        ExtendedStartCrdSessionResultCode::kFailureIncompatibleProtocol},
       {ErrorCode::AUTHENTICATION_FAILED,
        ExtendedStartCrdSessionResultCode::kFailureAuthenticationFailed},
       {ErrorCode::INVALID_ACCOUNT,
        ExtendedStartCrdSessionResultCode::kFailureInvalidAccount},
       {ErrorCode::CHANNEL_CONNECTION_ERROR,
        ExtendedStartCrdSessionResultCode::kFailureChannelConnectionError},
       {ErrorCode::SIGNALING_ERROR,
        ExtendedStartCrdSessionResultCode::kFailureSignalingError},
       {ErrorCode::SIGNALING_TIMEOUT,
        ExtendedStartCrdSessionResultCode::kFailureSignalingTimeout},
       {ErrorCode::HOST_OVERLOAD,
        ExtendedStartCrdSessionResultCode::kFailureHostOverload},
       {ErrorCode::MAX_SESSION_LENGTH,
        ExtendedStartCrdSessionResultCode::kFailureMaxSessionLength},
       {ErrorCode::HOST_CONFIGURATION_ERROR,
        ExtendedStartCrdSessionResultCode::kFailureHostConfigurationError},
       {ErrorCode::UNKNOWN_ERROR,
        ExtendedStartCrdSessionResultCode::kFailureUnknownError},
       {ErrorCode::ELEVATION_ERROR,
        ExtendedStartCrdSessionResultCode::kFailureUnknownError},
       {ErrorCode::HOST_CERTIFICATE_ERROR,
        ExtendedStartCrdSessionResultCode::kFailureHostCertificateError},
       {ErrorCode::HOST_REGISTRATION_ERROR,
        ExtendedStartCrdSessionResultCode::kFailureHostRegistrationError},
       {ErrorCode::EXISTING_ADMIN_SESSION,
        ExtendedStartCrdSessionResultCode::kFailureExistingAdminSession},
       {ErrorCode::AUTHZ_POLICY_CHECK_FAILED,
        ExtendedStartCrdSessionResultCode::kFailureAuthzPolicyCheckFailed},
       {ErrorCode::LOCATION_AUTHZ_POLICY_CHECK_FAILED,
        ExtendedStartCrdSessionResultCode::
            kFailureLocationAuthzPolicyCheckFailed},
       {ErrorCode::UNAUTHORIZED_ACCOUNT,
        ExtendedStartCrdSessionResultCode::kFailureUnauthorizedAccount}};

  for (auto& [error_code, expected_result_code] : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << "Failure for error code " << base::ToString(error_code));
    SupportHostObserver& observer = StartCrdHostAndBindObserver();

    observer.OnHostStateError(error_code);

    Response response = WaitForResponse();
    ASSERT_TRUE(response.HasError());
    EXPECT_EQ("host state error", response.error_message());
    EXPECT_EQ(expected_result_code, response.result_code());

    UnbindMojomConnection(observer);
    delegate().TerminateSession();
  }
}

TEST_F(CrdAdminSessionControllerTest, ShouldBlockLateIncomingConnections) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("code", base::Days(1));

  task_environment()->FastForwardBy(base::Seconds(15 * 60 + 1));

  observer.OnHostStateConnected("remote-user");
  FlushForTesting(observer);

  ASSERT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest, ShouldAcceptFastIncomingConnections) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("code", base::Days(1));

  task_environment()->FastForwardBy(base::Seconds(15 * 60 - 1));

  observer.OnHostStateConnected("remote-user");
  FlushForTesting(observer);

  ASSERT_TRUE(delegate().HasActiveSession());

  // Make sure we do not kill the session once the 15 minutes mark hit.
  task_environment()->FastForwardBy(base::Minutes(1));
  ASSERT_TRUE(delegate().HasActiveSession());
}

INSTANTIATE_TEST_SUITE_P(CrdAdminSessionControllerTestWithBoolParams,
                         CrdAdminSessionControllerTestWithBoolParams,
                         testing::Bool());

}  // namespace policy
