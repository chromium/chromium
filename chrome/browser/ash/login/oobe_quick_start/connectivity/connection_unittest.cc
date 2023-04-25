// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "connection.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_quick_start_decoder.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/wifi_credentials.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_requests.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "content/public/test/browser_task_environment.h"
#include "fido_authentication_message_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash::quick_start {

using message_helper::BuildEncodedResponseData;

namespace {
constexpr char kTestMessagePayloadKey[] = "bootstrapOptions";
constexpr char kTestMessagePayloadValue[] = "testValue";

const char kAccountRequirementKey[] = "accountRequirement";
const char kFlowTypeKey[] = "flowType";
const int kAccountRequirementSingle = 2;
const int kFlowTypeTargetChallenge = 2;

const char kChallengeBase64Url[] = "testchallenge";

const std::vector<uint8_t> kTestBytes = {0x00, 0x01, 0x02};
const std::vector<uint8_t> kExpectedGetInfoRequest = {0x04};

const char kNotifySourceOfUpdateMessageKey[] = "forced_update_required";
constexpr uint8_t kSuccess = 0x00;
constexpr char kAuthToken[] = "auth_token";

// 32 random bytes to use as the shared secret.
constexpr std::array<uint8_t, 32> kSharedSecret = {
    0x54, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

// 32 random bytes to use as the secondary shared secret.
constexpr std::array<uint8_t, 32> kSecondarySharedSecret = {
    0x50, 0xfe, 0xd7, 0x14, 0x1c, 0x93, 0xf6, 0x92, 0xaf, 0x7b, 0x4d,
    0xab, 0xa0, 0xe3, 0xfc, 0xd3, 0x5a, 0x04, 0x01, 0x63, 0xf6, 0xf5,
    0xeb, 0x40, 0x7f, 0x4b, 0xac, 0xe4, 0xd1, 0xbf, 0x20, 0x19};

// 12 random bytes to use as the nonce.
constexpr std::array<uint8_t, 12> kNonce = {0x60, 0x3e, 0x87, 0x69, 0xa3, 0x55,
                                            0xd3, 0x49, 0xbd, 0x0a, 0x63, 0xed};

// A serialized auth message produced with |kAuthToken|, |kSharedSecret|, and
// |kNonce|. Uses the "Target" role.
constexpr std::array<uint8_t, 50> kTargetDeviceAuthMessage = {
    0x08, 0x01, 0x12, 0x2e, 0x0a, 0x0c, 0x60, 0x3e, 0x87, 0x69,
    0xa3, 0x55, 0xd3, 0x49, 0xbd, 0x0a, 0x63, 0xed, 0x12, 0x1e,
    0x44, 0x28, 0x93, 0x04, 0xd3, 0xc0, 0x03, 0x50, 0xc9, 0x9d,
    0x4f, 0x8d, 0x01, 0xaa, 0xcf, 0xc6, 0x43, 0x41, 0xa2, 0xcf,
    0x4a, 0x91, 0x6e, 0x49, 0x14, 0x9d, 0x2e, 0xea, 0x9a, 0xf6};

// 6 random bytes to use as the RandomSessionId.
constexpr std::array<uint8_t, 6> kRandomSessionId = {0x6b, 0xb3, 0x85,
                                                     0x27, 0xbb, 0x28};

class ConstantNonceGenerator : public Connection::NonceGenerator {
  Connection::Nonce Generate() override { return kNonce; }
};

}  // namespace

class ConnectionTest : public testing::Test {
 public:
  ConnectionTest() = default;
  ConnectionTest(ConnectionTest&) = delete;
  ConnectionTest& operator=(ConnectionTest&) = delete;
  ~ConnectionTest() override = default;

