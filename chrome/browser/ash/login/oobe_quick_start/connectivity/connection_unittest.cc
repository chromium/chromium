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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/advertising_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/handshake_helpers.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "chromeos/ash/components/quick_start/fake_quick_start_decoder.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/components/quick_start/quick_start_requests.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-forward.h"
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

const char kAccountRequirementKey[] = "accountRequirement";
const char kFlowTypeKey[] = "flowType";
const char kDeviceTypeKey[] = "deviceType";
const int kAccountRequirementSingle = 2;
const int kFlowTypeTargetChallenge = 2;
const int kDeviceTypeChrome = 7;

const char kChallengeBase64[] = "aQ==";

const std::vector<uint8_t> kTestBytes = {0x00, 0x01, 0x02};
const std::vector<uint8_t> kExpectedGetInfoRequest = {0x04};
constexpr base::TimeDelta kShortInterval = base::Milliseconds(20);

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

// 6 random bytes to use as the AdvertisingId.
constexpr std::array<uint8_t, 6> kAdvertisingId = {0x6b, 0xb3, 0x85,
                                                   0x27, 0xbb, 0x28};
// random int with 64 bits to use as SessionId.
constexpr uint64_t kSessionId = 184467440;

// 12 random bytes to use as the nonce.
constexpr std::array<uint8_t, 12> kNonce = {0x60, 0x3e, 0x87, 0x69, 0xa3, 0x55,
                                            0xd3, 0x49, 0xbd, 0x0a, 0x63, 0xed};

constexpr base::TimeDelta kResponseTimeout = base::Seconds(3);

