// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/quick_start_decoder.h"

#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

namespace {

constexpr char kCredentialIdKey[] = "id";
constexpr char kEntitiyIdMapKey[] = "id";
constexpr char kBootstrapConfigurationsKey[] = "bootstrapConfigurations";
constexpr char kDeviceDetailsKey[] = "deviceDetails";
constexpr char kCryptauthDeviceIdKey[] = "cryptauthDeviceId";
constexpr char kExampleCryptauthDeviceId[] = "helloworld";
constexpr uint8_t kSuccess = 0x00;
constexpr uint8_t kCtap2ErrInvalidCBOR = 0x12;
constexpr int kCborDecoderErrorInvalidUtf8 = 6;
constexpr int kCborDecoderNoError = 0;
constexpr int kCborDecoderUnknownError = 14;

using GetAssertionStatus = mojom::GetAssertionResponse::GetAssertionStatus;

std::vector<uint8_t> BuildEncodedResponseData(
    std::vector<uint8_t> credential_id,
    std::vector<uint8_t> auth_data,
    std::vector<uint8_t> signature,
    std::vector<uint8_t> user_id,
    uint8_t status) {
  cbor::Value::MapValue cbor_map;
  cbor::Value::MapValue credential_map;
  credential_map[cbor::Value(kCredentialIdKey)] = cbor::Value(credential_id);
  cbor_map[cbor::Value(1)] = cbor::Value(credential_map);
  cbor_map[cbor::Value(2)] = cbor::Value(auth_data);
  cbor_map[cbor::Value(3)] = cbor::Value(signature);
  cbor::Value::MapValue user_map;
  user_map[cbor::Value(kEntitiyIdMapKey)] = cbor::Value(user_id);
  cbor_map[cbor::Value(4)] = cbor::Value(user_map);
  absl::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(cbor_map)));
  DCHECK(cbor_bytes);
  std::vector<uint8_t> response_bytes = std::move(*cbor_bytes);
  // Add the status byte to the beginning of this now fully encoded cbor bytes
  // vector.
  response_bytes.insert(response_bytes.begin(), status);
  return response_bytes;
}

}  // namespace

class QuickStartDecoderTest : public testing::Test {
 public:
  QuickStartDecoderTest() {
    decoder_ = std::make_unique<QuickStartDecoder>(
        remote_.BindNewPipeAndPassReceiver());
  }

  mojom::GetAssertionResponsePtr DoDecodeGetAssertionResponse(
      const std::vector<uint8_t>& data) {
    return decoder_->DoDecodeGetAssertionResponse(data);
  }

  mojom::BootstrapConfigurationsPtr DoDecodeBootstrapConfigurations(
      const std::vector<uint8_t>& data) {
    return decoder_->DoDecodeBootstrapConfigurations(data);
  }

  QuickStartDecoder* decoder() const { return decoder_.get(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::QuickStartDecoder> remote_;
  std::unique_ptr<QuickStartDecoder> decoder_;
};

TEST_F(QuickStartDecoderTest, ConvertCtapDeviceResponseCodeTest_InRange) {
  std::vector<uint8_t> credential_id = {0x01, 0x02, 0x03};
  std::vector<uint8_t> auth_data = {};
  std::vector<uint8_t> signature = {};
  std::vector<uint8_t> user_id = {};
  // kCtap2ErrActionTimeout
  uint8_t status_code = 0x3A;
  std::vector<uint8_t> data = BuildEncodedResponseData(
      credential_id, auth_data, signature, user_id, status_code);
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(data));
  EXPECT_EQ(response->ctap_device_response_code, status_code);
  EXPECT_EQ(response->status, GetAssertionStatus::kCtapResponseError);
  EXPECT_TRUE(response->credential_id.empty());
}

TEST_F(QuickStartDecoderTest, ConvertCtapDeviceRespnoseCodeTest_OutOfRange) {
  std::vector<uint8_t> credential_id = {0x01, 0x02, 0x03};
  std::vector<uint8_t> auth_data = {};
  std::vector<uint8_t> signature = {};
  std::vector<uint8_t> user_id = {};
  // Unmapped error byte
  uint8_t status_code = 0x07;
  std::vector<uint8_t> data = BuildEncodedResponseData(
      credential_id, auth_data, signature, user_id, status_code);
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(data));
  EXPECT_EQ(response->ctap_device_response_code, status_code);
  EXPECT_EQ(response->status, GetAssertionStatus::kCtapResponseError);
  EXPECT_TRUE(response->credential_id.empty());
}

