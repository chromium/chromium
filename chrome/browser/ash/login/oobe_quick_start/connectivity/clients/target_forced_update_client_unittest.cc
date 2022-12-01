// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/clients/target_forced_update_client.h"

#include "base/json/json_reader.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

namespace {
const char kNotifySourceOfUpdateMessageKey[] = "isForcedUpdateRequired";

}  // namespace

class TargetForcedUpdateClientTest : public testing::Test {
 public:
  TargetForcedUpdateClientTest() = default;
  TargetForcedUpdateClientTest(TargetForcedUpdateClientTest&) = delete;
  TargetForcedUpdateClientTest& operator=(TargetForcedUpdateClientTest&) =
      delete;
  ~TargetForcedUpdateClientTest() override = default;

  void SetUp() override {
    fake_nearby_connection_ = std::make_unique<FakeNearbyConnection>();
    NearbyConnection* nearby_connection = fake_nearby_connection_.get();
    target_forced_update_client_ =
        std::make_unique<TargetForcedUpdateClient>(nearby_connection, nullptr);
  }

 protected:
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
  std::unique_ptr<TargetForcedUpdateClient> target_forced_update_client_;
};

TEST_F(TargetForcedUpdateClientTest, NotifySourceOfUpdate) {
  target_forced_update_client_->NotifySourceOfUpdate();

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
