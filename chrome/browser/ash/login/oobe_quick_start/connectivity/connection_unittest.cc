// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "connection.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

namespace {

constexpr char kTestMessagePayloadKey[] = "bootstrapOptions";
constexpr char kTestMessagePayloadValue[] = "testValue";
constexpr char kTestBytes[] = "testbytes";

// 32 random bytes to use as the shared secret.
constexpr std::array<uint8_t, 32> kSharedSecret = {
    0x54, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

// 6 random bytes to use as the RandomSessionId.
constexpr std::array<uint8_t, 6> kRandomSessionId = {0x6b, 0xb3, 0x85,
                                                     0x27, 0xbb, 0x28};

class FakeConnection : public Connection {
 public:
  explicit FakeConnection(NearbyConnection* nearby_connection,
                          base::OnceClosure on_connection_closed)
      : Connection(nearby_connection,
                   RandomSessionId(kRandomSessionId),
                   kSharedSecret,
                   std::move(on_connection_closed)) {}

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
    connection_ =
        std::make_unique<FakeConnection>(nearby_connection, base::DoNothing());
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

TEST_F(ConnectionTest, TestDisconnectionTriggersListener) {
  base::test::TestFuture<void> future;
  std::unique_ptr<Connection> connection_under_test =
      std::make_unique<FakeConnection>(fake_nearby_connection_.get(),
                                       future.GetCallback());

  ASSERT_FALSE(future.IsReady());

  fake_nearby_connection_->Close();

  ASSERT_TRUE(future.IsReady());
}

}  // namespace ash::quick_start
