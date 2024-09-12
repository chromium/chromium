// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/crd_admin_session_controller.h"

#include <optional>
#include <string>
#include <tuple>

#include "ash/constants/ash_switches.h"
#include "ash/curtain/security_curtain_controller.h"
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
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"
#include "chrome/browser/ui/ash/login/mock_login_display_host.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
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
using ::testing::Eq;

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

// Returns a lambda that can be used inside a `WillOnce` statement to respond
// to the `GetReconnectableSessionId` call.
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
  const raw_ref<RemotingServiceProxy> implementation_;
};

class SecurityCurtainControllerFake
    : public ash::curtain::SecurityCurtainController {
 public:
  SecurityCurtainControllerFake() = default;
  SecurityCurtainControllerFake(const SecurityCurtainControllerFake&) = delete;
  SecurityCurtainControllerFake& operator=(
      const SecurityCurtainControllerFake&) = delete;
  ~SecurityCurtainControllerFake() override = default;

  void Enable(InitParams params) override {
    is_enabled_ = true;
    last_init_params_ = params;
  }
  void Disable() override { is_enabled_ = false; }
  bool IsEnabled() const override { return is_enabled_; }

  InitParams last_init_params() const { return last_init_params_; }

 private:
  bool is_enabled_ = false;
  InitParams last_init_params_;
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
    return *session_controller_;
  }
  StartCrdSessionJobDelegate& delegate() {
    return session_controller_->GetDelegate();
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
    controller.Init(&local_state(), curtain_controller(),
                    done_signal.GetCallback());
    ASSERT_TRUE(done_signal.Wait());
  }

  void InitWithNoReconnectableSession(CrdAdminSessionController& controller) {
    if (base::FeatureList::IsEnabled(kEnableCrdAdminRemoteAccessV2)) {
      EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
          .WillOnce(ReplyWithSessionId(std::nullopt));
    }

    Init(controller);

    ASSERT_FALSE(controller.GetDelegate().HasActiveSession());
  }

  void TerminateActiveSession() { delegate().TerminateSession(); }

  void SimulateClientConnects(SupportHostObserver& observer) {
    // The code expects the access code before a client can connect.
    observer.OnHostStateReceivedAccessCode("code", base::Days(1));
    observer.OnHostStateConnected(kTestUserName);
    FlushForTesting(observer);
    ASSERT_TRUE(delegate().HasActiveSession());
  }

  // UI elements (like the remote activity notification) can only be shown once
  // the login screen is visible.
  void SimulateLoginScreenIsVisible() {
    session_manager().NotifyLoginOrLockScreenVisible();
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

  SecurityCurtainControllerFake& curtain_controller() {
    return curtain_controller_fake_;
  }

  mojo::Remote<SupportHostObserver>& observer_remote() { return observer_; }

  void RecreateSessionController() {
    // It's possible the old session controller has an outstanding delete of
    // its previously active session (which are deleted asynchronously), so
    // give this outstanding delete a chance to finish before we delete
    // the controller itself.
    if (session_controller_.has_value()) {
      base::RunLoop().RunUntilIdle();
    }

    session_controller_.emplace(
        std::make_unique<RemotingServiceWrapper>(&remoting_service_));
    result_.Clear();
    session_finish_result_.Clear();
  }

 protected:
  void SetUp() override {
    AshTestBase::SetUp();
    RecreateSessionController();
    session_controller().SetOAuthTokenForTesting("test-oauth-token");
  }

  void TearDown() override {
    session_controller_->Shutdown();
    AshTestBase::TearDown();
  }

 private:
  ScopedTestingLocalState local_state_;
  testing::NiceMock<ash::MockLoginDisplayHost> mock_login_display_host_;
  TestFuture<Response> result_;
  TestFuture<base::TimeDelta> session_finish_result_;
  mojo::Remote<SupportHostObserver> observer_;
  testing::StrictMock<RemotingServiceMock> remoting_service_;
  SecurityCurtainControllerFake curtain_controller_fake_;
  std::optional<CrdAdminSessionController> session_controller_;
  base::test::ScopedFeatureList feature_;
};

// Fixture for tests parameterized over boolean values.
class CrdAdminSessionControllerTestWithBoolParams
    : public CrdAdminSessionControllerTest,
      public testing::WithParamInterface<bool> {};

