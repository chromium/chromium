// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/crd_host_delegate.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using ::testing::HasSubstr;

std::string FindStringKey(const base::Value& dictionary,
                          const std::string& key) {
  const std::string* result = dictionary.FindStringKey(key);
  if (result)
    return *result;

  return base::StringPrintf("Key '%s' not found", key.c_str());
}

#define EXPECT_STRING_KEY(dictionary, key, value)  \
  EXPECT_EQ(FindStringKey(dictionary, key), value) \
      << "Wrong value for key '" << key << "'";

#define EXPECT_BOOL_KEY(dictionary, key, value)                          \
  absl::optional<bool> value_maybe = dictionary.FindBoolKey(key);        \
  EXPECT_TRUE(value_maybe.has_value()) << "Missing key '" << key << "'"; \
  EXPECT_EQ(value_maybe.value_or(false), value)                          \
      << "Wrong value for key '" << key << "'";

#define EXPECT_TYPE(dictionary, value) \
  EXPECT_STRING_KEY(dictionary, remoting::kMessageType, value)

// Builder class that constructs a message to send to the native host.
class Message {
 public:
  Message() = default;
  Message(const Message&) = delete;
  Message& operator=(const Message&) = delete;
  ~Message() = default;

  Message& WithType(const std::string& type) {
    return AddString(remoting::kMessageType, type);
  }

  Message& WithState(const std::string& state) {
    return AddString(remoting::kState, state);
  }

  Message& AddString(const std::string& key, const std::string& value) {
    result.SetStringKey(key, value);
    return *this;
  }
  Message& AddInt(const std::string& key, int value) {
    result.SetIntKey(key, value);
    return *this;
  }

  base::Value Build() { return std::move(result); }

 private:
  base::Value result{base::Value::Type::DICTIONARY};
};

// Representation of a value that will be populated asynchronously.
// Provides accessors to wait for the value to arrive.
template <typename Type>
class FutureValue {
 public:
  FutureValue() = default;
  FutureValue(const FutureValue&) = delete;
  FutureValue& operator=(const FutureValue&) = delete;
  ~FutureValue() = default;

  // Wait for the value to arrive, and return the result.
  // Will time out if no value arrives.
  Type& GetWithTimeout(
      const std::string& error_message = "Timeout waiting for value") {
    WaitForValueWithTimeout(error_message);
    return value_.value();
  }

  // Wait for the value to arrive.
  // Will time out if no value arrives.
  void WaitForValueWithTimeout(const std::string& error_message = "Timeout") {
    if (!value_) {
      base::test::ScopedRunLoopTimeout timeout(
          FROM_HERE, base::TimeDelta::FromSeconds(5),
          base::BindLambdaForTesting(
              [error_message]() { return error_message; }));

      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }

  void SetValue(Type value) {
    value_ = std::move(value);
    if (run_loop_)
      run_loop_->Quit();
  }

  // Get the current value, or DCHECK if no value is set.
  Type& value() {
    DCHECK(has_value());
    return value_.value();
  }
  const Type& value() const {
    DCHECK(has_value());
    return value_.value();
  }

  bool has_value() const { return value_.has_value(); }

  // Unset the current value, so this object can be used again for a new value.
  void Reset() {
    value_.reset();
    run_loop_.reset();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  absl::optional<Type> value_;
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
    EXPECT_FALSE(last_message_.has_value())
        << "Test finishes without handling a message: "
        << last_message_.value();
  }

  // extensions::NativeMessageHost implementation:
  void OnMessage(const std::string& message) override {
    EXPECT_FALSE(last_message_.has_value())
        << "Unhandled message: " << last_message_.value();

    last_message_.SetValue(message);
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

    is_started_.WaitForValueWithTimeout(
        "Timeout waiting for NativeMessageHost::Start");
  }

  void WaitForHello() { WaitForMessageOfType("hello"); }

  // Wait until a message is received, checks the type and returns the message.
  base::Value WaitForMessageOfType(const std::string& type) {
    std::string message_str = last_message_.GetWithTimeout(base::StringPrintf(
        "Timeout waiting for message of type '%s'", type.c_str()));

    // Prepare the future value for our next message.
    last_message_.Reset();

    absl::optional<base::Value> message = base::JSONReader::Read(message_str);
    if (!message) {
      ADD_FAILURE() << "Malformed JSON message: " << message_str;
      base::Value dummy_message(base::Value::Type::DICTIONARY);
      return dummy_message;
    }

    EXPECT_TYPE(message.value(), type);
    return std::move(message.value());
  }

  bool has_message() const { return last_message_.has_value(); }

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
  FutureValue<bool> is_started_;
  FutureValue<std::string> last_message_;
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

}  // namespace

class CRDHostDelegateTest : public ash::DeviceSettingsTestBase {
 public:
  CRDHostDelegateTest() = default;
  CRDHostDelegateTest(const CRDHostDelegateTest&) = delete;
  CRDHostDelegateTest& operator=(const CRDHostDelegateTest&) = delete;
  ~CRDHostDelegateTest() override = default;

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    // SystemSaltGetter is used by the token service.
    chromeos::SystemSaltGetter::Initialize();
    DeviceOAuth2TokenServiceFactory::Initialize(
        test_url_loader_factory_.GetSafeWeakWrapper(), &local_state_);
    RegisterLocalState(local_state_.registry());

