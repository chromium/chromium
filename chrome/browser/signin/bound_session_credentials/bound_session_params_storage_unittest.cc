// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/protobuf_matchers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace {

bound_session_credentials::BoundSessionParams CreateValidBoundSessionParams() {
  bound_session_credentials::BoundSessionParams params;
  params.set_session_id("123");
  params.set_site("https://example.org");
  params.set_wrapped_key("456");

  bound_session_credentials::CookieCredential* cookie =
      params.add_credentials()->mutable_cookie_credential();
  cookie->set_name("auth_cookie");
  cookie->set_domain(".example.org");
  cookie->set_path("/");
  return params;
}

bound_session_credentials::BoundSessionParams
CreateInvalidBoundSessionParams() {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  // Removes a required `wrapped_key` field empty.
  params.clear_wrapped_key();
  return params;
}

MATCHER(TupleEqualsProto, "") {
  return testing::ExplainMatchResult(base::EqualsProto(std::get<1>(arg)),
                                     std::get<0>(arg), result_listener);
}

}  // namespace

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
  EXPECT_THAT(storage().ReadAllParams(), testing::IsEmpty());
}

TEST_P(BoundSessionParamsStorageTest, SaveAndRead) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  ASSERT_TRUE(storage().SaveParams(params));
  EXPECT_THAT(storage().ReadAllParams(),
              testing::Pointwise(TupleEqualsProto(), {params}));
}

TEST_P(BoundSessionParamsStorageTest, SaveInvalidParams) {
  EXPECT_FALSE(storage().SaveParams(CreateInvalidBoundSessionParams()));
  EXPECT_THAT(storage().ReadAllParams(), testing::IsEmpty());
}

TEST_P(BoundSessionParamsStorageTest, OverwriteWithValidParams) {
  ASSERT_TRUE(storage().SaveParams(CreateValidBoundSessionParams()));
  bound_session_credentials::BoundSessionParams new_params =
      CreateValidBoundSessionParams();
  new_params.set_wrapped_key("new_wrapped_key");
  EXPECT_TRUE(storage().SaveParams(new_params));
  EXPECT_THAT(storage().ReadAllParams(),
              testing::Pointwise(TupleEqualsProto(), {new_params}));
}

TEST_P(BoundSessionParamsStorageTest, OverwriteWithInvalidParams) {
  bound_session_credentials::BoundSessionParams valid_params =
      CreateValidBoundSessionParams();
  ASSERT_TRUE(storage().SaveParams(valid_params));
  EXPECT_FALSE(storage().SaveParams(CreateInvalidBoundSessionParams()));
  EXPECT_THAT(storage().ReadAllParams(),
              testing::Pointwise(TupleEqualsProto(), {valid_params}));
}

TEST_P(BoundSessionParamsStorageTest, SaveMultipleParamsSameSite) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params;
  for (size_t i = 0; i < 3; ++i) {
    bound_session_credentials::BoundSessionParams params =
        CreateValidBoundSessionParams();
    params.set_session_id(base::StrCat({"session_", base::NumberToString(i)}));
    EXPECT_TRUE(storage().SaveParams(params));
    all_params.push_back(std::move(params));
  }
  EXPECT_THAT(storage().ReadAllParams(),
              testing::UnorderedPointwise(TupleEqualsProto(), all_params));
}

TEST_P(BoundSessionParamsStorageTest, SaveMultipleParamsDifferentSites) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params;
  for (size_t i = 0; i < 3; ++i) {
    bound_session_credentials::BoundSessionParams params =
        CreateValidBoundSessionParams();
    params.set_site(base::StrCat(
        {"https://domain", base::NumberToString(i), ".example.org"}));
    EXPECT_TRUE(storage().SaveParams(params));
    all_params.push_back(std::move(params));
  }
  EXPECT_THAT(storage().ReadAllParams(),
              testing::UnorderedPointwise(TupleEqualsProto(), all_params));
}