TEST_F(CrdAdminSessionControllerTest, ShouldPassOAuthTokenToRemotingService) {
  InitWithNoReconnectableSession(session_controller());
  session_controller().SetOAuthTokenForTesting("<the-oauth-token>");

  SupportSessionParamsPtr actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(SessionParameters{}, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  ASSERT_FALSE(actual_parameters.is_null());
  EXPECT_EQ(actual_parameters->oauth_access_token, "<the-oauth-token>");
}

TEST_F(CrdAdminSessionControllerTest, ShouldPassUserNameToRemotingService) {
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
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

TEST_F(CrdAdminSessionControllerTest, ShouldPassAdminEmailToRemotingService) {
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("the-access-code", base::Days(1));

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasAccessCode());
  EXPECT_EQ("the-access-code", response.access_code());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldSurvive2CallsToHostStateReceivedAccessCode) {
  InitWithNoReconnectableSession(session_controller());
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("the-access-code", base::Days(1));
  // This can happen if the client tried to connect and the access is denied,
  // see `It2MeHost::OnClientAccessDenied`.
  // The system should not crash when receiving the access code twice.
  observer.OnHostStateReceivedAccessCode("the-access-code", base::Days(1));

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasAccessCode());
  EXPECT_EQ("the-access-code", response.access_code());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldStartSessionIfAccessCodeFetchSucceeds) {
  InitWithNoReconnectableSession(session_controller());
  session_controller().SetOAuthTokenForTesting("test-oauth-token");

  StartCrdHostAndBindObserver();

  EXPECT_TRUE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest, ShouldReportErrorIfAccessCodeFetchFails) {
  InitWithNoReconnectableSession(session_controller());
  session_controller().FailOAuthTokenFetchForTesting();

  EXPECT_NO_CALLS(remoting_service(), StartSession);

  delegate().StartCrdHostAndGetCode(SessionParameters{}, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ(ExtendedStartCrdSessionResultCode::kFailureNoOauthToken,
            response.result_code());

  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest, ShouldReportErrorWhenClientDisconnects) {
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
  EXPECT_FALSE(delegate().HasActiveSession());

  StartCrdHostAndBindObserver();

  EXPECT_TRUE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest, ShouldCleanupSessionWhenHostDisconnects) {
  InitWithNoReconnectableSession(session_controller());
  SupportHostObserver& observer = StartCrdHostAndBindObserver();
  ASSERT_TRUE(delegate().HasActiveSession());

  observer.OnHostStateDisconnected("disconnect-reason");
  FlushForTesting(observer);

  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldCleanupSessionWhenHostObserverDisconnectsMojom) {
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
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
  InitWithNoReconnectableSession(session_controller());
  StartCrdHostAndBindObserver();
  EXPECT_TRUE(delegate().HasActiveSession());

  delegate().TerminateSession();

  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldNotCrashIfCrdHostSendsMultipleResponses) {
  InitWithNoReconnectableSession(session_controller());
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("access-code", base::Days(1));
  observer.OnHostStateStarting();
  observer.OnHostStateDisconnected(std::nullopt);
  observer.OnHostStateDisconnected(std::nullopt);
  observer.OnHostStateConnected("name");
  observer.OnHostStateError(1);
  observer.OnPolicyError();
  observer.OnInvalidDomainError();

  FlushForTesting(observer);
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldReportSessionTerminationAfterActiveSessionEnds) {
  InitWithNoReconnectableSession(session_controller());
  SupportHostObserver& observer = StartCrdHostAndBindObserver();
  constexpr auto duration = base::Seconds(2);

  SimulateClientConnects(observer);
  task_environment()->FastForwardBy(duration);
  observer.OnHostStateDisconnected("the-disconnect-reason");

  base::TimeDelta session_duration = WaitForSessionFinishResult();
  EXPECT_EQ(duration, session_duration);
}

TEST_F(
    CrdAdminSessionControllerTest,
    ShouldReportErrorWhenRemotingServiceReportsEnterpriseRemoteSupportDisabledError) {
  InitWithNoReconnectableSession(session_controller());
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateError(static_cast<int64_t>(
      remoting::protocol::ErrorCode::DISALLOWED_BY_POLICY));

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("host state error", response.error_message());
  EXPECT_EQ(ExtendedStartCrdSessionResultCode::kFailureDisabledByPolicy,
            response.result_code());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldUmaLogErrorWhenRemotingServiceReportsStateError) {
  InitWithNoReconnectableSession(session_controller());

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
        ExtendedStartCrdSessionResultCode::kFailureUnauthorizedAccount},
       {ErrorCode::REAUTHZ_POLICY_CHECK_FAILED,
        ExtendedStartCrdSessionResultCode::kFailureReauthzPolicyCheckFailed}};

  for (auto& [error_code, expected_result_code] : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << "Failure for error code " << base::ToString(error_code));
    SupportHostObserver& observer = StartCrdHostAndBindObserver();

    observer.OnHostStateError(static_cast<int64_t>(error_code));

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

  task_environment()->FastForwardBy(base::Seconds(10 * 60 + 1));

  observer.OnHostStateConnected("remote-user");
  FlushForTesting(observer);

  ASSERT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest, ShouldAcceptFastIncomingConnections) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("code", base::Days(1));

  task_environment()->FastForwardBy(base::Seconds(10 * 60 - 1));

  observer.OnHostStateConnected("remote-user");
  FlushForTesting(observer);

  ASSERT_TRUE(delegate().HasActiveSession());

  // Make sure we do not kill the session once the 10 minutes mark hit.
  task_environment()->FastForwardBy(base::Minutes(1));
  ASSERT_TRUE(delegate().HasActiveSession());
}

