// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/authenticated_connection.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_quick_start_decoder.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/wifi_credentials.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
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
const char kBootstrapOptionsKey[] = "bootstrapOptions";
const char kAccountRequirementKey[] = "accountRequirement";
const char kFlowTypeKey[] = "flowType";
const int kAccountRequirementSingle = 2;
const int kFlowTypeTargetChallenge = 2;

const char kChallengeBase64Url[] = "testchallenge";
const char kTestOrigin[] = "https://google.com";
const char kCtapRequestType[] = "webauthn.get";

const std::vector<uint8_t> kTestBytes = {0x00, 0x01, 0x02};
const std::vector<uint8_t> kExpectedGetInfoRequest = {0x04};

const char kNotifySourceOfUpdateMessageKey[] = "isForcedUpdateRequired";
constexpr uint8_t kSuccess = 0x00;

// 32 random bytes to use as the shared secret.
constexpr std::array<uint8_t, 32> kSharedSecret = {
    0x54, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

// 6 random bytes to use as the RandomSessionId.
constexpr std::array<uint8_t, 6> kRandomSessionId = {0x6b, 0xb3, 0x85,
                                                     0x27, 0xbb, 0x28};
}  // namespace

class AuthenticatedConnectionTest : public testing::Test {
 public:
  AuthenticatedConnectionTest() = default;
  AuthenticatedConnectionTest(AuthenticatedConnectionTest&) = delete;
  AuthenticatedConnectionTest& operator=(AuthenticatedConnectionTest&) = delete;
  ~AuthenticatedConnectionTest() override = default;

  void SetUp() override {
    fake_nearby_connection_ = std::make_unique<FakeNearbyConnection>();
    fake_quick_start_decoder_ = std::make_unique<FakeQuickStartDecoder>();
    NearbyConnection* nearby_connection = fake_nearby_connection_.get();
    authenticated_connection_ = std::make_unique<AuthenticatedConnection>(
        nearby_connection,
        mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>(
            fake_quick_start_decoder_->GetRemote()),
        session_id_, kSharedSecret);
  }

  std::string CreateFidoClientDataJson(url::Origin origin) {
    return authenticated_connection_->CreateFidoClientDataJson(origin);
  }

  cbor::Value GenerateGetAssertionRequest() {
    return authenticated_connection_->GenerateGetAssertionRequest();
  }

  std::vector<uint8_t> CBOREncodeGetAssertionRequest(cbor::Value request) {
    return authenticated_connection_->CBOREncodeGetAssertionRequest(
        std::move(request));
  }

  void SetChallengeBytes(std::string challenge_b64url) {
    authenticated_connection_->challenge_b64url_ = challenge_b64url;
  }

  void VerifyAssertionInfo(absl::optional<FidoAssertionInfo> assertion_info) {
    ran_assertion_response_callback_ = true;
    assertion_info_ = assertion_info;
  }

  void VerifyWifiCredentials(absl::optional<WifiCredentials> credentials) {
    // TODO: Verify Wifi Credentials once parsed
  }

 protected:
  RandomSessionId session_id_ = RandomSessionId(kRandomSessionId);
  base::test::SingleThreadTaskEnvironment task_environment_;
  bool ran_assertion_response_callback_ = false;
  std::unique_ptr<AuthenticatedConnection> authenticated_connection_;
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
  std::unique_ptr<FakeQuickStartDecoder> fake_quick_start_decoder_;
  absl::optional<FidoAssertionInfo> assertion_info_;
};

TEST_F(AuthenticatedConnectionTest, RequestWifiCredentials) {
  // Random Session ID for testing
  int32_t session_id = 1;

  authenticated_connection_->RequestWifiCredentials(
      session_id,
      base::BindOnce(&AuthenticatedConnectionTest::VerifyWifiCredentials,
                     base::Unretained(this)));

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

  std::string shared_secret_str(kSharedSecret.begin(), kSharedSecret.end());
  std::string shared_secret_base64;
  base::Base64Encode(shared_secret_str, &shared_secret_base64);
  EXPECT_EQ(*wifi_request_payload.FindString("shared_secret"),
            shared_secret_base64);
}