constexpr char kGaiaTransferResultName[] = "QuickStart.GaiaTransferResult";

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
    session_context_ = std::make_unique<SessionContext>(
        kSessionId, advertising_id_, kSharedSecret, kSecondarySharedSecret);
    connection_ = std::make_unique<Connection>(
        nearby_connection, *session_context_,
        mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>(
            fake_quick_start_decoder_->GetRemote()),
        /*on_connection_closed=*/base::DoNothing(),
        /*on_connection_authenticated=*/
        base::BindLambdaForTesting(
            [&](base::WeakPtr<
                TargetDeviceConnectionBroker::AuthenticatedConnection>
                    authenticated_connection) {
              ran_connection_authenticated_callback_ = true;
              authenticated_connection_ = authenticated_connection;
            }));
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

  bool IsResponseTimeoutTimerRunning() {
    return connection_->response_timeout_timer_.IsRunning();
  }

  AccountTransferClientData* GetClientData() {
    return connection_->client_data_.get();
  }

  bool SimulateBootstrapConfigurationsResponse(
      absl::optional<std::string> instance_id) {
    base::test::TestFuture<void> future;
    if (instance_id.has_value()) {
      connection_->OnBootstrapConfigurationsResponse(
          future.GetCallback(),
          mojom::QuickStartMessage::NewBootstrapConfigurations(
              mojom::BootstrapConfigurations::New(instance_id.value())));
    } else {
      connection_->OnBootstrapConfigurationsResponse(future.GetCallback(),
                                                     nullptr);
    }
    return future.Wait();
  }

  void SendBytesAndReadResponse(std::vector<uint8_t>&& bytes,
                                Connection::ConnectionResponseCallback callback,
                                base::TimeDelta timeout,
                                QuickStartResponseType response_type =
                                    QuickStartResponseType::kHandshake) {
    connection_->SendBytesAndReadResponse(std::move(bytes), response_type,
                                          std::move(callback), timeout);
  }

  void EmulateEmptyResponseReceived(QuickStartResponseType response_type) {
    base::test::TestFuture<absl::optional<std::vector<uint8_t>>> future;
    SendBytesAndReadResponse(std::vector<uint8_t>(kTestBytes),
                             future.GetCallback(), kResponseTimeout,
                             response_type);
    connection_->OnResponseReceived(future.GetCallback(), response_type,
                                    absl::nullopt);
    TestMessageMetrics(
        /*succeeded=*/false, /*message_type=*/
        QuickStartMetrics::MapResponseToMessageType(response_type),
        /*error_code=*/
        QuickStartMetrics::MessageReceivedErrorCode::kDeserializationFailure);
  }

  void OnHandshakeResponse(base::OnceCallback<void(bool)> callback) {
    connection_->OnHandshakeResponse(kAuthToken, std::move(callback),
                                     absl::nullopt);
  }

  bool OnRequestAccountTransferAssertionResponse() {
    base::test::TestFuture<absl::optional<FidoAssertionInfo>> future;
    connection_->OnRequestAccountTransferAssertionResponse(future.GetCallback(),
                                                           nullptr);
    return future.Get().has_value();
  }

  void TestMessageMetrics(
      bool should_succeed,
      QuickStartMetrics::MessageType message_type,
      absl::optional<QuickStartMetrics::MessageReceivedErrorCode> error_code) {
    histogram_tester_.ExpectBucketCount("QuickStart.MessageSent.MessageType",
                                        message_type, 1);
    histogram_tester_.ExpectBucketCount(
        "QuickStart.MessageReceived.DesiredMessageType", message_type, 1);
    switch (message_type) {
      case QuickStartMetrics::MessageType::kWifiCredentials:
        histogram_tester_.ExpectBucketCount(
            "QuickStart.MessageReceived.WifiCredentials.Succeeded",
            should_succeed, 1);
        histogram_tester_.ExpectTotalCount(
            "QuickStart.MessageReceived.WifiCredentials.ListenDuration", 1);
        if (error_code.has_value()) {
          histogram_tester_.ExpectBucketCount(
              "QuickStart.MessageReceived.WifiCredentials.ErrorCode",
              error_code.value(), 1);
        }
        break;
      case QuickStartMetrics::MessageType::kBootstrapConfigurations:
        histogram_tester_.ExpectBucketCount(
            "QuickStart.MessageReceived.BootstrapConfigurations.Succeeded",
            should_succeed, 1);
        histogram_tester_.ExpectTotalCount(
            "QuickStart.MessageReceived.BootstrapConfigurations.ListenDuration",
            1);
        if (error_code.has_value()) {
          histogram_tester_.ExpectBucketCount(
              "QuickStart.MessageReceived.BootstrapConfigurations.ErrorCode",
              error_code.value(), 1);
        }
        break;
      case QuickStartMetrics::MessageType::kHandshake:
        histogram_tester_.ExpectBucketCount(
            "QuickStart.MessageReceived.Handshake.Succeeded", should_succeed,
            1);
        histogram_tester_.ExpectTotalCount(
            "QuickStart.MessageReceived.Handshake.ListenDuration", 1);
        if (error_code.has_value()) {
          histogram_tester_.ExpectBucketCount(
              "QuickStart.MessageReceived.Handshake.ErrorCode",
              error_code.value(), 1);
        }
        break;
      case QuickStartMetrics::MessageType::kNotifySourceOfUpdate:
        histogram_tester_.ExpectBucketCount(
            "QuickStart.MessageReceived.NotifySourceOfUpdate.Succeeded",
            should_succeed, 1);
        histogram_tester_.ExpectTotalCount(
            "QuickStart.MessageReceived.NotifySourceOfUpdate.ListenDuration",
            1);
        if (error_code.has_value()) {
          histogram_tester_.ExpectBucketCount(
              "QuickStart.MessageReceived.NotifySourceOfUpdate.ErrorCode",
              error_code.value(), 1);
        }
        break;
      case QuickStartMetrics::MessageType::kGetInfo:
        histogram_tester_.ExpectBucketCount(
            "QuickStart.MessageReceived.GetInfo.Succeeded", should_succeed, 1);
        histogram_tester_.ExpectTotalCount(
            "QuickStart.MessageReceived.GetInfo.ListenDuration", 1);
        if (error_code.has_value()) {
          histogram_tester_.ExpectBucketCount(
              "QuickStart.MessageReceived.GetInfo.ErrorCode",
              error_code.value(), 1);
        }
        break;
      case QuickStartMetrics::MessageType::kAssertion:
        histogram_tester_.ExpectBucketCount(
            "QuickStart.MessageReceived.Assertion.Succeeded", should_succeed,
            1);
        histogram_tester_.ExpectTotalCount(
            "QuickStart.MessageReceived.Assertion.ListenDuration", 1);
        if (error_code.has_value()) {
          histogram_tester_.ExpectBucketCount(
              "QuickStart.MessageReceived.Assertion.ErrorCode",
              error_code.value(), 1);
        }
        break;
    }
  }

  void TestHandshakeMetrics(
      bool should_succeed,
      absl::optional<QuickStartMetrics::HandshakeErrorCode> error_code) {
    if (!should_succeed) {
      histogram_tester_.ExpectBucketCount(
          "QuickStart.HandshakeResult.ErrorCode", error_code.value(), 1);
    }
    histogram_tester_.ExpectBucketCount("QuickStart.HandshakeResult.Succeeded",
                                        should_succeed, 1);
    histogram_tester_.ExpectTotalCount("QuickStart.HandshakeResult.Duration",
                                       1);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
  std::unique_ptr<Connection> connection_;
  std::unique_ptr<SessionContext> session_context_;
  AdvertisingId advertising_id_ = AdvertisingId(kAdvertisingId);
  bool ran_assertion_response_callback_ = false;
  bool ran_connection_authenticated_callback_ = false;
  base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>
      authenticated_connection_;
  std::unique_ptr<FakeQuickStartDecoder> fake_quick_start_decoder_;
  absl::optional<FidoAssertionInfo> assertion_info_;
  const Base64UrlString kChallenge_ =
      *Base64UrlTranscode(Base64String(kChallengeBase64));
  base::HistogramTester histogram_tester_;
};

