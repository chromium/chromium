// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "connection.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

namespace {

constexpr char kTestMessagePayloadKey[] = "bootstrapOptions";
constexpr char kTestMessagePayloadValue[] = "testValue";
constexpr char kTestBytes[] = "testbytes";

class FakeConnection : public Connection {
 public:
  explicit FakeConnection(NearbyConnection* nearby_connection)
      : Connection(nearby_connection) {}

  void SendPayloadAndReadResponseWrapperForTesting(
      const base::Value::Dict& message_payload,
      PayloadResponseCallback callback) {
    SendPayloadAndReadResponse(message_payload, std::move(callback));
  }
};

}  // namespace

class ConnectionTest : public testing::Test {
 public:
  ConnectionTest() = default;
  ConnectionTest(ConnectionTest&) = delete;
  ConnectionTest& operator=(ConnectionTest&) = delete;
  ~ConnectionTest() override = default;

  void SetUp() override {
    fake_nearby_connection_ = std::make_unique<FakeNearbyConnection>();
    NearbyConnection* nearby_connection = fake_nearby_connection_.get();
    connection_ = std::make_unique<FakeConnection>(nearby_connection);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
  std::unique_ptr<FakeConnection> connection_;
};

TEST_F(ConnectionTest, SendPayloadAndReadResponse) {
  base::Value::Dict message_payload;
  message_payload.Set(kTestMessagePayloadKey, kTestMessagePayloadValue);
  fake_nearby_connection_->AppendReadableData(
      std::vector<uint8_t>(std::begin(kTestBytes), std::end(kTestBytes)));

  base::RunLoop run_loop;
  connection_->SendPayloadAndReadResponseWrapperForTesting(
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

}  // namespace ash::quick_start