    // We can only create the delegate after the
    // OAuth2TokenServiceFactory has been set up.
    delegate_ = std::make_unique<CRDHostDelegate>(
        std::make_unique<NativeMessageHostFactoryStub>(&host_));
  }

  void TearDown() override {
    DeviceOAuth2TokenServiceFactory::Shutdown();
    chromeos::SystemSaltGetter::Shutdown();

    DeviceSettingsTestBase::TearDown();
  }

  void StartCRDHostAndGetCode(const std::string& auth_token = "auth-token",
                              bool terminate_upon_input = false) {
    delegate().StartCRDHostAndGetCode(auth_token, terminate_upon_input,
                                      response_.GetSuccessCallback(),
                                      response_.GetErrorCallback());
  }

  // Helper object representing the response, which is either the access code
  // or an error message.
  Response& response() { return response_; }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  CRDHostDelegate& delegate() { return *delegate_; }
  NativeMessageHostStub& host() { return host_; }

 private:
  NativeMessageHostStub host_;
  std::unique_ptr<CRDHostDelegate> delegate_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple local_state_;

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
  StartCRDHostAndGetCode(/*auth_token=*/"the-auth-token",
                         /*terminate_upon_input=*/true);
  host().WaitForHello();
  host().PostMessageOfType("helloResponse");

  base::Value response = host().WaitForMessageOfType(remoting::kConnectMessage);
  EXPECT_STRING_KEY(response, remoting::kAuthServiceWithToken,
                    "oauth2:the-auth-token");
  EXPECT_BOOL_KEY(response, remoting::kTerminateUponInput, true);
}

TEST_F(CRDHostDelegateTest, ShouldSendAccessCodeToCallback) {
  StartCRDHostAndGetCode();
  host().HandleHandshake();

  host().PostMessage(Message()
                         .WithType(remoting::kHostStateChangedMessage)
                         .WithState(remoting::kHostStateReceivedAccessCode)
                         .AddString(remoting::kAccessCode, "<the-access-code>")
                         .AddInt(remoting::kAccessCodeLifetime, 123));
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
          .AddString(remoting::kAccessCode, "<the-first-access-code>")
          .AddInt(remoting::kAccessCodeLifetime, 123));
  RunUntilIdle();

  EXPECT_EQ(response().access_code(), "<the-first-access-code>");

  host().PostMessage(
      Message()
          .WithType(remoting::kHostStateChangedMessage)
          .WithState(remoting::kHostStateReceivedAccessCode)
          .AddString(remoting::kAccessCode, "<the-second-access-code>")
          .AddInt(remoting::kAccessCodeLifetime, 123));

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

  host().PostMessage(Message()
                         .WithType(remoting::kHostStateChangedMessage)
                         .WithState(remoting::kHostStateDisconnected));

  host().WaitForMessageOfType(remoting::kDisconnectMessage);
}

TEST_F(CRDHostDelegateTest, ShouldIgnoreRemoveDisconnectBeforeRemoteConnect) {
  StartCRDHostAndGetCode();
  host().HandleHandshake();

  host().PostMessage(Message()
                         .WithType(remoting::kHostStateChangedMessage)
                         .WithState(remoting::kHostStateDisconnected));
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
                         .AddString(remoting::kAccessCode, "<the-access-code>")
                         .AddInt(remoting::kAccessCodeLifetime, 123));

  host().PostMessage(Message().WithType(remoting::kDisconnectResponse));
  RunUntilIdle();

  EXPECT_TRUE(host().is_destroyed());
}

TEST_F(CRDHostDelegateTest, ShouldDestroyHostOnStateError) {
  StartCRDHostAndGetCode();
  host().WaitForHello();

  host().PostMessage(
      Message()
          .WithType(remoting::kHostStateChangedMessage)
          .WithState(remoting::kHostStateError)
          .AddString(remoting::kErrorMessageCode, "<the-error-code>"));
  RunUntilIdle();

  EXPECT_THAT(response().error_message(),
              HasSubstr("CRD State Error: <the-error-code>"));

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
              HasSubstr("CRD State Error: Invalid domain"));

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

}  // namespace policy