TEST_F(ConnectionTest, RequestWifiCredentials) {
  MarkConnectionAuthenticated();

  fake_quick_start_decoder_->SetWifiCredentialsResponse(
      mojom::WifiCredentials::New("ssid", mojom::WifiSecurityType::kPSK, true,
                                  "password"));

  base::test::TestFuture<absl::optional<mojom::WifiCredentials>> future;

  authenticated_connection_->RequestWifiCredentials(future.GetCallback());

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
  EXPECT_EQ(wifi_request_payload.FindInt("SESSION_ID"),
            static_cast<int>(kSessionId));

  std::string shared_secret_str(kSecondarySharedSecret.begin(),
                                kSecondarySharedSecret.end());
  std::string shared_secret_base64;
  base::Base64Encode(shared_secret_str, &shared_secret_base64);
  EXPECT_EQ(*wifi_request_payload.FindString("shared_secret"),
            shared_secret_base64);

  const absl::optional<mojom::WifiCredentials>& credentials = future.Get();
  ASSERT_TRUE(credentials.has_value());
  EXPECT_EQ(credentials.value().ssid, "ssid");
  EXPECT_EQ(credentials.value().password, "password");
  EXPECT_EQ(credentials.value().security_type,
            ash::quick_start::mojom::WifiSecurityType::kPSK);
  EXPECT_TRUE(credentials.value().is_hidden);
  TestMessageMetrics(
      /*should_succeed=*/true,
      /*message_type=*/QuickStartMetrics::MessageType::kWifiCredentials,
      /*error_code=*/absl::nullopt);
}

TEST_F(ConnectionTest, RequestWifiCredentialsReturnsEmptyOnFailure) {
  MarkConnectionAuthenticated();
  fake_quick_start_decoder_->SetDecoderError(
      mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);

  base::test::TestFuture<absl::optional<mojom::WifiCredentials>> future;

  authenticated_connection_->RequestWifiCredentials(future.GetCallback());

  fake_nearby_connection_->AppendReadableData({0x00, 0x01, 0x02});

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ConnectionTest, RequestAccountInfo) {
  MarkConnectionAuthenticated();

  base::test::TestFuture<void> future;
  authenticated_connection_->RequestAccountInfo(future.GetCallback());

  std::vector<uint8_t> bootstrap_options_data =
      fake_nearby_connection_->GetWrittenData();
  QuickStartMessage::ReadResult read_result =
      ash::quick_start::QuickStartMessage::ReadMessage(
          bootstrap_options_data, QuickStartMessageType::kBootstrapOptions);
  ASSERT_TRUE(read_result.has_value());
  base::Value::Dict& bootstrap_options = *read_result.value()->GetPayload();

  // Verify that BootstrapOptions is written as expected.
  EXPECT_EQ(*bootstrap_options.FindInt(kAccountRequirementKey),
            kAccountRequirementSingle);
  EXPECT_EQ(*bootstrap_options.FindInt(kFlowTypeKey), kFlowTypeTargetChallenge);
  EXPECT_EQ(*bootstrap_options.FindInt(kDeviceTypeKey), kDeviceTypeChrome);

  // Emulate a BootstrapConfigurations response.
  std::vector<uint8_t> instance_id = {0x01, 0x02, 0x03};
  std::string expected_instance_id(instance_id.begin(), instance_id.end());
  fake_quick_start_decoder_->SetBootstrapConfigurationsResponse(
      expected_instance_id);
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  ASSERT_TRUE(future.Wait());

  TestMessageMetrics(/*should_succeed=*/true, /*message_type=*/
                     QuickStartMetrics::MessageType::kBootstrapConfigurations,
                     /*error_code=*/absl::nullopt);
}

