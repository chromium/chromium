// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

bound_session_credentials::BoundSessionParams CreateValidBoundSessionParams() {
  bound_session_credentials::BoundSessionParams params;
  params.set_session_id("123");
  params.set_site("https://example.org");
  params.set_wrapped_key("456");
  return params;
}

bound_session_credentials::BoundSessionParams
CreateInvalidBoundSessionParams() {
  bound_session_credentials::BoundSessionParams params;
  // Leaves a required `session_id` field empty.
  return params;
}

MATCHER_P(EqualsProto, message, "") {
  return message.SerializeAsString() == arg.SerializeAsString();
}

}  // namespace

TEST(BoundSessionParamsStorageAreParamsValidTest, Valid) {
  EXPECT_TRUE(BoundSessionParamsStorage::AreParamsValid(
      CreateValidBoundSessionParams()));
}

TEST(BoundSessionParamsStorageAreParamsValidTest, InvalidMissingSessionId) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  params.clear_session_id();
  EXPECT_FALSE(BoundSessionParamsStorage::AreParamsValid(params));
}

TEST(BoundSessionParamsStorageAreParamsValidTest, InvalidMissingWrappedKey) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  params.clear_wrapped_key();
  EXPECT_FALSE(BoundSessionParamsStorage::AreParamsValid(params));
}

class BoundSessionParamsStorageTest : public testing::TestWithParam<bool> {
 public:
  BoundSessionParamsStorageTest() : storage_(CreateStorage()) {}

  bool IsOffTheRecord() { return GetParam(); }

  BoundSessionParamsStorage& storage() { return *storage_; }

  void ResetStorage() { storage_ = CreateStorage(); }

 private:
  std::unique_ptr<BoundSessionParamsStorage> CreateStorage() {
    return BoundSessionParamsStorage::CreateForProfile(
        IsOffTheRecord()
            ? *profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true)
            : profile_);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BoundSessionParamsStorage> storage_;
};

TEST_P(BoundSessionParamsStorageTest, InitiallyEmpty) {
  EXPECT_EQ(storage().ReadParams(), absl::nullopt);
}

TEST_P(BoundSessionParamsStorageTest, SaveAndRead) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  ASSERT_TRUE(storage().SaveParams(params));
  EXPECT_THAT(storage().ReadParams(), testing::Optional(EqualsProto(params)));
}

TEST_P(BoundSessionParamsStorageTest, SaveInvalidParams) {
  EXPECT_FALSE(storage().SaveParams(CreateInvalidBoundSessionParams()));
  EXPECT_EQ(storage().ReadParams(), absl::nullopt);
}

TEST_P(BoundSessionParamsStorageTest, OverwriteWithValidParams) {
  ASSERT_TRUE(storage().SaveParams(CreateValidBoundSessionParams()));
  bound_session_credentials::BoundSessionParams new_params =
      CreateValidBoundSessionParams();
  new_params.set_session_id("unique_id");
  EXPECT_TRUE(storage().SaveParams(new_params));
  EXPECT_THAT(storage().ReadParams(),
              testing::Optional(EqualsProto(new_params)));
}

TEST_P(BoundSessionParamsStorageTest, OverwriteWithInvalidParams) {
  bound_session_credentials::BoundSessionParams valid_params =
      CreateValidBoundSessionParams();
  ASSERT_TRUE(storage().SaveParams(valid_params));
  EXPECT_FALSE(storage().SaveParams(CreateInvalidBoundSessionParams()));
  EXPECT_THAT(storage().ReadParams(),
              testing::Optional(EqualsProto(valid_params)));
}

TEST_P(BoundSessionParamsStorageTest, Clear) {
  ASSERT_TRUE(storage().SaveParams(CreateValidBoundSessionParams()));
  storage().ClearParams();
  EXPECT_EQ(storage().ReadParams(), absl::nullopt);
}

TEST_P(BoundSessionParamsStorageTest, Persistence) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  ASSERT_TRUE(storage().SaveParams(params));
  EXPECT_TRUE(storage().ReadParams().has_value());

  ResetStorage();

  if (IsOffTheRecord()) {
    EXPECT_EQ(storage().ReadParams(), absl::nullopt);
  } else {
    EXPECT_THAT(storage().ReadParams(), testing::Optional(EqualsProto(params)));
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         BoundSessionParamsStorageTest,
                         testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "OTR" : "Persistent";
                         });

class BoundSessionParamsStorageOTRTest : public testing::Test {
 public:
  TestingProfile& parent_profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// Tests that an OTR profile storage isn't affected by the contents of the
// parent storage.
TEST_F(BoundSessionParamsStorageOTRTest, NoInheritance) {
  std::unique_ptr<BoundSessionParamsStorage> parent_storage =
      BoundSessionParamsStorage::CreateForProfile(parent_profile());
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  ASSERT_TRUE(parent_storage->SaveParams(params));
  EXPECT_TRUE(parent_storage->ReadParams().has_value());

  std::unique_ptr<BoundSessionParamsStorage> otr_storage =
      BoundSessionParamsStorage::CreateForProfile(
          *parent_profile().GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_EQ(otr_storage->ReadParams(), absl::nullopt);
  bound_session_credentials::BoundSessionParams params2 =
      CreateValidBoundSessionParams();
  params2.set_session_id("otr_session");
  ASSERT_TRUE(otr_storage->SaveParams(params2));
  EXPECT_THAT(otr_storage->ReadParams(),
              testing::Optional(EqualsProto(params2)));

  // Parent storage hasn't changed.
  EXPECT_THAT(parent_storage->ReadParams(),
              testing::Optional(EqualsProto(params)));
}
