// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_client_base.h"

#include "base/json/json_reader.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

namespace {

const char kTestMessagePayloadKey[] = "bootstrapOptions";
const char kTestMessagePayloadValue[] = "testValue";
const char kTestBytes[] = "testbytes";

class TestTargetDeviceClient : public TargetDeviceClientBase {
 public:
  explicit TestTargetDeviceClient(NearbyConnection* nearby_connection,
                                  QuickStartDecoder* quick_start_decoder)
      : TargetDeviceClientBase(nearby_connection, quick_start_decoder) {}

  bool OnDataReadCalled() { return on_data_read_called_; }

 private:
  void OnDataRead(absl::optional<std::vector<uint8_t>> data) override {
    on_data_read_called_ = true;
  }

  bool on_data_read_called_ = false;
};

}  // namespace

class TargetDeviceClientBaseTest : public testing::Test {
 public:
  TargetDeviceClientBaseTest() = default;
  TargetDeviceClientBaseTest(TargetDeviceClientBaseTest&) = delete;
  TargetDeviceClientBaseTest& operator=(TargetDeviceClientBaseTest&) = delete;
  ~TargetDeviceClientBaseTest() override = default;

  void SetUp() override {
    fake_nearby_connection_ = std::make_unique<FakeNearbyConnection>();
    NearbyConnection* nearby_connection = fake_nearby_connection_.get();
    CreateDeviceClient(nearby_connection);
  }

  void CreateDeviceClient(NearbyConnection* nearby_connection) {
    test_target_device_client_ =
        std::make_unique<TestTargetDeviceClient>(nearby_connection, nullptr);
  }

  void SendPayload(const base::Value::Dict& message_payload) {
    test_target_device_client_->SendPayload(message_payload);
  }

 protected:
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
  std::unique_ptr<TestTargetDeviceClient> test_target_device_client_;
};

TEST_F(TargetDeviceClientBaseTest, SendPayload) {
  base::Value::Dict message_payload;
  message_payload.Set(kTestMessagePayloadKey, kTestMessagePayloadValue);
  SendPayload(message_payload);
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

TEST_F(TargetDeviceClientBaseTest, SendPayload_EmptyPayload) {
  base::Value::Dict message_payload;
  SendPayload(message_payload);
  std::vector<uint8_t> written_payload =
      fake_nearby_connection_->GetWrittenData();
  std::string written_payload_string(written_payload.begin(),
                                     written_payload.end());
  EXPECT_TRUE(written_payload_string == "{}");
}

TEST_F(TargetDeviceClientBaseTest, OnDataRead) {
  std::vector<uint8_t> test_bytes(std::begin(kTestBytes), std::end(kTestBytes));
  fake_nearby_connection_->AppendReadableData(test_bytes);
  base::Value::Dict message_payload;
  // SendPayload will register the OnDataRead() to the read callback. This means
  // that OnDataRead() will be called as a callback when there is readable data
  // in the Nearby Connection.
  SendPayload(message_payload);
  EXPECT_TRUE(test_target_device_client_->OnDataReadCalled());
}

}  // namespace ash::quick_start