TEST_F(ConnectionTest, RequestAccountTransferAssertion) {
  MarkConnectionAuthenticated();
  // Start the Quick Start account transfer flow by initially sending
  // a FIDO GetInfo request.
  authenticated_connection_->RequestAccountTransferAssertion(
      kChallenge_, base::BindOnce(&ConnectionTest::VerifyAssertionInfo,
                                  base::Unretained(this)));

  EXPECT_EQ(GetClientData()->GetChallengeBase64URLString(), kChallenge_);

  std::vector<uint8_t> fido_get_info_data =
      fake_nearby_connection_->GetWrittenData();

  QuickStartMessage::ReadResult get_info_request =
      ash::quick_start::QuickStartMessage::ReadMessage(
          fido_get_info_data, QuickStartMessageType::kSecondDeviceAuthPayload);
  ASSERT_TRUE(get_info_request.has_value());

  // Verify that FIDO GetInfo request is written as expected
  base::Value::Dict* get_info_payload = get_info_request.value()->GetPayload();
  std::string get_info_message = *get_info_payload->FindString("fidoMessage");
  absl::optional<std::vector<uint8_t>> get_info_command =
      base::Base64Decode(get_info_message);
  EXPECT_TRUE(get_info_command);
  EXPECT_EQ(*get_info_command, kExpectedGetInfoRequest);

  // Emulate a GetInfo response.
  fake_nearby_connection_->AppendReadableData(kTestBytes);
  TestMessageMetrics(
      /*should_succeed=*/true,
      /*message_type=*/QuickStartMetrics::MessageType::kGetInfo,
      /*error_code=*/absl::nullopt);

  // OnFidoGetInfoResponse should trigger a write of FIDO GetAssertion
  // request.
  std::vector<uint8_t> fido_assertion_request_data =
      fake_nearby_connection_->GetWrittenData();

  QuickStartMessage::ReadResult assertion_read_result =
      ash::quick_start::QuickStartMessage::ReadMessage(
          fido_assertion_request_data,
          QuickStartMessageType::kSecondDeviceAuthPayload);

  ASSERT_TRUE(assertion_read_result.has_value());

  std::string get_assertion_message_payload =
      *assertion_read_result.value()->GetPayload()->FindString("fidoMessage");
  absl::optional<std::vector<uint8_t>> get_assertion_command =
      base::Base64Decode(get_assertion_message_payload);
  EXPECT_TRUE(get_assertion_command);
  cbor::Value request = requests::GenerateGetAssertionRequest(
      AccountTransferClientData(kChallenge_).CreateHash());
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

  fake_quick_start_decoder_->SetAssertionResponse(
      mojom::FidoAssertionResponse::New(
          /*email=*/email,
          /*credential_id=*/expected_credential_id,
          /*auth_data=*/auth_data,
          /*signature=*/signature));
  fake_nearby_connection_->AppendReadableData(data);
  EXPECT_FALSE(fake_nearby_connection_->IsClosed());
  TestMessageMetrics(
      /*should_succeed=*/true,
      /*message_type=*/QuickStartMetrics::MessageType::kAssertion,
      /*error_code=*/absl::nullopt);

  // Wait for callback to finish and verify response
  // TODO(b/306474980): Eliminate RunUntilIdle, simplify this test
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(assertion_info_.has_value());
  EXPECT_EQ(email, assertion_info_->email);
  EXPECT_EQ(expected_credential_id, assertion_info_->credential_id);
  EXPECT_EQ(auth_data, assertion_info_->authenticator_data);
  EXPECT_EQ(signature, assertion_info_->signature);
  histogram_tester_.ExpectBucketCount(kGaiaTransferResultName, true, 1);
}