// Fixture for all tests related to reconnecting to an existing session.
class CrdAdminSessionControllerReconnectTest
    : public CrdAdminSessionControllerTest {
 public:
  CrdAdminSessionControllerReconnectTest() = default;
  CrdAdminSessionControllerReconnectTest(
      const CrdAdminSessionControllerReconnectTest&) = delete;
  CrdAdminSessionControllerReconnectTest& operator=(
      const CrdAdminSessionControllerReconnectTest&) = delete;
  ~CrdAdminSessionControllerReconnectTest() override = default;

  // Initializes the controller and spoofs the presence of a reconnectable
  // session.
  SupportHostObserver& InitWithReconnectableSession(
      CrdAdminSessionController& controller,
      SessionId id = kValidSessionId) {
    EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
        .WillOnce(ReplyWithSessionId(id));
    EXPECT_CALL(remoting_service(), ReconnectToSession)
        .WillOnce([&](remoting::SessionId, const std::string&,
                      StartSupportSessionCallback callback) {
          std::move(callback).Run(
              StartSupportSessionResponse::NewObserver(BindObserver()));
        });

    Init(controller);

    EXPECT_TRUE(controller.GetDelegate().HasActiveSession());
    EXPECT_TRUE(observer_remote().is_bound())
        << "StartSession() was not called";
    return *observer_remote();
  }

  void SimulateCrdClientConnects() {
    observer_remote()->OnHostStateConnecting();
    observer_remote()->OnHostStateConnected(kTestUserName);
    FlushForTesting(*observer_remote());
  }

  void SimulateCrdClientDisconnects() {
    observer_remote()->OnHostStateDisconnected(std::nullopt);
    FlushForTesting(*observer_remote());
  }

  void SimulateCrdSessionWithClient(bool is_curtained) {
    StartCrdHost(is_curtained);
    SimulateCrdClientConnects();
    SimulateCrdClientDisconnects();
  }
  void StartCrdHost(bool is_curtained) {
    SessionParameters parameters;
    parameters.curtain_local_user_session = is_curtained;
    StartCrdHostAndBindObserver(parameters);
  }
};

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldNotCurtainOffAnUncurtainedSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());

  StartCrdHost(/*is_curtained=*/false);

  EXPECT_FALSE(curtain_controller().IsEnabled());
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldCurtainOffCurtainedSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());

  StartCrdHost(/*is_curtained=*/true);

  EXPECT_TRUE(curtain_controller().IsEnabled());
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldUncurtainAndForceTerminateWhenCurtainedSessionEnds) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());

  SimulateCrdSessionWithClient(/*is_curtained=*/true);

  EXPECT_FALSE(curtain_controller().IsEnabled());

  EXPECT_EQ(GetSessionControllerClient()->request_sign_out_count(), 1);
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldResumeReconnectableSessionDuringInitIfAvailable) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  const SessionId kSessionId{123};
  const std::string kOAuthToken = "oauth-token-for-reconnect";

  session_controller().SetOAuthTokenForTesting(kOAuthToken);

  // First we should query for the reconnectable session id.
  EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
      .WillOnce(ReplyWithSessionId(kSessionId));

  // And next we should use this session id to reconnect.
  EXPECT_CALL(remoting_service(), ReconnectToSession)
      .WillOnce([&](remoting::SessionId session_id,
                    const std::string& oauth_token,
                    StartSupportSessionCallback callback) {
        std::move(callback).Run(
            StartSupportSessionResponse::NewObserver(BindObserver()));

        EXPECT_EQ(oauth_token, kOAuthToken);
        EXPECT_EQ(session_id, kSessionId);
      });

  Init(session_controller());

  EXPECT_TRUE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldHandleOauthTokenFailureWhileReconnecting) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  session_controller().FailOAuthTokenFetchForTesting();

  // First we should query for the reconnectable session id.
  EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
      .WillOnce(ReplyWithSessionId(kValidSessionId));

  // But since there is no oauth token we should never actually reconnect.
  EXPECT_NO_CALLS(remoting_service(), ReconnectToSession);

  Init(session_controller());

  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldNotResumeReconnectableSessionIfUnavailable) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  // First we return nullopt when we query for the reconnectable session id.
  EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
      .WillOnce(ReplyWithSessionId(std::nullopt));

  // Which means we should not attempt to reconnect.
  EXPECT_NO_CALLS(remoting_service(), ReconnectToSession);

  Init(session_controller());

  // And we should not have an active session.
  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldNotTryToResumeReconnectableSessionIfFeatureIsDisabled) {
  DisableFeature(kEnableCrdAdminRemoteAccessV2);

  EXPECT_NO_CALLS(remoting_service(), GetReconnectableSessionId);
  EXPECT_NO_CALLS(remoting_service(), ReconnectToSession);

  Init(session_controller());
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldNotCurtainOffIfThereIsNoReconnectableSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  InitWithNoReconnectableSession(session_controller());

  EXPECT_FALSE(curtain_controller().IsEnabled());
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldCurtainOffIfThereIsAReconnectableSession) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  InitWithReconnectableSession(session_controller());

  EXPECT_TRUE(curtain_controller().IsEnabled());
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldUncurtainAndTerminateSessionIfCrdHostFails) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  SupportHostObserver& observer =
      InitWithReconnectableSession(session_controller());
  ASSERT_TRUE(curtain_controller().IsEnabled());

  observer.OnHostStateError(static_cast<int64_t>(
      remoting::protocol::ErrorCode::AUTHENTICATION_FAILED));
  FlushForTesting(observer);

  EXPECT_FALSE(curtain_controller().IsEnabled());
  EXPECT_EQ(GetSessionControllerClient()->request_sign_out_count(), 1);
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldUncurtainAndTerminateSessionIfRemoteAdminDisconnects) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  SupportHostObserver& observer =
      InitWithReconnectableSession(session_controller());
  ASSERT_TRUE(curtain_controller().IsEnabled());

  SimulateClientConnects(observer);

  observer.OnHostStateDisconnected("disconnect-reason");
  FlushForTesting(observer);

  EXPECT_FALSE(curtain_controller().IsEnabled());

  EXPECT_EQ(GetSessionControllerClient()->request_sign_out_count(), 1);
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldUncurtainAndTerminateSessionIfRemoteAdminNeverReconnects) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  InitWithReconnectableSession(session_controller());
  ASSERT_TRUE(curtain_controller().IsEnabled());

  task_environment()->FastForwardBy(base::Minutes(16));

  EXPECT_FALSE(curtain_controller().IsEnabled());
  EXPECT_EQ(GetSessionControllerClient()->request_sign_out_count(), 1);
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       ShouldUncurtainAndTerminateSessionIfFetchingOAuthTokenFails) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);

  EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
      .WillOnce(ReplyWithSessionId(kValidSessionId));

  session_controller().FailOAuthTokenFetchForTesting();

  Init(session_controller());
  // The session is destroyed asynchronously.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(curtain_controller().IsEnabled());
  EXPECT_EQ(GetSessionControllerClient()->request_sign_out_count(), 1);
}