TEST_F(QuickStartDecoderTest, CborDecodeGetAssertionResponse_DecoderError) {
  // UTF-8 validation should not stop at the first NUL character in the string.
  // That is, a string with an invalid byte sequence should fail UTF-8
  // validation even if the invalid character is located after one or more NUL
  // characters. Here, 0xA6 is an unexpected continuation byte.

  // Include 0x00 as first byte for kSuccess CtapDeviceResponse status.
  std::vector<uint8_t> data = {0x00, 0x63, 0x00, 0x00, 0xA6};
  int expected = kCborDecoderErrorInvalidUtf8;
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(data));
  EXPECT_EQ(response->cbor_decoder_error, expected);
  EXPECT_EQ(response->status, GetAssertionStatus::kCborDecoderError);
  EXPECT_TRUE(response->credential_id.empty());
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_EmptyResponse) {
  std::vector<uint8_t> data{};
  uint8_t expected_device_response_code = kCtap2ErrInvalidCBOR;
  int expected_decoder_error = kCborDecoderUnknownError;
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(data));
  EXPECT_EQ(response->ctap_device_response_code, expected_device_response_code);
  EXPECT_EQ(response->cbor_decoder_error, expected_decoder_error);
  EXPECT_EQ(response->status, GetAssertionStatus::kCtapResponseError);
  EXPECT_TRUE(response->credential_id.empty());
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_OnlyStatusCode) {
  std::vector<uint8_t> data{0x00};
  uint8_t expected_device_response_code = kCtap2ErrInvalidCBOR;
  int expected_decoder_error = kCborDecoderUnknownError;
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(data));
  EXPECT_EQ(response->ctap_device_response_code, expected_device_response_code);
  EXPECT_EQ(response->cbor_decoder_error, expected_decoder_error);
  EXPECT_EQ(response->status, GetAssertionStatus::kCtapResponseError);
  EXPECT_TRUE(response->credential_id.empty());
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_Valid) {
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
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(data));
  EXPECT_EQ(response->ctap_device_response_code, kSuccess);
  EXPECT_EQ(response->cbor_decoder_error, kCborDecoderNoError);
  EXPECT_EQ(response->status, GetAssertionStatus::kSuccess);
  EXPECT_EQ(response->credential_id, expected_credential_id);
  EXPECT_EQ(response->email, email);
  EXPECT_EQ(response->auth_data, auth_data);
  EXPECT_EQ(response->signature, signature);
}

