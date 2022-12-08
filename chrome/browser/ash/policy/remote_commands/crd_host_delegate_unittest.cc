// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_host_delegate.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/remote_support.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using SessionParameters =
    DeviceCommandStartCrdSessionJob::Delegate::SessionParameters;
using StartSupportSessionCallback =
    crosapi::mojom::Remoting::StartSupportSessionCallback;

using ResultCode = DeviceCommandStartCrdSessionJob::ResultCode;
using remoting::mojom::StartSupportSessionResponse;
using remoting::mojom::StartSupportSessionResponsePtr;
using remoting::mojom::SupportHostObserver;
using remoting::mojom::SupportSessionParamsPtr;

// Returns a valid response that can be sent to a |StartSupportSessionCallback|.
StartSupportSessionResponsePtr AnyResponse() {
  // Note we return an error response as the success response requires us to
  // bind an observer (|SupportHostObserver|).
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

class RemotingServiceMock : public CrdHostDelegate::RemotingServiceProxy {
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
};

// Wrapper around the |RemotingServiceMock|, solving the lifetime issue
// where this wrapper is owned by the |CrdHostDelegate|, but we want
// to be able to access the |RemotingServiceMock| from our tests.
class RemotingServiceWrapper : public CrdHostDelegate::RemotingServiceProxy {
 public:
  explicit RemotingServiceWrapper(RemotingServiceProxy* implementation)
      : implementation_(*implementation) {}
  RemotingServiceWrapper(const RemotingServiceWrapper&) = delete;
  RemotingServiceWrapper& operator=(const RemotingServiceWrapper&) = delete;
  ~RemotingServiceWrapper() override = default;

  void StartSession(SupportSessionParamsPtr params,
                    const remoting::ChromeOsEnterpriseParams& enterprise_params,
                    StartSessionCallback callback) override {
    implementation_.StartSession(std::move(params), enterprise_params,
                                 std::move(callback));
  }

 private:
  RemotingServiceProxy& implementation_;
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

class CrdHostDelegateTest : public ::testing::Test {
 public:
  CrdHostDelegateTest() = default;
  CrdHostDelegateTest(const CrdHostDelegateTest&) = delete;
  CrdHostDelegateTest& operator=(const CrdHostDelegateTest&) = delete;
  ~CrdHostDelegateTest() override = default;

  RemotingServiceMock& remoting_service() { return remoting_service_; }
  CrdHostDelegate& delegate() { return delegate_; }

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

  // Wait until either the success or error callback is invoked,
  // and return the response.
  Response WaitForResponse() { return result_.Take(); }

  // Call delegate().StartCrdHostAndGetCode() and wait until the
  // |SupportHostObserver| is bound.
  // This observer is used by the CRD host code to inform our delegate of status
  // updates, and is returned by this method so we can spoof these status
  // updates during our tests.
  SupportHostObserver& StartCrdHostAndBindObserver() {
    EXPECT_CALL(remoting_service(), StartSession)
        .WillOnce(
            [&](SupportSessionParamsPtr params,
                const remoting::ChromeOsEnterpriseParams& enterprise_params,
                StartSupportSessionCallback callback) {
              auto response = StartSupportSessionResponse::NewObserver(
                  observer_.BindNewPipeAndPassReceiver());
              std::move(callback).Run(std::move(response));
            });

    delegate().StartCrdHostAndGetCode(SessionParameters{}, success_callback(),
                                      error_callback());

    EXPECT_TRUE(observer_.is_bound()) << "StartSession() was not called";
    return *observer_;
  }

  void FlushForTesting() { observer_.FlushForTesting(); }

 private:
  base::test::SingleThreadTaskEnvironment environment_;

  base::test::TestFuture<Response> result_;
  mojo::Remote<SupportHostObserver> observer_;
  RemotingServiceMock remoting_service_;
  CrdHostDelegate delegate_{
      std::make_unique<RemotingServiceWrapper>(&remoting_service_)};
};

TEST_F(CrdHostDelegateTest, ShouldPassOAuthTokenToRemotingService) {
  SessionParameters parameters;
  parameters.oauth_token = "<the-oauth-token>";

  SupportSessionParamsPtr actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback());

  ASSERT_FALSE(actual_parameters.is_null());
  EXPECT_EQ(actual_parameters->oauth_access_token, "oauth2:<the-oauth-token>");
}

TEST_F(CrdHostDelegateTest, ShouldPassUserNameToRemotingService) {
  SessionParameters parameters;
  parameters.user_name = "<the-user-name>";

  SupportSessionParamsPtr actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback());

  ASSERT_FALSE(actual_parameters.is_null());
  EXPECT_EQ(actual_parameters->user_name, "<the-user-name>");
}

TEST_F(CrdHostDelegateTest,
       ShouldPassShowConfirmationDialogTrueToRemotingService) {
  SessionParameters parameters;
  parameters.show_confirmation_dialog = true;

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback());

  EXPECT_EQ(actual_parameters.suppress_notifications, false);
  EXPECT_EQ(actual_parameters.suppress_user_dialogs, false);
}