TEST_F(CrdAdminSessionControllerReconnectTest,
       CurtainedSessionShouldDisableInputDevices) {
  EnableFeature(kEnableCrdAdminRemoteAccessV2);
  InitWithNoReconnectableSession(session_controller());

  StartCrdHost(/*is_curtained=*/true);
  SimulateCrdClientConnects();

  EXPECT_TRUE(curtain_controller().last_init_params().disable_input_devices);
}

class CrdAdminSessionControllerNotificationTest
    : public CrdAdminSessionControllerReconnectTest {
 public:
  void SetUp() override {
    EnableFeature(kEnableCrdAdminRemoteAccessV2);

    CrdAdminSessionControllerReconnectTest::SetUp();

    InitWithNoReconnectableSession(session_controller());
  }

  void SimulateChromeRestart() {
    RecreateSessionController();
    observer_remote().reset();
    InitWithNoReconnectableSession(session_controller());
    session_controller().SetOAuthTokenForTesting("fake-oauth-token");
    SimulateLoginScreenIsVisible();
  }
};

TEST_F(CrdAdminSessionControllerNotificationTest,
       ShouldNotShowActivityNotificationIfDisabledByFeature) {
  DisableFeature(kEnableCrdAdminRemoteAccessV2);
  // Ensure disabling the feature takes effect.
  SimulateChromeRestart();

  SimulateCrdSessionWithClient(/*is_curtained=*/true);

  EXPECT_NO_CALLS(login_display_host(), ShowRemoteActivityNotificationScreen);
  SimulateChromeRestart();
}