TEST_F(QuickStartDecoderTest, DecodeGetAssertionResponse_ValidEmptyValues) {
  std::vector<uint8_t> credential_id = {};
  std::string expected_credential_id(credential_id.begin(),
                                     credential_id.end());
  std::vector<uint8_t> auth_data = {0x02, 0x03, 0x04};
  std::vector<uint8_t> signature = {0x03, 0x04, 0x05};
  std::string email = "";
  std::vector<uint8_t> user_id(email.begin(), email.end());
  // kSuccess
  uint8_t status = kSuccess;
  std::vector<uint8_t> data = BuildEncodedResponseData(
      credential_id, auth_data, signature, user_id, status);
  mojom::GetAssertionResponsePtr response =
      DoDecodeGetAssertionResponse(std::move(data));
  EXPECT_EQ(response->ctap_device_response_code, kSuccess);
  EXPECT_EQ(response->cbor_decoder_error, kCborDecoderNoError);
  EXPECT_EQ(response->status, GetAssertionStatus::kSuccess);
  EXPECT_EQ(response->credential_id, expected_credential_id);
  EXPECT_EQ(response->email, email);
  EXPECT_EQ(response->auth_data, auth_data);
  EXPECT_EQ(response->signature, signature);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_EmptyMessagePayload) {
  base::Value::Dict message_payload;
  std::string json_bootstrap_configuration;
  ASSERT_TRUE(
      base::JSONWriter::Write(message_payload, &json_bootstrap_configuration));
  std::vector<uint8_t> payload(json_bootstrap_configuration.begin(),
                               json_bootstrap_configuration.end());
  mojom::BootstrapConfigurationsPtr response =
      DoDecodeBootstrapConfigurations(std::move(payload));
  EXPECT_FALSE(response);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_EmptyBootstrapConfigurations) {
  base::Value::Dict bootstrap_configurations;
  base::Value::Dict message_payload;
  message_payload.Set(kBootstrapConfigurationsKey,
                      std::move(bootstrap_configurations));

  std::string json_bootstrap_configuration;
  ASSERT_TRUE(
      base::JSONWriter::Write(message_payload, &json_bootstrap_configuration));
  std::vector<uint8_t> payload(json_bootstrap_configuration.begin(),
                               json_bootstrap_configuration.end());
  mojom::BootstrapConfigurationsPtr response =
      DoDecodeBootstrapConfigurations(std::move(payload));
  EXPECT_FALSE(response);
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_EmptyDeviceDetails) {
  base::Value::Dict device_details;
  base::Value::Dict bootstrap_configurations;
  bootstrap_configurations.Set(kDeviceDetailsKey, std::move(device_details));

  base::Value::Dict message_payload;
  message_payload.Set(kBootstrapConfigurationsKey,
                      std::move(bootstrap_configurations));

  std::string json_bootstrap_configuration;
  ASSERT_TRUE(
      base::JSONWriter::Write(message_payload, &json_bootstrap_configuration));
  std::vector<uint8_t> payload(json_bootstrap_configuration.begin(),
                               json_bootstrap_configuration.end());
  mojom::BootstrapConfigurationsPtr response =
      DoDecodeBootstrapConfigurations(std::move(payload));
  EXPECT_TRUE(response);
  EXPECT_EQ(response->cryptauth_device_id, "");
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_EmptyCryptauthDeviceId) {
  base::Value::Dict device_details;
  device_details.Set(kCryptauthDeviceIdKey, "");

  base::Value::Dict bootstrap_configurations;
  bootstrap_configurations.Set(kDeviceDetailsKey, std::move(device_details));

  base::Value::Dict message_payload;
  message_payload.Set(kBootstrapConfigurationsKey,
                      std::move(bootstrap_configurations));

  std::string json_bootstrap_configuration;
  ASSERT_TRUE(
      base::JSONWriter::Write(message_payload, &json_bootstrap_configuration));
  std::vector<uint8_t> payload(json_bootstrap_configuration.begin(),
                               json_bootstrap_configuration.end());
  mojom::BootstrapConfigurationsPtr response =
      DoDecodeBootstrapConfigurations(std::move(payload));
  EXPECT_TRUE(response);
  EXPECT_EQ(response->cryptauth_device_id, "");
}

TEST_F(QuickStartDecoderTest,
       DecodeBootstrapConfigurations_ValidBootstrapConfigurations) {
  base::Value::Dict device_details;
  device_details.Set(kCryptauthDeviceIdKey, kExampleCryptauthDeviceId);

  base::Value::Dict bootstrap_configurations;
  bootstrap_configurations.Set(kDeviceDetailsKey, std::move(device_details));

  base::Value::Dict message_payload;
  message_payload.Set(kBootstrapConfigurationsKey,
                      std::move(bootstrap_configurations));

  std::string json_bootstrap_configuration;
  ASSERT_TRUE(
      base::JSONWriter::Write(message_payload, &json_bootstrap_configuration));
  std::vector<uint8_t> payload(json_bootstrap_configuration.begin(),
                               json_bootstrap_configuration.end());
  mojom::BootstrapConfigurationsPtr response =
      DoDecodeBootstrapConfigurations(std::move(payload));
  EXPECT_TRUE(response);
  EXPECT_EQ(response->cryptauth_device_id, kExampleCryptauthDeviceId);
}

}  // namespace ash::quick_start