TEST_F(CrdHostDelegateTest,
       ShouldPassShowConfirmationDialogFalseToRemotingService) {
  SessionParameters parameters;
  parameters.show_confirmation_dialog = false;

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback());

  EXPECT_EQ(actual_parameters.suppress_notifications, true);
  EXPECT_EQ(actual_parameters.suppress_user_dialogs, true);
}

TEST_F(CrdHostDelegateTest,
       ShouldPassTerminateUponInputFalseToRemotingService) {
  SessionParameters parameters;
  parameters.terminate_upon_input = false;

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback());

  EXPECT_EQ(actual_parameters.terminate_upon_input, false);
}

TEST_F(CrdHostDelegateTest, ShouldPassTerminateUponInputTrueToRemotingService) {
  SessionParameters parameters;
  parameters.terminate_upon_input = true;

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback());

  EXPECT_EQ(actual_parameters.terminate_upon_input, true);
}

TEST_F(CrdHostDelegateTest,
       ShouldPassCurtainLocalUserSessionFalseToRemotingService) {
  SessionParameters parameters;
  parameters.curtain_local_user_session = false;

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback());

  EXPECT_EQ(actual_parameters.curtain_local_user_session, false);
}

TEST_F(CrdHostDelegateTest,
       ShouldPassCurtainLocalUserSessionTrueToRemotingService) {
  SessionParameters parameters;
  parameters.curtain_local_user_session = true;

  remoting::ChromeOsEnterpriseParams actual_parameters;
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce(SaveParamAndInvokeCallback(&actual_parameters));

  delegate().StartCrdHostAndGetCode(parameters, success_callback(),
                                    error_callback());

  EXPECT_EQ(actual_parameters.curtain_local_user_session, true);
}

TEST_F(CrdHostDelegateTest, ShouldReportErrorIfStartSessionReturnsError) {
  EXPECT_CALL(remoting_service(), StartSession)
      .WillOnce([](SupportSessionParamsPtr params,
                   const remoting::ChromeOsEnterpriseParams& enterprise_params,
                   StartSupportSessionCallback callback) {
        auto response = StartSupportSessionResponse::NewSupportSessionError(
            remoting::mojom::StartSupportSessionError::kExistingAdminSession);
        std::move(callback).Run(std::move(response));
      });

  delegate().StartCrdHostAndGetCode(SessionParameters{}, success_callback(),
                                    error_callback());

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ(ResultCode::FAILURE_CRD_HOST_ERROR, response.error_code());
}

TEST_F(CrdHostDelegateTest, ShouldReturnAccessCode) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("the-access-code", base::Days(1));

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasAccessCode());
  EXPECT_EQ("the-access-code", response.access_code());
}

TEST_F(CrdHostDelegateTest,
       ShouldReportErrorWhenHostStateChangesToDisconnected) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateDisconnected("the-disconnect-reason");

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("host disconnected", response.error_message());
  EXPECT_EQ(ResultCode::FAILURE_CRD_HOST_ERROR, response.error_code());
}

TEST_F(CrdHostDelegateTest,
       ShouldReportErrorWhenRemotingServiceReportsPolicyError) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnPolicyError();

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("policy error", response.error_message());
  EXPECT_EQ(ResultCode::FAILURE_CRD_HOST_ERROR, response.error_code());
}

TEST_F(CrdHostDelegateTest,
       ShouldReportErrorWhenRemotingServiceReportsInvalidDomainError) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnInvalidDomainError();

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("invalid domain error", response.error_message());
  EXPECT_EQ(ResultCode::FAILURE_CRD_HOST_ERROR, response.error_code());
}

TEST_F(CrdHostDelegateTest,
       ShouldReportErrorWhenRemotingServiceReportsStateError) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateError(123);

  Response response = WaitForResponse();
  ASSERT_TRUE(response.HasError());
  EXPECT_EQ("host state error", response.error_message());
  EXPECT_EQ(ResultCode::FAILURE_CRD_HOST_ERROR, response.error_code());
}

TEST_F(CrdHostDelegateTest, HasActiveSessionShouldBeTrueWhenASessionIsStarted) {
  EXPECT_FALSE(delegate().HasActiveSession());

  delegate().StartCrdHostAndGetCode(SessionParameters{}, success_callback(),
                                    error_callback());

  EXPECT_TRUE(delegate().HasActiveSession());
}

TEST_F(CrdHostDelegateTest, TerminateSessionShouldTerminateTheActiveSession) {
  delegate().StartCrdHostAndGetCode(SessionParameters{}, success_callback(),
                                    error_callback());
  EXPECT_TRUE(delegate().HasActiveSession());

  base::RunLoop terminate_callback;
  delegate().TerminateSession(terminate_callback.QuitClosure());

  terminate_callback.Run();
  EXPECT_FALSE(delegate().HasActiveSession());
}

TEST_F(CrdHostDelegateTest, ShouldNotCrashIfCrdHostSendsMultipleResponses) {
  SupportHostObserver& observer = StartCrdHostAndBindObserver();

  observer.OnHostStateReceivedAccessCode("access-code", base::Days(1));
  observer.OnHostStateStarting();
  observer.OnHostStateDisconnected(absl::nullopt);
  observer.OnHostStateError(1);
  observer.OnPolicyError();
  observer.OnInvalidDomainError();

  FlushForTesting();
}

}  // namespace policy