TEST_F(AuthenticatedConnectionTest, RequestAccountTransferAssertion) {
  // Start the Quick Start account transfer flow by initially sending
  // BootstrapOptions.
  authenticated_connection_->RequestAccountTransferAssertion(
      kChallengeBase64Url,
      base::BindOnce(&AuthenticatedConnectionTest::VerifyAssertionInfo,
                     base::Unretained(this)));

  std::vector<uint8_t> bootstrap_options_data =
      fake_nearby_connection_->GetWrittenData();
  std::string bootstrap_options_string(bootstrap_options_data.begin(),
                                       bootstrap_options_data.end());
  absl::optional<base::Value> parsed_bootstrap_options_json =
      base::JSONReader::Read(bootstrap_options_string);
  ASSERT_TRUE(parsed_bootstrap_options_json);
  ASSERT_TRUE(parsed_bootstrap_options_json->is_dict());
  base::Value::Dict& parsed_bootstrap_options_dict =
      parsed_bootstrap_options_json.value().GetDict();
  base::Value::Dict& bootstrap_options =
      *parsed_bootstrap_options_dict.FindDict(kBootstrapOptionsKey);

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
  std::string fido_get_info_string(fido_get_info_data.begin(),
                                   fido_get_info_data.end());
  absl::optional<base::Value> parsed_fido_get_info_json =
      base::JSONReader::Read(fido_get_info_string);
  ASSERT_TRUE(parsed_fido_get_info_json);
  ASSERT_TRUE(parsed_fido_get_info_json->is_dict());
  base::Value::Dict& parsed_fido_get_info_dict =
      parsed_fido_get_info_json.value().GetDict();

  // Verify that FIDO GetInfo request is written as expected
  base::Value::Dict* get_info_payload =
      parsed_fido_get_info_dict.FindDict("secondDeviceAuthPayload");
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
  std::string fido_assertion_request_string(fido_assertion_request_data.begin(),
                                            fido_assertion_request_data.end());
  absl::optional<base::Value> parsed_fido_assertion_request_json =
      base::JSONReader::Read(fido_assertion_request_string);
  ASSERT_TRUE(parsed_fido_assertion_request_json);
  ASSERT_TRUE(parsed_fido_assertion_request_json->is_dict());
  base::Value::Dict& parsed_fido_assertion_request_dict =
      parsed_fido_assertion_request_json.value().GetDict();

  // Verify that FIDO GetAssertion request is written as expected.
  base::Value::Dict* get_assertion_payload =
      parsed_fido_assertion_request_dict.FindDict("secondDeviceAuthPayload");
  std::string get_assertion_message =
      *get_assertion_payload->FindString("fidoMessage");
  absl::optional<std::vector<uint8_t>> get_assertion_command =
      base::Base64Decode(get_assertion_message);
  EXPECT_TRUE(get_assertion_command);
  cbor::Value request = GenerateGetAssertionRequest();
  std::vector<uint8_t> cbor_encoded_request =
      CBOREncodeGetAssertionRequest(std::move(request));
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

TEST_F(AuthenticatedConnectionTest, CreateFidoClientDataJson) {
  url::Origin test_origin = url::Origin::Create(GURL(kTestOrigin));
  SetChallengeBytes(kChallengeBase64Url);
  std::string client_data_json = CreateFidoClientDataJson(test_origin);
  absl::optional<base::Value> parsed_json =
      base::JSONReader::Read(client_data_json);
  ASSERT_TRUE(parsed_json);
  ASSERT_TRUE(parsed_json->is_dict());
  base::Value::Dict& parsed_json_dict = parsed_json.value().GetDict();
  EXPECT_EQ(*parsed_json_dict.FindString("type"), kCtapRequestType);
  EXPECT_EQ(*parsed_json_dict.FindString("challenge"), kChallengeBase64Url);
  EXPECT_EQ(*parsed_json_dict.FindString("origin"), kTestOrigin);
  EXPECT_FALSE(parsed_json_dict.FindBool("crossOrigin").value());
}

TEST_F(AuthenticatedConnectionTest,
       GenerateGetAssertionRequest_ValidChallenge) {
  SetChallengeBytes(kChallengeBase64Url);
  cbor::Value request = GenerateGetAssertionRequest();
  ASSERT_TRUE(request.is_map());
  const cbor::Value::MapValue& request_map = request.GetMap();
  // CBOR Index 0x01 stores the relying_party_id for the GetAssertionRequest.
  EXPECT_EQ(request_map.find(cbor::Value(0x01))->second.GetString(),
            "google.com");
  // CBOR Index 0x05 stores the options for the GetAssertionRequest.
  const cbor::Value::MapValue& options_map =
      request_map.find(cbor::Value(0x05))->second.GetMap();
  // CBOR key "uv" stores the userVerification bit for the options.
  EXPECT_TRUE(options_map.find(cbor::Value("uv"))->second.GetBool());
  // CBOR key "up" stores the userPresence bit for the options.
  EXPECT_TRUE(options_map.find(cbor::Value("up"))->second.GetBool());
  EXPECT_TRUE(
      request_map.find(cbor::Value(0x02))->second.GetBytestring().size() > 0);
}

TEST_F(AuthenticatedConnectionTest, CBOREncodeGetAssertionRequest) {
  SetChallengeBytes(kChallengeBase64Url);
  cbor::Value request = GenerateGetAssertionRequest();
  std::vector<uint8_t> cbor_encoded_request =
      CBOREncodeGetAssertionRequest(std::move(request));
  absl::optional<cbor::Value> cbor;
  const base::span<const uint8_t> ctap_request_span =
      base::make_span(cbor_encoded_request);
  cbor = cbor::Reader::Read(ctap_request_span.subspan(1));
  ASSERT_TRUE(cbor);
  ASSERT_TRUE(cbor->is_map());
  const cbor::Value::MapValue& cbor_map = cbor->GetMap();
  // CBOR Index 0x01 stores the relying_party_id for the GetAssertionRequest.
  EXPECT_EQ(cbor_map.find(cbor::Value(0x01))->second.GetString(), "google.com");
}

TEST_F(AuthenticatedConnectionTest, NotifySourceOfUpdate) {
  authenticated_connection_->NotifySourceOfUpdate();

  std::vector<uint8_t> written_payload =
      fake_nearby_connection_->GetWrittenData();
  std::string written_payload_string(written_payload.begin(),
                                     written_payload.end());
  absl::optional<base::Value> parsed_json =
      base::JSONReader::Read(written_payload_string);
  ASSERT_TRUE(parsed_json);
  ASSERT_TRUE(parsed_json->is_dict());
  base::Value::Dict& parsed_json_dict = parsed_json.value().GetDict();
  EXPECT_EQ(*parsed_json_dict.FindBool(kNotifySourceOfUpdateMessageKey), true);
}

}  // namespace ash::quick_start
