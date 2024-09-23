// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_candidate_store.h"

#include <string_view>

#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class LobsterCandidateStoreTest : public testing::Test {
 public:
  LobsterCandidateStoreTest() {}
  LobsterCandidateStoreTest(const LobsterCandidateStoreTest&) = delete;
  LobsterCandidateStoreTest& operator=(const LobsterCandidateStoreTest&) =
      delete;

  ~LobsterCandidateStoreTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LobsterCandidateStoreTest, CanFindImageCandidateAfterCaching) {
  LobsterCandidateStore store;

  store.Cache({.id = 123,
               .image_bytes = "a1b2c3",
               .seed = 10001,
               .query = "a sunny day"});
  store.Cache({.id = 456,
               .image_bytes = "a4b5c6",
               .seed = 10004,
               .query = "a windy weekday"});

  EXPECT_EQ(store.FindCandidateById(123).value(),
            LobsterImageCandidate(123, "a1b2c3", 10001, "a sunny day"));
  EXPECT_EQ(store.FindCandidateById(456).value(),
            LobsterImageCandidate(456, "a4b5c6", 10004, "a windy weekday"));
}

TEST_F(LobsterCandidateStoreTest, ReturnsEmptyCandidateIfIdDoesNotMatch) {
  LobsterCandidateStore store;

  store.Cache({.id = 123,
               .image_bytes = "a1b2c3",
               .seed = 10001,
               .query = "a sunny day"});
  store.Cache({.id = 456,
               .image_bytes = "a4b5c6",
               .seed = 10004,
               .query = "a windy weekday"});

  EXPECT_FALSE(store.FindCandidateById(120).has_value());
  EXPECT_FALSE(store.FindCandidateById(505).has_value());
}

TEST_F(LobsterCandidateStoreTest, CacheOverridesPreviouslyCachedCandidate) {
  LobsterCandidateStore store;

  store.Cache({.id = 123,
               .image_bytes = "a1b2c3",
               .seed = 10001,
               .query = "a sunny day"});
  store.Cache({.id = 456,
               .image_bytes = "a4b5c6",
               .seed = 10004,
               .query = "a windy weekday"});

  EXPECT_EQ(store.FindCandidateById(123).value(),
            LobsterImageCandidate(123, "a1b2c3", 10001, "a sunny day"));

  store.Cache({.id = 123,
               .image_bytes = "x8y9z0",
               .seed = 10011,
               .query = "a starry night"});

  EXPECT_EQ(store.FindCandidateById(123).value(),
            LobsterImageCandidate(123, "x8y9z0", 10011, "a starry night"));
}

}  // namespace

}  // namespace ash
