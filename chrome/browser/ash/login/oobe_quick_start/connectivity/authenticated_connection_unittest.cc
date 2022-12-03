// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/authenticated_connection.h"

#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

namespace {
const char kBootstrapOptionsKey[] = "bootstrapOptions";
const char kAccountRequirementKey[] = "accountRequirement";
const char kFlowTypeKey[] = "flowType";
const int kAccountRequirementSingle = 2;
const int kFlowTypeTargetChallenge = 2;
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

 protected:
  std::unique_ptr<AuthenticatedConnection> authenticated_connection_;
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(AuthenticatedConnectionTest, SendBootstrapOptions) {
  authenticated_connection_->RequestAccountTransferAssertion();
  std::vector<uint8_t> written_payload =
      fake_nearby_connection_->GetWrittenData();
  std::string written_payload_string(written_payload.begin(),
                                     written_payload.end());
  absl::optional<base::Value> parsed_json =
      base::JSONReader::Read(written_payload_string);
  ASSERT_TRUE(parsed_json);
  ASSERT_TRUE(parsed_json->is_dict());
  base::Value::Dict& parsed_json_dict = parsed_json.value().GetDict();
  base::Value::Dict& bootstrap_options =
      *parsed_json_dict.FindDict(kBootstrapOptionsKey);
  EXPECT_EQ(*bootstrap_options.FindInt(kAccountRequirementKey),
            kAccountRequirementSingle);
  EXPECT_EQ(*bootstrap_options.FindInt(kFlowTypeKey), kFlowTypeTargetChallenge);
}

}  // namespace ash::quick_start
