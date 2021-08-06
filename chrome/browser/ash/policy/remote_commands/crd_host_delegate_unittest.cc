// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_host_delegate.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/policy/remote_commands/crd_connection_observer.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using ::testing::HasSubstr;
using SessionParameters = CRDHostDelegate::SessionParameters;
using base::test::TestFuture;

std::string FindStringKey(const base::Value& dictionary,
                          const std::string& key) {
  const std::string* result = dictionary.FindStringKey(key);
  if (result)
    return *result;

  return base::StringPrintf("Key '%s' not found", key.c_str());
}

#define EXPECT_STRING_KEY(dictionary, key, value)    \
  ({                                                 \
    EXPECT_EQ(FindStringKey(dictionary, key), value) \
        << "Wrong value for key '" << key << "'";    \
  })

#define EXPECT_BOOL_KEY(dictionary, key, value)                            \
  ({                                                                       \
    absl::optional<bool> value_maybe = dictionary.FindBoolKey(key);        \
    EXPECT_TRUE(value_maybe.has_value()) << "Missing key '" << key << "'"; \
    EXPECT_EQ(value_maybe.value_or(false), value)                          \
        << "Wrong value for key '" << key << "'";                          \
  })

#define EXPECT_TYPE(dictionary, value) \
  EXPECT_STRING_KEY(dictionary, remoting::kMessageType, value)

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0);

// Builder class that constructs a message to send to the native host.
class Message {
 public:
  Message() = default;
  Message(const Message&) = delete;
  Message& operator=(const Message&) = delete;
  ~Message() = default;

  Message& WithType(const std::string& type) {
    return WithString(remoting::kMessageType, type);
  }

  Message& WithState(const std::string& state) {
    return WithString(remoting::kState, state);
  }

  Message& WithString(const std::string& key, const std::string& value) {
    result.SetStringKey(key, value);
    return *this;
  }
  Message& WithInt(const std::string& key, int value) {
    result.SetIntKey(key, value);
    return *this;
  }

  base::Value Build() { return std::move(result); }

 private:
  base::Value result{base::Value::Type::DICTIONARY};
};

// Stub implementation of the |NativeMessageHost| which allows the test to wait
// for messages to the host and to send replies to the client.
// The implementation is strict, meaning the test will fail if the client sends
// a message that the test does not handle through a |WaitFor...| call.
class NativeMessageHostStub : public extensions::NativeMessageHost {
 public:
  NativeMessageHostStub() = default;
  NativeMessageHostStub(const NativeMessageHostStub&) = delete;
  NativeMessageHostStub& operator=(const NativeMessageHostStub&) = delete;
  ~NativeMessageHostStub() override {
    EXPECT_FALSE(last_message_->IsReady())
        << "Test finishes without handling a message: " << last_message_->Get();
  }

  // extensions::NativeMessageHost implementation:
  void OnMessage(const std::string& message) override {
    EXPECT_FALSE(last_message_->IsReady())
        << "Unhandled message: " << last_message_->Get();

    last_message_->SetValue(message);
  }

  void Start(Client* client) override {
    client_ = client;
    is_started_.SetValue(true);
  }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override {
    return base::ThreadTaskRunnerHandle::Get();
  }

  void WaitForStart() {
    if (client_)
      return;  // Start has already been called

    EXPECT_TRUE(is_started_.Wait()) << "Timeout waiting for start.";
  }

  void WaitForHello() { WaitForMessageOfType("hello"); }

  // Wait until a message is received, checks the type and returns the message.
  base::Value WaitForMessageOfType(const std::string& type) {
    EXPECT_TRUE(last_message_->Wait())
        << "Timeout waiting for message of type '" << type.c_str() << "'";

    std::string message_str = last_message_->Get();

    // Prepare for our next message.
    last_message_ = std::make_unique<TestFuture<std::string>>();

    absl::optional<base::Value> message = base::JSONReader::Read(message_str);
    if (!message) {
      ADD_FAILURE() << "Malformed JSON message: " << message_str;
      base::Value dummy_message(base::Value::Type::DICTIONARY);
      return dummy_message;
    }

    EXPECT_TYPE(message.value(), type);
    return std::move(message.value());
  }