TEST_F(CrdAdminSessionControllerNotificationTest,
       ShouldShowActivityNotificationIfThePreviousSessionWasCurtained) {
  SimulateCrdSessionWithClient(/*is_curtained=*/true);

  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen);
  SimulateChromeRestart();
}

TEST_F(CrdAdminSessionControllerNotificationTest,
       ShouldNotShowActivityNotificationIfThePreviousSessionWasNotCurtained) {
  SimulateCrdSessionWithClient(/*is_curtained=*/false);

  EXPECT_NO_CALLS(login_display_host(), ShowRemoteActivityNotificationScreen);
  SimulateChromeRestart();
}

TEST_F(CrdAdminSessionControllerNotificationTest,
       ShouldShowActivityNotificationAgainIfUserDidNotDismissIt) {
  SimulateCrdSessionWithClient(/*is_curtained=*/true);

  // The first time the notification is displayed.
  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen);
  SimulateChromeRestart();

  // And it is shown again after restarting without dismissing the notification.
  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen);
  SimulateChromeRestart();
}

TEST_F(CrdAdminSessionControllerNotificationTest,
       ShouldNotShowActivityNotificationAgainIfUserDidNotDismissIt) {
  SimulateCrdSessionWithClient(/*is_curtained=*/true);

  // The first time the notification is displayed.
  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen);
  SimulateChromeRestart();

  // It is *not* shown again after dismissing the notification.
  DismissNotification();

  EXPECT_NO_CALLS(login_display_host(), ShowRemoteActivityNotificationScreen);
  SimulateChromeRestart();
}

TEST_F(CrdAdminSessionControllerNotificationTest,
       ShouldHideActivityNotificationDuringCurtainedCrdSession) {
  SimulateCrdSessionWithClient(/*is_curtained=*/true);

  // The first time the notification is displayed.
  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen);
  SimulateChromeRestart();

  StartCrdHost(/*is_curtained=*/true);

  EXPECT_CALL(login_display_host(), HideOobeDialog);
  SimulateCrdClientConnects();

  // It should be shown again after the CRD session ends.
  EXPECT_CALL(login_display_host(), ShowRemoteActivityNotificationScreen);
  SimulateCrdClientDisconnects();
}

INSTANTIATE_TEST_SUITE_P(CrdAdminSessionControllerTestWithBoolParams,
                         CrdAdminSessionControllerTestWithBoolParams,
                         testing::Bool());

}  // namespace policy