  void SetUp() override {
    // Since this test doesn't run in a sandbox, disable the sandbox checks
    // on QuickStartMessage. Without this, the Message class will fail.
    QuickStartMessage::DisableSandboxCheckForTesting();

    fake_nearby_connection_ = std::make_unique<FakeNearbyConnection>();
    NearbyConnection* nearby_connection = fake_nearby_connection_.get();
    fake_quick_start_decoder_ = std::make_unique<FakeQuickStartDecoder>();
    connection_ = std::make_unique<Connection>(
        nearby_connection, session_id_, kSharedSecret, kSecondarySharedSecret,
        std::make_unique<ConstantNonceGenerator>(),
        /*on_connection_closed=*/base::DoNothing(),
        /*on_connection_authenticated=*/
        base::BindLambdaForTesting(
            [&](base::WeakPtr<
                TargetDeviceConnectionBroker::AuthenticatedConnection>
                    authenticated_connection) {
              ran_connection_authenticated_callback_ = true;
              authenticated_connection_ = authenticated_connection;
            }));
    connection_->decoder_ =
        mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>(
            fake_quick_start_decoder_->GetRemote());
  }

  void MarkConnectionAuthenticated() {
    ASSERT_FALSE(ran_connection_authenticated_callback_);
    connection_->MarkConnectionAuthenticated();
    ASSERT_TRUE(ran_connection_authenticated_callback_);
    ASSERT_TRUE(authenticated_connection_);
  }

  void VerifyAssertionInfo(absl::optional<FidoAssertionInfo> assertion_info) {
    ran_assertion_response_callback_ = true;
    assertion_info_ = assertion_info;
  }