  bool has_message() const { return last_message_->IsReady(); }

  void PostMessageOfType(const std::string& type) {
    PostMessage(Message().WithType(type));
  }

  void PostMessage(Message& builder) { PostMessage(builder.Build()); }

  void PostMessage(const base::Value& message) {
    std::string message_string;
    base::JSONWriter::Write(message, &message_string);
    client().PostMessageFromNativeHost(message_string);
  }

  void HandleHandshake() {
    WaitForMessageOfType(remoting::kHelloMessage);
    PostMessageOfType(remoting::kHelloResponse);
    WaitForMessageOfType(remoting::kConnectMessage);
    PostMessageOfType(remoting::kConnectResponse);
  }

  // Return the client passed to Start()
  Client& client() {
    DCHECK(client_);
    return *client_;
  }

  // True if the host was destroyed. Note that this only destroys the
  // wrapper, |this| remains valid (so it is safe to call this method).
  bool is_destroyed() const { return is_destroyed_; }
  void MarkAsDestroyed() { is_destroyed_ = true; }

 private:
  Client* client_ = nullptr;
  TestFuture<bool> is_started_;

  // Waiter for the next message from the CRD Host.
  // Will never be null.
  std::unique_ptr<TestFuture<std::string>> last_message_ =
      std::make_unique<TestFuture<std::string>>();

  bool is_destroyed_ = false;
};

class NativeMessageHostWrapper : public extensions::NativeMessageHost {
 public:
  explicit NativeMessageHostWrapper(NativeMessageHostStub* impl)
      : impl_(*impl) {}
  NativeMessageHostWrapper(const NativeMessageHostWrapper&) = delete;
  NativeMessageHostWrapper& operator=(const NativeMessageHostWrapper&) = delete;
  ~NativeMessageHostWrapper() override { impl_.MarkAsDestroyed(); }

  // extensionsNativeMessageHost implementation:
  void OnMessage(const std::string& message) override {
    impl_.OnMessage(message);
  }
  void Start(Client* client) override { impl_.Start(client); }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override {
    return impl_.task_runner();
  }

 private:
  NativeMessageHostStub& impl_;
};

class NativeMessageHostFactoryStub
    : public CRDHostDelegate::NativeMessageHostFactory {
 public:
  explicit NativeMessageHostFactoryStub(NativeMessageHostStub* host)
      : host_(*host) {}
  NativeMessageHostFactoryStub(const NativeMessageHostFactoryStub&) = delete;
  NativeMessageHostFactoryStub& operator=(const NativeMessageHostFactoryStub&) =
      delete;
  ~NativeMessageHostFactoryStub() override = default;

  // CRDHostDelegate::NativeMessageHostFactory implementation:
  std::unique_ptr<extensions::NativeMessageHost> CreateNativeMessageHostHost()
      override {
    return std::make_unique<NativeMessageHostWrapper>(&host_);
  }

 private:
  NativeMessageHostStub& host_;
};

// Represents the response to the CRD host request, which is either an
// access code or an error message.
class Response {
 public:
  Response() = default;
  Response(const Response&) = delete;
  Response& operator=(const Response&) = delete;
  ~Response() = default;

  bool HasAccessCode() const { return access_code_.has_value(); }
  bool HasError() const { return error_message_.has_value(); }

  std::string error_message() const {
    EXPECT_FALSE(HasAccessCode());
    EXPECT_TRUE(HasError());
    return error_message_.value_or("<no error received>");
  }

  std::string access_code() const {
    EXPECT_TRUE(HasAccessCode());
    EXPECT_FALSE(HasError());
    return access_code_.value_or("<no access code received>");
  }

  DeviceCommandStartCRDSessionJob::AccessCodeCallback GetSuccessCallback() {
    return base::BindOnce(&Response::OnSuccess, weak_factory_.GetWeakPtr());
  }

