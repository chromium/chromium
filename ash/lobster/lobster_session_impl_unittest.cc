// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session_impl.h"

#include <string_view>

#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class MockLobsterClient : public LobsterClient {
 public:
  MockLobsterClient() {}

  MockLobsterClient(const MockLobsterClient&) = delete;
  MockLobsterClient& operator=(const MockLobsterClient&) = delete;

  ~MockLobsterClient() override = default;

  MOCK_METHOD(void, SetActiveSession, (LobsterSession * session), (override));
  MOCK_METHOD(LobsterSystemState, GetSystemState, (), (override));
  MOCK_METHOD(void,
              RequestCandidates,
              (std::string_view query,
               int num_candidates,
               RequestCandidatesCallback),
              (override));
};

class LobsterSessionImplTest : public testing::Test {
 public:
  LobsterSessionImplTest() {}
  LobsterSessionImplTest(const LobsterSessionImplTest&) = delete;
  LobsterSessionImplTest& operator=(const LobsterSessionImplTest&) = delete;

  ~LobsterSessionImplTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LobsterSessionImplTest, RequestCandidatesWithTwoResults) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  EXPECT_CALL(*lobster_client,
              RequestCandidates(/*query=*/"a nice strawberry",
                                /*num_candidates=*/3, testing::_))
      .WillOnce(testing::Invoke([](std::string_view query, int num_candidates,
                                   RequestCandidatesCallback done_callback) {
        std::vector<LobsterImageCandidate> image_candidates = {
            LobsterImageCandidate(/*id=*/0, /*image_bytes=*/"a1b2c3"),
            LobsterImageCandidate(/*id=*/1, /*image_bytes=*/"d4e5f6"),
            LobsterImageCandidate(/*id=*/2, /*image_bytes=*/"g7h8i9")};
        std::move(done_callback).Run(std::move(image_candidates));
      }));

  LobsterSessionImpl session(std::move(lobster_client));

  base::test::TestFuture<const std::vector<LobsterImageCandidate>&> future;

  session.RequestCandidates(/*query=*/"a nice strawberry", /*num_candidates=*/3,
                            future.GetCallback());

  EXPECT_THAT(future.Get(),
              testing::ElementsAre(
                  LobsterImageCandidate(/*expected_id=*/0,
                                        /*expected_image_bytes=*/"a1b2c3"),
                  LobsterImageCandidate(/*expected_id=*/1,
                                        /*expected_image_bytes=*/"d4e5f6"),
                  LobsterImageCandidate(/*expected_id=*/2,
                                        /*expected_image_bytes=*/"g7h8i9")));
}

TEST_F(LobsterSessionImplTest, RequestCandidatesWithEmptyResults) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  EXPECT_CALL(*lobster_client,
              RequestCandidates(/*query=*/"a nice blueberry",
                                /*num_candidates=*/1, testing::_))
      .WillOnce(testing::Invoke([](std::string_view query, int num_candidates,
                                   RequestCandidatesCallback done_callback) {
        std::move(done_callback).Run({});
      }));

  LobsterSessionImpl session(std::move(lobster_client));

  base::test::TestFuture<const std::vector<LobsterImageCandidate>&> future;

  session.RequestCandidates(/*query=*/"a nice blueberry", /*num_candidates=*/1,
                            future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

}  // namespace

}  // namespace ash
