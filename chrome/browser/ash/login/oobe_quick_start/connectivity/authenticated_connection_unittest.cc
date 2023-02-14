// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/authenticated_connection.h"

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash::quick_start {

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
}  // namespace

class AuthenticatedConnectionTest : public testing::Test {
 public:
  AuthenticatedConnectionTest() = default;
  AuthenticatedConnectionTest(AuthenticatedConnectionTest&) = delete;
  AuthenticatedConnectionTest& operator=(AuthenticatedConnectionTest&) = delete;
  ~AuthenticatedConnectionTest() override = default;

  void SetUp() override {
    fake_nearby_connection_ = std::make_unique<FakeNearbyConnection>();
    NearbyConnection* nearby_connection = fake_nearby_connection_.get();
    authenticated_connection_ =
        std::make_unique<AuthenticatedConnection>(nearby_connection);
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

 protected:
  std::unique_ptr<AuthenticatedConnection> authenticated_connection_;
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(AuthenticatedConnectionTest, RequestAccountTransferAssertion) {
  // Start the Quick Start account transfer flow by initially sending
  // BootstrapOptions.
  authenticated_connection_->RequestAccountTransferAssertion(
      kChallengeBase64Url, base::DoNothing());
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

  // OnBootstrapOptionsResponse should trigger a write of FIDO GetInfo request.
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