TEST_F(ConnectionTest, RequestAccountTransferAssertion_UnexpectedMessage) {
  MarkConnectionAuthenticated();

  base::test::TestFuture<absl::optional<FidoAssertionInfo>> future;
  // Start the Quick Start account transfer flow by initially sending
  // a FIDO GetInfo request.
  authenticated_connection_->RequestAccountTransferAssertion(
      kChallenge_, future.GetCallback());

  // Emulate a GetInfo response.
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  // Set an unexpected response.
  fake_quick_start_decoder_->SetNotifySourceOfUpdateResponse(
      mojom::NotifySourceOfUpdateResponse::New(/*ack_received=*/true));
  fake_nearby_connection_->AppendReadableData({0x01, 0x02, 0x03});

  // RequestAccountTransferAssertion() will retry after receiving an unexpected
  // response, so set a valid response.
  std::vector<uint8_t> credential_id = {0x01, 0x02, 0x03};
  std::string expected_credential_id(credential_id.begin(),
                                     credential_id.end());
  std::vector<uint8_t> auth_data = {0x02, 0x03, 0x04};
  std::vector<uint8_t> signature = {0x03, 0x04, 0x05};
  std::string email = "testcase@google.com";
  std::vector<uint8_t> user_id(email.begin(), email.end());
  fake_quick_start_decoder_->SetAssertionResponse(
      mojom::FidoAssertionResponse::New(
          /*email=*/email,
          /*credential_id=*/expected_credential_id,
          /*auth_data=*/auth_data,
          /*signature=*/signature));
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  absl::optional<FidoAssertionInfo> response = future.Get();
  EXPECT_TRUE(response.has_value());
}

TEST_F(ConnectionTest, NotifySourceOfUpdate_Success) {
  MarkConnectionAuthenticated();
  fake_quick_start_decoder_->SetNotifySourceOfUpdateResponse(
      mojom::NotifySourceOfUpdateResponse::New(/*ack_received=*/true));
  base::test::TestFuture<bool> future;

  authenticated_connection_->NotifySourceOfUpdate(future.GetCallback());

  fake_nearby_connection_->AppendReadableData(kTestBytes);
  std::vector<uint8_t> notify_source_data =
      fake_nearby_connection_->GetWrittenData();

  QuickStartMessage::ReadResult read_result =
      ash::quick_start::QuickStartMessage::ReadMessage(
          notify_source_data, QuickStartMessageType::kQuickStartPayload);
  ASSERT_TRUE(read_result.has_value());
  base::Value::Dict& parsed_payload = *read_result.value()->GetPayload();

  EXPECT_EQ(parsed_payload.FindBool(kNotifySourceOfUpdateMessageKey), true);

  EXPECT_EQ(parsed_payload.FindInt("SESSION_ID"), static_cast<int>(kSessionId));

  std::string shared_secret_str(kSecondarySharedSecret.begin(),
                                kSecondarySharedSecret.end());
  std::string shared_secret_base64;
  base::Base64Encode(shared_secret_str, &shared_secret_base64);
  EXPECT_EQ(*parsed_payload.FindString("shared_secret"), shared_secret_base64);

  EXPECT_TRUE(future.Get());
  TestMessageMetrics(
      /*should_succeed=*/true,
      /*message_type=*/QuickStartMetrics::MessageType::kNotifySourceOfUpdate,
      /*error_code=*/absl::nullopt);
}

TEST_F(ConnectionTest, NotifySourceOfUpdate_FalseAckReceivedValue) {
  MarkConnectionAuthenticated();
  fake_quick_start_decoder_->SetNotifySourceOfUpdateResponse(
      mojom::NotifySourceOfUpdateResponse::New(/*ack_received=*/false));
  base::test::TestFuture<bool> future;

  authenticated_connection_->NotifySourceOfUpdate(future.GetCallback());

  fake_nearby_connection_->AppendReadableData({0x00, 0x01, 0x02});
  EXPECT_FALSE(future.Get());
  TestMessageMetrics(
      /*should_succeed=*/true,
      /*message_type=*/QuickStartMetrics::MessageType::kNotifySourceOfUpdate,
      /*error_code=*/absl::nullopt);
}