  void SendPayloadAndReadResponse(
      const base::Value::Dict& message_payload,
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)> callback) {
    connection_->SendPayloadAndReadResponse(message_payload,
                                            std::move(callback));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
  std::unique_ptr<Connection> connection_;
  RandomSessionId session_id_ = RandomSessionId(kRandomSessionId);
  bool ran_assertion_response_callback_ = false;
  bool ran_connection_authenticated_callback_ = false;
  base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>
      authenticated_connection_;
  std::unique_ptr<FakeQuickStartDecoder> fake_quick_start_decoder_;
  absl::optional<FidoAssertionInfo> assertion_info_;
};

TEST_F(ConnectionTest, RequestWifiCredentials) {
  MarkConnectionAuthenticated();
  // Arbitrary Session ID for testing
  int32_t session_id = 1;

  fake_quick_start_decoder_->SetWifiCredentialsResponse(
      mojom::GetWifiCredentialsResponse::NewCredentials(
          mojom::WifiCredentials::New("ssid", mojom::WifiSecurityType::kPSK,
                                      true, "password")));

  base::test::TestFuture<absl::optional<WifiCredentials>> future;

  authenticated_connection_->RequestWifiCredentials(session_id,
                                                    future.GetCallback());

  fake_nearby_connection_->AppendReadableData({0x00, 0x01, 0x02});
  std::vector<uint8_t> wifi_request = fake_nearby_connection_->GetWrittenData();
  std::string wifi_request_string(wifi_request.begin(), wifi_request.end());
  absl::optional<base::Value> parsed_wifi_request_json =
      base::JSONReader::Read(wifi_request_string);
  ASSERT_TRUE(parsed_wifi_request_json);
  ASSERT_TRUE(parsed_wifi_request_json->is_dict());
  base::Value::Dict& written_wifi_credentials_request =
      parsed_wifi_request_json.value().GetDict();

  // Try to decode the payload written to Nearby Connections
  std::string base64_encoded_payload =
      *written_wifi_credentials_request.FindString("quickStartPayload");

  absl::optional<std::vector<uint8_t>> parsed_payload =
      base::Base64Decode(base64_encoded_payload);

  EXPECT_TRUE(parsed_payload.has_value());
  std::string parsed_payload_string(parsed_payload->begin(),
                                    parsed_payload->end());

  absl::optional<base::Value> parsed_wifi_request_payload_json =
      base::JSONReader::Read(parsed_payload_string);
  ASSERT_TRUE(parsed_wifi_request_payload_json);
  ASSERT_TRUE(parsed_wifi_request_payload_json->is_dict());
  base::Value::Dict& wifi_request_payload =
      parsed_wifi_request_payload_json.value().GetDict();

  EXPECT_TRUE(wifi_request_payload.FindBool("request_wifi"));
  EXPECT_EQ(wifi_request_payload.FindInt("SESSION_ID"), session_id);

  std::string shared_secret_str(kSecondarySharedSecret.begin(),
                                kSecondarySharedSecret.end());
  std::string shared_secret_base64;
  base::Base64Encode(shared_secret_str, &shared_secret_base64);
  EXPECT_EQ(*wifi_request_payload.FindString("shared_secret"),
            shared_secret_base64);

  const absl::optional<WifiCredentials>& credentials = future.Get();
  EXPECT_TRUE(credentials.has_value());
  EXPECT_EQ(credentials->ssid, "ssid");
  EXPECT_EQ(credentials->password, "password");
  EXPECT_EQ(credentials->security_type,
            ash::quick_start::mojom::WifiSecurityType::kPSK);
  EXPECT_TRUE(credentials->is_hidden);
}

TEST_F(ConnectionTest, RequestWifiCredentialsReturnsEmptyOnFailure) {
  MarkConnectionAuthenticated();
  // Random Session ID for testing
  int32_t session_id = 1;
  fake_quick_start_decoder_->SetWifiCredentialsResponse(
      mojom::GetWifiCredentialsResponse::NewFailureReason(
          mojom::GetWifiCredentialsFailureReason::kMissingWifiHiddenStatus));

  base::test::TestFuture<absl::optional<WifiCredentials>> future;

  authenticated_connection_->RequestWifiCredentials(session_id,
                                                    future.GetCallback());

  fake_nearby_connection_->AppendReadableData({0x00, 0x01, 0x02});

  absl::optional<WifiCredentials> credentials = future.Get();
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ConnectionTest, RequestAccountTransferAssertion) {
  MarkConnectionAuthenticated();
  // Start the Quick Start account transfer flow by initially sending
  // BootstrapOptions.
  authenticated_connection_->RequestAccountTransferAssertion(
      kChallengeBase64Url, base::BindOnce(&ConnectionTest::VerifyAssertionInfo,
                                          base::Unretained(this)));

  std::vector<uint8_t> bootstrap_options_data =
      fake_nearby_connection_->GetWrittenData();
  std::unique_ptr<ash::quick_start::QuickStartMessage>
      bootstrap_options_message =
          ash::quick_start::QuickStartMessage::ReadMessage(
              bootstrap_options_data,
              QuickStartMessageType::kBootstrapConfigurations);
  ASSERT_TRUE(bootstrap_options_message != nullptr);
  base::Value::Dict& bootstrap_options =
      *bootstrap_options_message->GetPayload();

  // Verify that BootstrapOptions is written as expected.
  EXPECT_EQ(*bootstrap_options.FindInt(kAccountRequirementKey),
            kAccountRequirementSingle);
  EXPECT_EQ(*bootstrap_options.FindInt(kFlowTypeKey), kFlowTypeTargetChallenge);

  // Emulate a BootstrapConfigurations response.
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  // OnBootstrapOptionsResponse should trigger a write of FIDO GetInfo
  // request.
  std::vector<uint8_t> fido_get_info_data =
      fake_nearby_connection_->GetWrittenData();

  std::unique_ptr<ash::quick_start::QuickStartMessage> fido_message =
      ash::quick_start::QuickStartMessage::ReadMessage(
          fido_get_info_data, QuickStartMessageType::kSecondDeviceAuthPayload);
  ASSERT_TRUE(fido_message != nullptr);

  // Verify that FIDO GetInfo request is written as expected
  base::Value::Dict* get_info_payload = fido_message->GetPayload();
  std::string get_info_message = *get_info_payload->FindString("fidoMessage");
  absl::optional<std::vector<uint8_t>> get_info_command =
      base::Base64Decode(get_info_message);
  EXPECT_TRUE(get_info_command);
  EXPECT_EQ(*get_info_command, kExpectedGetInfoRequest);

  // Emulate a GetInfo response.
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  // OnFidoGetInfoResponse should trigger a write of FIDO GetAssertion
  // request.
  std::vector<uint8_t> fido_assertion_request_data =
      fake_nearby_connection_->GetWrittenData();

  std::unique_ptr<ash::quick_start::QuickStartMessage> assertion_message =
      ash::quick_start::QuickStartMessage::ReadMessage(
          fido_assertion_request_data,
          QuickStartMessageType::kSecondDeviceAuthPayload);

  ASSERT_TRUE(assertion_message != nullptr);

  std::string get_assertion_message_payload =
      *assertion_message->GetPayload()->FindString("fidoMessage");
  absl::optional<std::vector<uint8_t>> get_assertion_command =
      base::Base64Decode(get_assertion_message_payload);
  EXPECT_TRUE(get_assertion_command);
  cbor::Value request =
      requests::GenerateGetAssertionRequest(kChallengeBase64Url);
  std::vector<uint8_t> cbor_encoded_request =
      requests::CBOREncodeGetAssertionRequest(std::move(request));
  EXPECT_EQ(*get_assertion_command, cbor_encoded_request);

  // Emulate a GetAssertion response.
  std::vector<uint8_t> credential_id = {0x01, 0x02, 0x03};
  std::string expected_credential_id(credential_id.begin(),
                                     credential_id.end());
  std::vector<uint8_t> auth_data = {0x02, 0x03, 0x04};
  std::vector<uint8_t> signature = {0x03, 0x04, 0x05};
  std::string email = "testcase@google.com";
  std::vector<uint8_t> user_id(email.begin(), email.end());
  // kSuccess
  uint8_t status = kSuccess;
  std::vector<uint8_t> data = BuildEncodedResponseData(
      credential_id, auth_data, signature, user_id, status);

  fake_quick_start_decoder_->SetExpectedData(data);
  fake_quick_start_decoder_->SetAssertionResponse(
      /*status=*/mojom::GetAssertionResponse::GetAssertionStatus::kSuccess,
      /*decoder_status=*/kSuccess,
      /*decoder_error=*/kSuccess, /*email=*/email,
      /*credential_id=*/expected_credential_id,
      /*signature=*/signature, auth_data);
  fake_nearby_connection_->AppendReadableData(data);
  EXPECT_FALSE(fake_nearby_connection_->IsClosed());

  // Wait for callback to finish and verify response
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(assertion_info_.has_value());
  EXPECT_EQ(email, assertion_info_->email);
  EXPECT_EQ(expected_credential_id, assertion_info_->credential_id);
  EXPECT_EQ(auth_data, assertion_info_->authenticator_data);
  EXPECT_EQ(signature, assertion_info_->signature);
}

TEST_F(ConnectionTest, NotifySourceOfUpdate) {
  MarkConnectionAuthenticated();

  int32_t session_id = 1;

  authenticated_connection_->NotifySourceOfUpdate(session_id);

  std::vector<uint8_t> notify_source_data =
      fake_nearby_connection_->GetWrittenData();

  std::unique_ptr<ash::quick_start::QuickStartMessage> notify_source_message =
      ash::quick_start::QuickStartMessage::ReadMessage(
          notify_source_data, QuickStartMessageType::kQuickStartPayload);
  ASSERT_TRUE(notify_source_message != nullptr);
  base::Value::Dict& parsed_payload = *notify_source_message->GetPayload();

  EXPECT_EQ(parsed_payload.FindBool(kNotifySourceOfUpdateMessageKey), true);

  EXPECT_EQ(parsed_payload.FindInt("SESSION_ID"), session_id);

  std::string shared_secret_str(kSecondarySharedSecret.begin(),
                                kSecondarySharedSecret.end());
  std::string shared_secret_base64;
  base::Base64Encode(shared_secret_str, &shared_secret_base64);
  EXPECT_EQ(*parsed_payload.FindString("shared_secret"), shared_secret_base64);
}

TEST_F(ConnectionTest, SendPayloadAndReadResponse) {
  base::Value::Dict message_payload;
  message_payload.Set(kTestMessagePayloadKey, kTestMessagePayloadValue);
  fake_nearby_connection_->AppendReadableData(
      std::vector<uint8_t>(std::begin(kTestBytes), std::end(kTestBytes)));

  base::RunLoop run_loop;
  SendPayloadAndReadResponse(
      message_payload,
      base::BindLambdaForTesting(
          [&run_loop](absl::optional<std::vector<uint8_t>> response) {
            EXPECT_EQ(response.value(),
                      std::vector<uint8_t>(std::begin(kTestBytes),
                                           std::end(kTestBytes)));
            run_loop.Quit();
          }));

  std::vector<uint8_t> written_payload =
      fake_nearby_connection_->GetWrittenData();
  std::string written_payload_string(written_payload.begin(),
                                     written_payload.end());
  absl::optional<base::Value> parsed_json =
      base::JSONReader::Read(written_payload_string);
  ASSERT_TRUE(parsed_json);
  ASSERT_TRUE(parsed_json->is_dict());
  base::Value::Dict& parsed_json_dict = parsed_json.value().GetDict();
  EXPECT_EQ(*parsed_json_dict.FindString(kTestMessagePayloadKey),
            kTestMessagePayloadValue);
}

TEST_F(ConnectionTest, TestClose) {
  base::test::TestFuture<TargetDeviceConnectionBroker::ConnectionClosedReason>
      future;
  std::unique_ptr<Connection> connection_under_test =
      std::make_unique<Connection>(
          fake_nearby_connection_.get(), session_id_, kSharedSecret,
          kSecondarySharedSecret, std::make_unique<ConstantNonceGenerator>(),
          /*on_connection_closed=*/future.GetCallback(),
          /*on_connection_authenticated=*/base::DoNothing());

  ASSERT_FALSE(future.IsReady());
  ASSERT_EQ(connection_under_test->GetState(), Connection::State::kOpen);

  connection_under_test->Close(
      TargetDeviceConnectionBroker::ConnectionClosedReason::kComplete);

  ASSERT_TRUE(future.IsReady());
  ASSERT_EQ(future.Get(),
            TargetDeviceConnectionBroker::ConnectionClosedReason::kComplete);
  ASSERT_EQ(connection_under_test->GetState(), Connection::State::kClosed);
}

TEST_F(ConnectionTest, TestDisconnectsWithoutCloseIssueUnknownError) {
  base::test::TestFuture<TargetDeviceConnectionBroker::ConnectionClosedReason>
      future;
  std::unique_ptr<Connection> connection_under_test =
      std::make_unique<Connection>(
          fake_nearby_connection_.get(), session_id_, kSharedSecret,
          kSecondarySharedSecret, std::make_unique<ConstantNonceGenerator>(),
          /*on_connection_closed=*/future.GetCallback(),
          /*on_connection_authenticated=*/base::DoNothing());

  ASSERT_FALSE(future.IsReady());
  ASSERT_EQ(connection_under_test->GetState(), Connection::State::kOpen);

  fake_nearby_connection_->Close();

  ASSERT_TRUE(future.IsReady());
  ASSERT_EQ(
      future.Get(),
      TargetDeviceConnectionBroker::ConnectionClosedReason::kUnknownError);
  ASSERT_EQ(connection_under_test->GetState(), Connection::State::kClosed);
}

TEST_F(ConnectionTest, InitiateHandshake) {
  connection_->InitiateHandshake(kAuthToken, base::DoNothing());
  std::vector<uint8_t> written_payload =
      fake_nearby_connection_->GetWrittenData();
  ASSERT_EQ(kTargetDeviceAuthMessage.size(), written_payload.size());
  for (size_t i = 0; i < kTargetDeviceAuthMessage.size(); i++) {
    EXPECT_EQ(kTargetDeviceAuthMessage[i], written_payload[i]);
  }
}

}  // namespace ash::quick_start
