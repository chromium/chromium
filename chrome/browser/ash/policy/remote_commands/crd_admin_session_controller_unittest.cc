// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_admin_session_controller.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/remote_support.mojom.h"
#include "remoting/protocol/errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0);

namespace policy {

namespace {

using SessionParameters = StartCrdSessionJobDelegate::SessionParameters;
using StartSupportSessionCallback =
    crosapi::mojom::Remoting::StartSupportSessionCallback;

using base::test::TestFuture;
using remoting::mojom::StartSupportSessionResponse;
using remoting::mojom::StartSupportSessionResponsePtr;
using remoting::mojom::SupportHostObserver;
using remoting::mojom::SupportSessionParamsPtr;

constexpr char kTestUserName[] = "test-username";

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
              (remoting::SessionId, StartSessionCallback));
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

  void ReconnectToSession(remoting::SessionId session_id,
                          StartSessionCallback callback) override {
    implementation_->ReconnectToSession(session_id, std::move(callback));
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

  static Response Error(ResultCode error_code,
                        const std::string& error_message) {
    return Response(error_code, error_message);
  }

  Response(Response&&) = default;
  Response& operator=(Response&&) = default;
  ~Response() = default;

  bool HasAccessCode() const { return access_code_.has_value(); }
  bool HasError() const { return error_message_.has_value(); }

  std::string error_message() const {
    return error_message_.value_or("<no error received>");
  }

  ResultCode error_code() const {
    return error_code_.value_or(ResultCode::SUCCESS);
  }

  std::string access_code() const {
    return access_code_.value_or("<no access code received>");
  }

 private:
  explicit Response(const std::string& access_code)
      : access_code_(access_code) {}
  Response(ResultCode error_code, const std::string& error_message)
      : error_code_(error_code), error_message_(error_message) {}

  absl::optional<std::string> access_code_;
  absl::optional<ResultCode> error_code_;
  absl::optional<std::string> error_message_;
};

}  // namespace

// A test class used for testing the `CrdAdminSessionController` class.
// Use this class as fixture for parameterized tests over boolean value.
// The value is used to verify the correct delivery of individual boolean fields
// of `ChromeOsEnterpriseParams`.
class CrdAdminSessionControllerTest : public testing::TestWithParam<bool> {
 public:
  CrdAdminSessionControllerTest() = default;
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
        [](base::OnceCallback<void(Response)> setter, ResultCode error_code,
           const std::string& error_message) {
          std::move(setter).Run(Response::Error(error_code, error_message));
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

  // Calls StartCrdHostAndGetCode() and waits until the
  // `SupportHostObserver` is bound.
  // This observer is used by the CRD host code to inform our delegate of status
  // updates, and is returned by this method so we can spoof these status
  // updates during our tests.
  SupportHostObserver& StartCrdHostAndBindObserver() {
    EXPECT_CALL(remoting_service(), StartSession)
        .WillOnce(
            [&](SupportSessionParamsPtr params,
                const remoting::ChromeOsEnterpriseParams& enterprise_params,
                StartSupportSessionCallback callback) {
              std::move(callback).Run(
                  StartSupportSessionResponse::NewObserver(BindObserver()));
            });

    delegate().StartCrdHostAndGetCode(SessionParameters{}, success_callback(),
                                      error_callback(),
                                      session_finished_callback());

    EXPECT_TRUE(observer_.is_bound()) << "StartSession() was not called";
    return *observer_;
  }

  void FlushForTesting() { observer_.FlushForTesting(); }

  base::test::SingleThreadTaskEnvironment& environment() {
    return environment_;
  }

  mojo::PendingReceiver<SupportHostObserver> BindObserver() {
    return observer_.BindNewPipeAndPassReceiver();
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestFuture<Response> result_;
  TestFuture<base::TimeDelta> session_finish_result_;
  mojo::Remote<SupportHostObserver> observer_;
  RemotingServiceMock remoting_service_;
  CrdAdminSessionController session_controller_{
      std::make_unique<RemotingServiceWrapper>(&remoting_service_)};
};

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
  EXPECT_EQ(actual_parameters->oauth_access_token, "oauth2:<the-oauth-token>");
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

TEST_P(CrdAdminSessionControllerTest,
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

TEST_P(CrdAdminSessionControllerTest,
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

TEST_P(CrdAdminSessionControllerTest,
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

TEST_P(CrdAdminSessionControllerTest,
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

TEST_P(CrdAdminSessionControllerTest,
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

TEST_P(CrdAdminSessionControllerTest,
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

TEST_P(CrdAdminSessionControllerTest,
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
  EXPECT_EQ(ResultCode::FAILURE_CRD_HOST_ERROR, response.error_code());
}

TEST_F(CrdAdminSessionControllerTest, ShouldReturnAccessCode) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("the-access-code", base::Days(1));

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasAccessCode());
  EXPECT_EQ("the-access-code", response.access_code());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldReportErrorWhenHostStateChangesToDisconnected) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateDisconnected("the-disconnect-reason");

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("host disconnected", response.error_message());
  EXPECT_EQ(ResultCode::FAILURE_CRD_HOST_ERROR, response.error_code());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldReportErrorWhenRemotingServiceReportsPolicyError) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnPolicyError();

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("policy error", response.error_message());
  EXPECT_EQ(ResultCode::FAILURE_CRD_HOST_ERROR, response.error_code());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldReportErrorWhenRemotingServiceReportsInvalidDomainError) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnInvalidDomainError();

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("invalid domain error", response.error_message());
  EXPECT_EQ(ResultCode::FAILURE_CRD_HOST_ERROR, response.error_code());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldReportErrorWhenRemotingServiceReportsStateError) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateError(123);

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("host state error", response.error_message());
  EXPECT_EQ(ResultCode::FAILURE_CRD_HOST_ERROR, response.error_code());
}

TEST_F(CrdAdminSessionControllerTest,
       HasActiveSessionShouldBeTrueWhenASessionIsStarted) {
  EXPECT_FALSE(delegate().HasActiveSession());

  delegate().StartCrdHostAndGetCode(SessionParameters{}, success_callback(),
                                    error_callback(),
                                    session_finished_callback());

  EXPECT_TRUE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       TerminateSessionShouldTerminateTheActiveSession) {
  delegate().StartCrdHostAndGetCode(SessionParameters{}, success_callback(),
                                    error_callback(),
                                    session_finished_callback());
  EXPECT_TRUE(delegate().HasActiveSession());

  TestFuture<void> terminate_session_future;
  delegate().TerminateSession(terminate_session_future.GetCallback());

  ASSERT_TRUE(terminate_session_future.Wait())
      << "TerminateSession did not invoke the callback.";
  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldNotCrashIfCrdHostSendsMultipleResponses) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("access-code", base::Days(1));
  observer.OnHostStateStarting();
  observer.OnHostStateDisconnected(absl::nullopt);
  observer.OnHostStateError(1);
  observer.OnPolicyError();
  observer.OnInvalidDomainError();

  FlushForTesting();
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldReportSessionTerminationAfterActiveSessionEnds) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();
  constexpr auto duration = base::Seconds(2);