  DeviceCommandStartCRDSessionJob::ErrorCallback GetErrorCallback() {
    return base::BindOnce(&Response::OnError, weak_factory_.GetWeakPtr());
  }

 private:
  void OnSuccess(const std::string& access_code) {
    EXPECT_FALSE(HasResponse());
    access_code_ = access_code;
    run_loop_.Quit();
  }

  void OnError(DeviceCommandStartCRDSessionJob::ResultCode error_code,
               const std::string& error_message) {
    EXPECT_FALSE(HasResponse());
    error_message_ = error_message;
    error_code_ = error_code;
    run_loop_.Quit();
  }

  bool HasResponse() const { return HasAccessCode() || HasError(); }

  absl::optional<std::string> access_code_;
  absl::optional<DeviceCommandStartCRDSessionJob::ResultCode> error_code_;
  absl::optional<std::string> error_message_;

  base::RunLoop run_loop_;
  base::WeakPtrFactory<Response> weak_factory_{this};
};

class ConnectionObserverMock : public CrdConnectionObserver {
 public:
  ConnectionObserverMock() = default;
  ConnectionObserverMock(const ConnectionObserverMock&) = delete;
  ConnectionObserverMock& operator=(const ConnectionObserverMock&) = delete;
  ~ConnectionObserverMock() override = default;

  // CRDConnectionObserver  implementation:
  MOCK_METHOD(void, OnConnectionRejected, ());
  MOCK_METHOD(void, OnConnectionEstablished, ());
};

}  // namespace

class CRDHostDelegateTest : public ::testing::Test {
 public:
  CRDHostDelegateTest() = default;
  CRDHostDelegateTest(const CRDHostDelegateTest&) = delete;
  CRDHostDelegateTest& operator=(const CRDHostDelegateTest&) = delete;
  ~CRDHostDelegateTest() override = default;

  void StartCRDHostAndGetCode(
      const SessionParameters& parameters = SessionParameters()) {
    delegate().StartCRDHostAndGetCode(parameters,
                                      response_.GetSuccessCallback(),
                                      response_.GetErrorCallback());
  }

  // Helper object representing the response, which is either the access code
  // or an error message.
  Response& response() { return response_; }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  CRDHostDelegate& delegate() { return delegate_; }
  NativeMessageHostStub& host() { return host_; }

