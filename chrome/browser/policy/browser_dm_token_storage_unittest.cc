// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/policy/fake_browser_dm_token_storage.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;

namespace policy {

namespace {

constexpr char kClientId1[] = "fake-client-id-1";
constexpr char kClientId2[] = "fake-client-id-2";
constexpr char kEnrollmentToken1[] = "fake-enrollment-token-1";
constexpr char kEnrollmentToken2[] = "fake-enrollment-token-2";
constexpr char kDMToken1[] = "fake-dm-token-1";
constexpr char kDMToken2[] = "fake-dm-token-2";

class BrowserDMTokenStorageTestBase {
 public:
  BrowserDMTokenStorageTestBase(const std::string& client_id,
                                const std::string& enrollment_token,
                                const std::string& dm_token,
                                const bool enrollment_error_option)
      : storage_(client_id,
                 enrollment_token,
                 dm_token,
                 enrollment_error_option) {}
  FakeBrowserDMTokenStorage storage_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

class BrowserDMTokenStorageTest : public BrowserDMTokenStorageTestBase,
                                  public testing::Test {
 public:
  BrowserDMTokenStorageTest()
      : BrowserDMTokenStorageTestBase(kClientId1,
                                      kEnrollmentToken1,
                                      kDMToken1,
                                      false) {}
};

struct StoreAndRetrieveTestParams {
 public:
  StoreAndRetrieveTestParams(const std::string& dm_token_to_store,
                             const std::string& expected_retrieved_dm_token,
                             bool expect_valid,
                             bool expect_invalid,
                             bool expect_empty)
      : dm_token_to_store(dm_token_to_store),
        expected_retrieved_dm_token(expected_retrieved_dm_token),
        expect_valid(expect_valid),
        expect_invalid(expect_invalid),
        expect_empty(expect_empty) {}

  std::string dm_token_to_store;
  std::string expected_retrieved_dm_token;
  bool expect_valid;
  bool expect_invalid;
  bool expect_empty;
};

class BrowserDMTokenStorageStoreAndRetrieveTest
    : public BrowserDMTokenStorageTestBase,
      public testing::TestWithParam<StoreAndRetrieveTestParams> {
 public:
  BrowserDMTokenStorageStoreAndRetrieveTest()
      : BrowserDMTokenStorageTestBase(kClientId1,
                                      kEnrollmentToken1,
                                      GetParam().dm_token_to_store,
                                      false) {}
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    BrowserDMTokenStorageStoreAndRetrieveTest,
    BrowserDMTokenStorageStoreAndRetrieveTest,
    testing::Values(
        StoreAndRetrieveTestParams(kDMToken1, kDMToken1, true, false, false),
        StoreAndRetrieveTestParams(kDMToken2, kDMToken2, true, false, false),
        StoreAndRetrieveTestParams("INVALID_DM_TOKEN", "", false, true, false),
        StoreAndRetrieveTestParams("", "", false, false, true)));

TEST_F(BrowserDMTokenStorageTest, RetrieveClientId) {
  EXPECT_EQ(kClientId1, storage_.RetrieveClientId());
  // The client ID value should be cached in memory and not read from the system
  // again.
  storage_.SetClientId(kClientId2);
  EXPECT_EQ(kClientId1, storage_.RetrieveClientId());
}

TEST_F(BrowserDMTokenStorageTest, RetrieveEnrollmentToken) {
  EXPECT_EQ(kEnrollmentToken1, storage_.RetrieveEnrollmentToken());

  // The enrollment token should be cached in memory and not read from the
  // system again.
  storage_.SetEnrollmentToken(kEnrollmentToken2);
  EXPECT_EQ(kEnrollmentToken1, storage_.RetrieveEnrollmentToken());
}

TEST_P(BrowserDMTokenStorageStoreAndRetrieveTest, StoreDMToken) {
  storage_.SetDMToken(GetParam().dm_token_to_store);
  auto dm_token = storage_.RetrieveBrowserDMToken();
  if (GetParam().expect_valid || GetParam().expect_empty) {
    EXPECT_EQ(GetParam().expected_retrieved_dm_token, dm_token.value());
  }
  EXPECT_EQ(GetParam().expect_valid, dm_token.is_valid());
  EXPECT_EQ(GetParam().expect_invalid, dm_token.is_invalid());
  EXPECT_EQ(GetParam().expect_empty, dm_token.is_empty());

  // The DM token should be cached in memory and not read from the system again.
  storage_.SetDMToken("not_saved");
  if (GetParam().expect_valid || GetParam().expect_empty) {
    EXPECT_EQ(GetParam().expected_retrieved_dm_token, dm_token.value());
  }
}

TEST_P(BrowserDMTokenStorageStoreAndRetrieveTest, RetrieveDMToken) {
  auto dm_token = storage_.RetrieveBrowserDMToken();
  if (GetParam().expect_valid || GetParam().expect_empty) {
    EXPECT_EQ(GetParam().expected_retrieved_dm_token, dm_token.value());
  }
  EXPECT_EQ(GetParam().expect_valid, dm_token.is_valid());
  EXPECT_EQ(GetParam().expect_invalid, dm_token.is_invalid());
  EXPECT_EQ(GetParam().expect_empty, dm_token.is_empty());
}

TEST_F(BrowserDMTokenStorageTest, ShouldDisplayErrorMessageOnFailure) {
  EXPECT_FALSE(storage_.ShouldDisplayErrorMessageOnFailure());

  // The error option should be cached in memory and not read from the system
  // again.
  storage_.SetEnrollmentErrorOption(true);
  EXPECT_FALSE(storage_.ShouldDisplayErrorMessageOnFailure());
}

}  // namespace policy