  observer.OnHostStateConnected(kTestUserName);
  observer.OnHostStateStarting();
  environment().FastForwardBy(duration);
  observer.OnHostStateDisconnected("the-disconnect-reason");

  base::TimeDelta session_duration = WaitForSessionFinishResult();
  EXPECT_EQ(duration, session_duration);
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldResumeReconnectableSessionIfAvailable) {
  const remoting::SessionId kSessionId{123};

  // First we should query for the reconnectable session id.
  EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
      .WillOnce([&](auto callback) { std::move(callback).Run(kSessionId); });

  // And next we should use this session id to reconnect.
  EXPECT_CALL(remoting_service(), ReconnectToSession(kSessionId, testing::_))
      .WillOnce([&](remoting::SessionId, StartSupportSessionCallback callback) {
        std::move(callback).Run(
            StartSupportSessionResponse::NewObserver(BindObserver()));
      });

  TestFuture<void> done_signal;
  delegate().TryToReconnect(done_signal.GetCallback());
  ASSERT_TRUE(done_signal.Wait());

  EXPECT_TRUE(delegate().HasActiveSession());
}

TEST_F(CrdAdminSessionControllerTest,
       ShouldNotResumeReconnectableSessionIfUnavailable) {
  // First we return nullopt when we query for the reconnectable session id.
  EXPECT_CALL(remoting_service(), GetReconnectableSessionId)
      .WillOnce([&](auto callback) { std::move(callback).Run(absl::nullopt); });

  // Which means we should not attempt to reconnect.
  EXPECT_NO_CALLS(remoting_service(), ReconnectToSession);

  TestFuture<void> done_signal;
  delegate().TryToReconnect(done_signal.GetCallback());

  // The `done_signal` should still be invoked.
  ASSERT_TRUE(done_signal.Wait());
}

TEST_F(
    CrdAdminSessionControllerTest,
    ShouldReportErrorWhenRemotingServiceReportsEnterpriseRemoteSupportDisabledError) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateError(
      remoting::protocol::ErrorCode::DISALLOWED_BY_POLICY);

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("enterprise remote support disabled", response.error_message());
  EXPECT_EQ(ResultCode::FAILURE_DISABLED_BY_POLICY, response.error_code());
}

INSTANTIATE_TEST_SUITE_P(CrdAdminSessionControllerTest,
                         CrdAdminSessionControllerTest,
                         testing::Bool());

}  // namespace policy