  ConnectionObserverMock& InstallConnectionObserverMock() {
    delegate_.AddConnectionObserver(&connection_observer_);
    return connection_observer_;
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;

  NativeMessageHostStub host_;
  ::testing::StrictMock<ConnectionObserverMock> connection_observer_;
  CRDHostDelegate delegate_{
      std::make_unique<NativeMessageHostFactoryStub>(&host_)};

  Response response_;
};

TEST_F(CRDHostDelegateTest, ShouldStartNativeMessageHostAndSendHello) {
  StartCRDHostAndGetCode();
  host().WaitForStart();
  host().WaitForMessageOfType(remoting::kHelloMessage);
}

TEST_F(CRDHostDelegateTest, ShouldErrorOutIfNativeHostSendsInvalidResponse) {
  StartCRDHostAndGetCode();
  host().WaitForHello();

  host().client().PostMessageFromNativeHost("invalid message");
  RunUntilIdle();

  EXPECT_THAT(response().error_message(), HasSubstr("invalid JSON"));
}

TEST_F(CRDHostDelegateTest, ShouldDestroyHostOnError) {
  StartCRDHostAndGetCode();
  host().WaitForHello();

  host().client().PostMessageFromNativeHost("invalid message");
  RunUntilIdle();

  EXPECT_TRUE(host().is_destroyed());
}

TEST_F(CRDHostDelegateTest,
       ShouldErrorOutIfNativeHostResponseIsNotADictionary) {
  StartCRDHostAndGetCode();
  host().WaitForHello();

  host().client().PostMessageFromNativeHost(R"([ "valid json but an array" ])");
  RunUntilIdle();

  EXPECT_THAT(response().error_message(), HasSubstr("not a dictionary"));
}

TEST_F(CRDHostDelegateTest, ShouldErrorOutIfNativeHostResponseHasNoType) {
  StartCRDHostAndGetCode();
  host().WaitForHello();

  host().client().PostMessageFromNativeHost(R"({ "key": "value" })");
  RunUntilIdle();

  EXPECT_THAT(response().error_message(), HasSubstr("without type"));
}

TEST_F(CRDHostDelegateTest, ShouldSendConnectMessageOnHelloResponse) {
  StartCRDHostAndGetCode();
  host().WaitForHello();
  host().PostMessageOfType("helloResponse");

  host().WaitForMessageOfType(remoting::kConnectMessage);
}

TEST_F(CRDHostDelegateTest, ShouldSendAuthTokenInConnectMessage) {
  SessionParameters parameters;
  parameters.oauth_token = "the-oauth-token";
  StartCRDHostAndGetCode(parameters);

  host().WaitForHello();
  host().PostMessageOfType("helloResponse");

  base::Value response = host().WaitForMessageOfType(remoting::kConnectMessage);
  EXPECT_STRING_KEY(response, remoting::kAuthServiceWithToken,
                    "oauth2:the-oauth-token");
}

TEST_F(CRDHostDelegateTest, ShouldSendUserNameInConnectMessage) {
  SessionParameters parameters;
  parameters.user_name = "the-user-name";
  StartCRDHostAndGetCode(parameters);

  host().WaitForHello();
  host().PostMessageOfType("helloResponse");

  base::Value response = host().WaitForMessageOfType(remoting::kConnectMessage);
  EXPECT_STRING_KEY(response, remoting::kUserName, "the-user-name");
}

TEST_F(CRDHostDelegateTest, ShouldSendTerminateUponInputTrueInConnectMessage) {
  SessionParameters parameters;
  parameters.terminate_upon_input = true;
  StartCRDHostAndGetCode(parameters);

  host().WaitForHello();
  host().PostMessageOfType("helloResponse");

  base::Value response = host().WaitForMessageOfType(remoting::kConnectMessage);
  EXPECT_BOOL_KEY(response, remoting::kTerminateUponInput, true);
}

TEST_F(CRDHostDelegateTest, ShouldSendTerminateUponInputFalseInConnectMessage) {
  SessionParameters parameters;
  parameters.terminate_upon_input = false;
  StartCRDHostAndGetCode(parameters);

  host().WaitForHello();
  host().PostMessageOfType("helloResponse");

  base::Value response = host().WaitForMessageOfType(remoting::kConnectMessage);
  EXPECT_BOOL_KEY(response, remoting::kTerminateUponInput, false);
}

TEST_F(CRDHostDelegateTest,
       ShouldSendShowConfirmationDialogTrueInConnectMessage) {
  SessionParameters parameters;
  parameters.show_confirmation_dialog = true;
  StartCRDHostAndGetCode(parameters);

  host().WaitForHello();
  host().PostMessageOfType("helloResponse");

  base::Value response = host().WaitForMessageOfType(remoting::kConnectMessage);
  EXPECT_BOOL_KEY(response, remoting::kSuppressNotifications, false);
  EXPECT_BOOL_KEY(response, remoting::kSuppressUserDialogs, false);
}

TEST_F(CRDHostDelegateTest,
       ShouldSendShowConfirmationDialogFalseInConnectMessage) {
  SessionParameters parameters;
  parameters.show_confirmation_dialog = false;
  StartCRDHostAndGetCode(parameters);

  host().WaitForHello();
  host().PostMessageOfType("helloResponse");

  base::Value response = host().WaitForMessageOfType(remoting::kConnectMessage);
  EXPECT_BOOL_KEY(response, remoting::kSuppressNotifications, true);
  EXPECT_BOOL_KEY(response, remoting::kSuppressUserDialogs, true);
}

TEST_F(CRDHostDelegateTest, ShouldSetIsEnterpriseAdminUser) {
  StartCRDHostAndGetCode();

  host().WaitForHello();
  host().PostMessageOfType("helloResponse");

  base::Value response = host().WaitForMessageOfType(remoting::kConnectMessage);
  EXPECT_BOOL_KEY(response, remoting::kIsEnterpriseAdminUser, true);
}

TEST_F(CRDHostDelegateTest, ShouldSendAccessCodeToCallback) {
  StartCRDHostAndGetCode();
  host().HandleHandshake();

  host().PostMessage(Message()
                         .WithType(remoting::kHostStateChangedMessage)
                         .WithState(remoting::kHostStateReceivedAccessCode)
                         .WithString(remoting::kAccessCode, "<the-access-code>")
                         .WithInt(remoting::kAccessCodeLifetime, 123));
  RunUntilIdle();

  EXPECT_EQ(response().access_code(), "<the-access-code>");
}

TEST_F(CRDHostDelegateTest, ShouldDisconnectTheHostIfASecondAccessCodeArrives) {
  StartCRDHostAndGetCode();
  host().HandleHandshake();

  host().PostMessage(
      Message()
          .WithType(remoting::kHostStateChangedMessage)
          .WithState(remoting::kHostStateReceivedAccessCode)
          .WithString(remoting::kAccessCode, "<the-first-access-code>")
          .WithInt(remoting::kAccessCodeLifetime, 123));
  RunUntilIdle();

  EXPECT_EQ(response().access_code(), "<the-first-access-code>");

  host().PostMessage(
      Message()
          .WithType(remoting::kHostStateChangedMessage)
          .WithState(remoting::kHostStateReceivedAccessCode)
          .WithString(remoting::kAccessCode, "<the-second-access-code>")
          .WithInt(remoting::kAccessCodeLifetime, 123));

  host().WaitForMessageOfType(remoting::kDisconnectMessage);
}

TEST_F(CRDHostDelegateTest, ShouldErrorOutIfStateChangeHasNoStateField) {
  StartCRDHostAndGetCode();
  host().HandleHandshake();

  host().PostMessage(Message().WithType(remoting::kHostStateChangedMessage));
  RunUntilIdle();

  EXPECT_THAT(response().error_message(), HasSubstr("No state"));
}

TEST_F(CRDHostDelegateTest, ShouldDisconnectTheHostIfRemoteDisconnects) {
  StartCRDHostAndGetCode();
  host().HandleHandshake();
  host().PostMessage(Message()
                         .WithType(remoting::kHostStateChangedMessage)
                         .WithState(remoting::kHostStateConnected));

  host().PostMessage(
      Message()
          .WithType(remoting::kHostStateChangedMessage)
          .WithState(remoting::kHostStateDisconnected)
          .WithString(remoting::kDisconnectReason, "<the-disconnect-reason>"));

  host().WaitForMessageOfType(remoting::kDisconnectMessage);
}

TEST_F(CRDHostDelegateTest, ShouldIgnoreRemoveDisconnectBeforeRemoteConnect) {
  StartCRDHostAndGetCode();
  host().HandleHandshake();

  host().PostMessage(
      Message()
          .WithType(remoting::kHostStateChangedMessage)
          .WithState(remoting::kHostStateDisconnected)
          .WithString(remoting::kDisconnectReason, "<the-disconnect-reason>"));
  RunUntilIdle();

  // The disconnect should be ignored and the host should just keep running.
  EXPECT_FALSE(host().has_message());
  EXPECT_FALSE(host().is_destroyed());
}

TEST_F(CRDHostDelegateTest, ShouldDestroyHostIfHostDisconnects) {
  StartCRDHostAndGetCode();
  host().HandleHandshake();
  // First send the access code, as the disconnect message is only expected
  // after receiving the access code.
  host().PostMessage(Message()
                         .WithType(remoting::kHostStateChangedMessage)
                         .WithState(remoting::kHostStateReceivedAccessCode)
                         .WithString(remoting::kAccessCode, "<the-access-code>")
                         .WithInt(remoting::kAccessCodeLifetime, 123));

  host().PostMessage(
      Message()
          .WithType(remoting::kDisconnectResponse)
          .WithString(remoting::kDisconnectReason, "<the-disconnect-reason>"));
  RunUntilIdle();

  EXPECT_TRUE(host().is_destroyed());
}

TEST_F(CRDHostDelegateTest, ShouldDestroyHostOnStateError) {
  StartCRDHostAndGetCode();
  host().WaitForHello();

  host().PostMessage(
      Message()
          .WithType(remoting::kErrorMessage)
          .WithString(remoting::kErrorMessageCode, "<the-error-code>"));
  RunUntilIdle();

  EXPECT_THAT(response().error_message(),
              HasSubstr("CRD Connection Error: <the-error-code>"));

  EXPECT_TRUE(host().is_destroyed());
}

TEST_F(CRDHostDelegateTest, ShouldDestroyHostOnStateDomainError) {
  StartCRDHostAndGetCode();
  host().WaitForHello();

  host().PostMessage(Message()
                         .WithType(remoting::kHostStateChangedMessage)
                         .WithState(remoting::kHostStateDomainError));
  RunUntilIdle();

  EXPECT_THAT(response().error_message(),
              HasSubstr("CRD Connection Error: INVALID_DOMAIN_ERROR"));

  EXPECT_TRUE(host().is_destroyed());
}

TEST_F(CRDHostDelegateTest, ShouldIgnoreOtherStateValues) {
  StartCRDHostAndGetCode();
  host().WaitForHello();

  std::vector<std::string> ignored_states{
      remoting::kHostStateStarting, remoting::kHostStateRequestedAccessCode,
      remoting::kHostStateConnecting, "<unknown-state>"};

  for (const std::string& state : ignored_states) {
    host().PostMessage(Message()
                           .WithType(remoting::kHostStateChangedMessage)
                           .WithState(state));
    RunUntilIdle();
    EXPECT_FALSE(host().has_message())
        << "Unexpected response to state " << state;
    EXPECT_FALSE(host().is_destroyed())
        << "Unexpected shutdown due to state " << state;
  }
}

TEST_F(CRDHostDelegateTest, ShouldReportSuccessfullAttemptsToLockoutStrategy) {
  ConnectionObserverMock& connection_observer = InstallConnectionObserverMock();

  StartCRDHostAndGetCode();
  host().HandleHandshake();

  EXPECT_CALL(connection_observer, OnConnectionEstablished);

  host().PostMessage(Message()
                         .WithType(remoting::kHostStateChangedMessage)
                         .WithState(remoting::kHostStateConnected));
  RunUntilIdle();
}

TEST_F(CRDHostDelegateTest, ShouldReportRejectedAttemptsToLockoutStrategy) {
  ConnectionObserverMock& connection_observer = InstallConnectionObserverMock();

  StartCRDHostAndGetCode();
  host().HandleHandshake();

  EXPECT_CALL(connection_observer, OnConnectionRejected());

  host().PostMessage(
      Message()
          .WithType(remoting::kHostStateChangedMessage)
          .WithState(remoting::kHostStateDisconnected)
          .WithString(remoting::kDisconnectReason, "SESSION_REJECTED"));
  RunUntilIdle();
}

TEST_F(CRDHostDelegateTest,
       ShouldNotReportOtherConnectionFailedReasonsToLockoutStrategy) {
  ConnectionObserverMock& connection_observer = InstallConnectionObserverMock();

  StartCRDHostAndGetCode();
  host().HandleHandshake();

  EXPECT_NO_CALLS(connection_observer, OnConnectionRejected());

  host().PostMessage(
      Message()
          .WithType(remoting::kHostStateChangedMessage)
          .WithState(remoting::kHostStateDisconnected)
          .WithString(remoting::kDisconnectReason, "other disconnect reason"));
  RunUntilIdle();
}

}  // namespace policy