TEST_P(BoundSessionParamsStorageTest, Clear) {
  const std::string kSite = "https://mydomain.example.org";
  const std::string kSessionId = "my_session";
  bound_session_credentials::BoundSessionParams params_to_be_removed =
      CreateValidBoundSessionParams();
  params_to_be_removed.set_site(kSite);
  params_to_be_removed.set_session_id(kSessionId);

  // Populate storage with params matching by either a site or a session_id.
  std::vector<bound_session_credentials::BoundSessionParams> expected_params;
  for (size_t i = 0; i < 3; ++i) {
    bound_session_credentials::BoundSessionParams same_site_params =
        CreateValidBoundSessionParams();
    same_site_params.set_site(kSite);
    same_site_params.set_session_id(
        base::StrCat({"session_", base::NumberToString(i)}));
    ASSERT_TRUE(storage().SaveParams(same_site_params));
    expected_params.push_back(std::move(same_site_params));
  }
  for (size_t i = 0; i < 3; ++i) {
    bound_session_credentials::BoundSessionParams same_session_id_params =
        CreateValidBoundSessionParams();
    same_session_id_params.set_session_id(kSessionId);
    same_session_id_params.set_site(base::StrCat(
        {"https://domain", base::NumberToString(i), ".example.org"}));
    EXPECT_TRUE(storage().SaveParams(same_session_id_params));
    expected_params.push_back(std::move(same_session_id_params));
  }
  // Add `params_to_be_removed` last to easily `pop_back()` later.
  ASSERT_TRUE(storage().SaveParams(params_to_be_removed));
  expected_params.push_back(params_to_be_removed);
  // Verify that the storage is populated as expected.
  ASSERT_THAT(storage().ReadAllParams(),
              testing::UnorderedPointwise(TupleEqualsProto(), expected_params));

  EXPECT_TRUE(storage().ClearParams(params_to_be_removed.site(),
                                    params_to_be_removed.session_id()));
  // Removes `params_to_be_removed`.
  expected_params.pop_back();

  EXPECT_THAT(storage().ReadAllParams(),
              testing::UnorderedPointwise(TupleEqualsProto(), expected_params));
}

TEST_P(BoundSessionParamsStorageTest, ClearNonExisting) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  params.set_session_id("my_session_id");
  ASSERT_TRUE(storage().SaveParams(params));
  bound_session_credentials::BoundSessionParams other_params = params;
  other_params.set_session_id("other_session_id");

  EXPECT_FALSE(
      storage().ClearParams(other_params.site(), other_params.session_id()));

  EXPECT_THAT(storage().ReadAllParams(),
              testing::Pointwise(TupleEqualsProto(), {params}));
}

TEST_P(BoundSessionParamsStorageTest, ClearAll) {
  ASSERT_TRUE(storage().SaveParams(CreateValidBoundSessionParams()));
  storage().ClearAllParams();
  EXPECT_THAT(storage().ReadAllParams(), testing::IsEmpty());
}

TEST_P(BoundSessionParamsStorageTest, Persistence) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  ASSERT_TRUE(storage().SaveParams(params));
  EXPECT_THAT(storage().ReadAllParams(), testing::Not(testing::IsEmpty()));

  ResetStorage();

  if (IsOffTheRecord()) {
    EXPECT_THAT(storage().ReadAllParams(), testing::IsEmpty());
  } else {
    EXPECT_THAT(storage().ReadAllParams(),
                testing::Pointwise(TupleEqualsProto(), {params}));
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
  EXPECT_THAT(parent_storage->ReadAllParams(),
              testing::Not(testing::IsEmpty()));

  std::unique_ptr<BoundSessionParamsStorage> otr_storage =
      BoundSessionParamsStorage::CreateForProfile(
          *parent_profile().GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_THAT(otr_storage->ReadAllParams(), testing::IsEmpty());
  bound_session_credentials::BoundSessionParams params2 =
      CreateValidBoundSessionParams();
  params2.set_session_id("otr_session");
  ASSERT_TRUE(otr_storage->SaveParams(params2));
  EXPECT_THAT(otr_storage->ReadAllParams(),
              testing::Pointwise(TupleEqualsProto(), {params2}));

  // Parent storage hasn't changed.
  EXPECT_THAT(parent_storage->ReadAllParams(),
              testing::Pointwise(TupleEqualsProto(), {params}));
}