TEST_F(ConnectionTest, NotifySourceOfUpdate_NoAckReceivedValue) {
  MarkConnectionAuthenticated();
  fake_quick_start_decoder_->SetDecoderError(
      mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  base::test::TestFuture<bool> future;

  authenticated_connection_->NotifySourceOfUpdate(future.GetCallback());

  fake_nearby_connection_->AppendReadableData({0x00, 0x01, 0x02});
  EXPECT_FALSE(future.Get());
}

TEST_F(ConnectionTest, NotifySourceOfUpdate_UnexpectedMessage) {
  MarkConnectionAuthenticated();

  // Have the decoder provide an unexpected message.
  fake_quick_start_decoder_->SetUserVerificationRequested(true);
  base::test::TestFuture<bool> future;

  authenticated_connection_->NotifySourceOfUpdate(future.GetCallback());

  fake_nearby_connection_->AppendReadableData(kTestBytes);
  EXPECT_FALSE(future.Get());
}

TEST_F(ConnectionTest, NotifySourceOfUpdate_ResponseTimeout) {
  MarkConnectionAuthenticated();
  ASSERT_FALSE(IsResponseTimeoutTimerRunning());
  authenticated_connection_->NotifySourceOfUpdate(base::DoNothing());
  EXPECT_TRUE(IsResponseTimeoutTimerRunning());
  EXPECT_EQ(connection_->GetState(), Connection::State::kOpen);

  task_environment_.FastForwardBy(kResponseTimeout);
  EXPECT_EQ(connection_->GetState(), Connection::State::kClosed);
  TestMessageMetrics(
      /*should_succeed=*/false,
      /*message_type=*/QuickStartMetrics::MessageType::kNotifySourceOfUpdate,
      /*error_code=*/QuickStartMetrics::MessageReceivedErrorCode::kTimeOut);
}

TEST_F(ConnectionTest, SendBytesAndReadResponse_TimedOut) {
  ASSERT_FALSE(IsResponseTimeoutTimerRunning());

  base::test::TestFuture<absl::optional<std::vector<uint8_t>>> future;
  SendBytesAndReadResponse(std::vector<uint8_t>(kTestBytes),
                           future.GetCallback(), kResponseTimeout);

  EXPECT_TRUE(IsResponseTimeoutTimerRunning());
  EXPECT_EQ(connection_->GetState(), Connection::State::kOpen);

  task_environment_.FastForwardBy(kResponseTimeout);
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(connection_->GetState(), Connection::State::kClosed);
}

TEST_F(ConnectionTest, SendBytesAndReadResponse_SucceedsBeforeTimeout) {
  ASSERT_FALSE(IsResponseTimeoutTimerRunning());

  base::test::TestFuture<absl::optional<std::vector<uint8_t>>> future;
  SendBytesAndReadResponse(std::vector<uint8_t>(kTestBytes),
                           future.GetCallback(), kResponseTimeout);

  EXPECT_TRUE(IsResponseTimeoutTimerRunning());
  EXPECT_EQ(connection_->GetState(), Connection::State::kOpen);

  task_environment_.FastForwardBy(kResponseTimeout - kShortInterval);
  fake_nearby_connection_->AppendReadableData(kTestBytes);
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(connection_->GetState(), Connection::State::kOpen);

  task_environment_.FastForwardBy(kShortInterval);
  EXPECT_EQ(connection_->GetState(), Connection::State::kOpen);
}

TEST_F(ConnectionTest, TestClose) {
  base::test::TestFuture<TargetDeviceConnectionBroker::ConnectionClosedReason>
      future;
  std::unique_ptr<Connection> connection_under_test =
      std::make_unique<Connection>(
          fake_nearby_connection_.get(), *session_context_,
          mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>(
              fake_quick_start_decoder_->GetRemote()),
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
          fake_nearby_connection_.get(), *session_context_,
          mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>(
              fake_quick_start_decoder_->GetRemote()),
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
  base::test::TestFuture<bool> future;
  connection_->InitiateHandshake(kAuthToken, future.GetCallback());
  EXPECT_EQ(handshake::VerifyHandshakeMessage(
                fake_nearby_connection_->GetWrittenData(), kAuthToken,
                kSharedSecret, handshake::DeviceRole::kTarget),
            handshake::VerifyHandshakeMessageStatus::kSuccess);

  std::vector<uint8_t> response = handshake::BuildHandshakeMessage(
      kAuthToken, kSharedSecret, kNonce, handshake::DeviceRole::kSource);
  fake_nearby_connection_->AppendReadableData(response);
  EXPECT_TRUE(future.Get());
  TestMessageMetrics(
      /*should_succeed=*/true,
      /*message_type=*/QuickStartMetrics::MessageType::kHandshake,
      /*error_code=*/absl::nullopt);
  TestHandshakeMetrics(/*should_succeed=*/true, /*error_code=*/absl::nullopt);
}

TEST_F(ConnectionTest, InitiateHandshake_BadResponse) {
  base::test::TestFuture<bool> future;
  connection_->InitiateHandshake(kAuthToken, future.GetCallback());
  std::vector<uint8_t> written_payload =
      fake_nearby_connection_->GetWrittenData();

  // Simulate the source device sending back the same message it received from
  // the target device. Should fail because it uses the wrong role.
  fake_nearby_connection_->AppendReadableData(written_payload);
  EXPECT_FALSE(future.Get());
  TestHandshakeMetrics(/*should_succeed=*/false,
                       /*error_code=*/QuickStartMetrics::HandshakeErrorCode::
                           kUnexpectedAuthPayloadRole);
}

TEST_F(ConnectionTest, EmptyHandshakeResponse) {
  base::test::TestFuture<bool> future;
  connection_->InitiateHandshake(kAuthToken, future.GetCallback());
  OnHandshakeResponse(future.GetCallback());
  TestHandshakeMetrics(/*should_succeed=*/false,
                       /*error_code=*/QuickStartMetrics::HandshakeErrorCode::
                           kFailedToReadResponse);
}

TEST_F(ConnectionTest, TestUserVerificationRequested_ReturnsResult) {
  fake_quick_start_decoder_->SetUserVerificationRequested(true);
  fake_quick_start_decoder_->SetUserVerificationMethod(true);
  fake_quick_start_decoder_->SetUserVerificationResponse(
      mojom::UserVerificationResult::kUserVerified, true);

  MarkConnectionAuthenticated();

  base::test::TestFuture<absl::optional<mojom::UserVerificationResponse>>
      future;
  authenticated_connection_->WaitForUserVerification(future.GetCallback());
  fake_nearby_connection_->AppendReadableData(kTestBytes);
  fake_nearby_connection_->AppendReadableData(kTestBytes);
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(mojom::UserVerificationResult::kUserVerified, future.Get()->result);
  EXPECT_TRUE(future.Get()->is_first_user_verification);
}

TEST_F(ConnectionTest,
       TestUserVerificationRequested_TooManyUserVerificationPackets) {
  fake_quick_start_decoder_->SetUserVerificationRequested(true);
  fake_quick_start_decoder_->SetUserVerificationMethod(true);
  fake_quick_start_decoder_->SetUserVerificationMethod(true);

  MarkConnectionAuthenticated();

  base::test::TestFuture<absl::optional<mojom::UserVerificationResponse>>
      future;
  authenticated_connection_->WaitForUserVerification(future.GetCallback());
  fake_nearby_connection_->AppendReadableData(kTestBytes);
  fake_nearby_connection_->AppendReadableData(kTestBytes);
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  ASSERT_FALSE(future.Get().has_value());
}

TEST_F(ConnectionTest,
       TestUserVerificationRequested_UnsupportedVerificationMethod) {
  fake_quick_start_decoder_->SetUserVerificationRequested(true);
  fake_quick_start_decoder_->SetUserVerificationMethod(false);

  MarkConnectionAuthenticated();

  base::test::TestFuture<absl::optional<mojom::UserVerificationResponse>>
      future;
  authenticated_connection_->WaitForUserVerification(future.GetCallback());
  fake_nearby_connection_->AppendReadableData(kTestBytes);
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  ASSERT_FALSE(future.Get().has_value());
}

TEST_F(ConnectionTest, TestUserVerificationRequested_UnexpectedMessage) {
  std::vector<uint8_t> instance_id = {0x01, 0x02, 0x03};
  std::string expected_instance_id(instance_id.begin(), instance_id.end());
  fake_quick_start_decoder_->SetBootstrapConfigurationsResponse(
      expected_instance_id);

  MarkConnectionAuthenticated();

  base::test::TestFuture<absl::optional<mojom::UserVerificationResponse>>
      future;
  authenticated_connection_->WaitForUserVerification(future.GetCallback());
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  ASSERT_FALSE(future.Get().has_value());
}

TEST_F(ConnectionTest,
       TestUserVerificationRequested_ReturnsEmptyIfRequestIsEmpty) {
  fake_quick_start_decoder_->SetDecoderError(
      mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  fake_quick_start_decoder_->SetUserVerificationResponse(
      mojom::UserVerificationResult::kUserVerified, true);

  MarkConnectionAuthenticated();

  base::test::TestFuture<absl::optional<mojom::UserVerificationResponse>>
      future;
  authenticated_connection_->WaitForUserVerification(future.GetCallback());
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(
    ConnectionTest,
    TestUserVerificationRequested_ReturnsEmptyIfAwaitingUserVerificationIsFalse) {
  fake_quick_start_decoder_->SetUserVerificationRequested(false);
  fake_quick_start_decoder_->SetUserVerificationResponse(
      mojom::UserVerificationResult::kUserVerified, true);

  MarkConnectionAuthenticated();

  base::test::TestFuture<absl::optional<mojom::UserVerificationResponse>>
      future;
  authenticated_connection_->WaitForUserVerification(future.GetCallback());
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ConnectionTest,
       TestUserVerificationRequested_ReturnsEmptyIfResponseReturnsError) {
  fake_quick_start_decoder_->SetUserVerificationRequested(true);

  MarkConnectionAuthenticated();

  base::test::TestFuture<absl::optional<mojom::UserVerificationResponse>>
      future;
  authenticated_connection_->WaitForUserVerification(future.GetCallback());
  fake_nearby_connection_->AppendReadableData(kTestBytes);
  fake_quick_start_decoder_->SetDecoderError(
      mojom::QuickStartDecoderError::kMessageDoesNotMatchSchema);
  fake_nearby_connection_->AppendReadableData(kTestBytes);

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ConnectionTest, GetPhoneInstanceId) {
  MarkConnectionAuthenticated();

  // Phone instance ID is initially empty.
  EXPECT_TRUE(authenticated_connection_->get_phone_instance_id().empty());

  // Arbitrary instance ID.
  std::vector<uint8_t> instance_id = {0x01, 0x02, 0x03};
  std::string expected_instance_id(instance_id.begin(), instance_id.end());

  ASSERT_TRUE(SimulateBootstrapConfigurationsResponse(expected_instance_id));

  EXPECT_EQ(authenticated_connection_->get_phone_instance_id(),
            expected_instance_id);
}

TEST_F(ConnectionTest, ParseBootstrapConfigurationsHandlesNull) {
  MarkConnectionAuthenticated();
  ASSERT_TRUE(authenticated_connection_->get_phone_instance_id().empty());

  ASSERT_TRUE(SimulateBootstrapConfigurationsResponse(absl::nullopt));

  EXPECT_TRUE(authenticated_connection_->get_phone_instance_id().empty());
}

TEST_F(ConnectionTest, MetricsEmittedOnEmptyResponse) {
  const QuickStartResponseType response_types[] = {
      QuickStartResponseType::kWifiCredentials,
      QuickStartResponseType::kBootstrapConfigurations,
      QuickStartResponseType::kHandshake,
      QuickStartResponseType::kAssertion,
      QuickStartResponseType::kGetInfo,
      QuickStartResponseType::kNotifySourceOfUpdate};
  for (const auto response_type : response_types) {
    EmulateEmptyResponseReceived(response_type);
  }
}

}  // namespace ash::quick_start
