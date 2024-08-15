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
              (const std::string& query,
               int num_candidates,
               RequestCandidatesCallback),
              (override));
  MOCK_METHOD(void,
              InflateCandidate,
              (uint32_t seed,
               const std::string& query,
               InflateCandidateCallback),
              (override));
};

class LobsterSessionImplTest : public testing::Test {
 public:
  LobsterSessionImplTest() {}
  LobsterSessionImplTest(const LobsterSessionImplTest&) = delete;
  LobsterSessionImplTest& operator=(const LobsterSessionImplTest&) = delete;

  ~LobsterSessionImplTest() override = default;

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LobsterSessionImplTest, RequestCandidatesWithThreeResults) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  EXPECT_CALL(*lobster_client,
              RequestCandidates(/*query=*/"a nice strawberry",
                                /*num_candidates=*/3, testing::_))
      .WillOnce(testing::Invoke([](std::string_view query, int num_candidates,
                                   RequestCandidatesCallback done_callback) {
        std::vector<LobsterImageCandidate> image_candidates = {
            LobsterImageCandidate(/*id=*/0, /*image_bytes=*/"a1b2c3",
                                  /*seed=*/20,
                                  /*query=*/"a nice strawberry"),
            LobsterImageCandidate(/*id=*/1, /*image_bytes=*/"d4e5f6",
                                  /*seed=*/21,
                                  /*query=*/"a nice strawberry"),
            LobsterImageCandidate(/*id=*/2, /*image_bytes=*/"g7h8i9",
                                  /*seed=*/22,
                                  /*query=*/"a nice strawberry")};
        std::move(done_callback).Run(std::move(image_candidates));
      }));

  LobsterSessionImpl session(std::move(lobster_client));

  base::test::TestFuture<const LobsterResult&> future;

  session.RequestCandidates(/*query=*/"a nice strawberry", /*num_candidates=*/3,
                            future.GetCallback());

  EXPECT_THAT(
      future.Get().value(),
      testing::ElementsAre(
          LobsterImageCandidate(/*expected_id=*/0,
                                /*expected_image_bytes=*/"a1b2c3",
                                /*seed=*/20, /*query=*/"a nice strawberry"),
          LobsterImageCandidate(/*expected_id=*/1,
                                /*expected_image_bytes=*/"d4e5f6",
                                /*seed=*/21, /*query=*/"a nice strawberry"),
          LobsterImageCandidate(/*expected_id=*/2,
                                /*expected_image_bytes=*/"g7h8i9",
                                /*seed=*/22, /*query=*/"a nice strawberry")));
}

TEST_F(LobsterSessionImplTest, RequestCandidatesReturnsUnknownError) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  EXPECT_CALL(*lobster_client,
              RequestCandidates(/*query=*/"a nice blueberry",
                                /*num_candidates=*/1, testing::_))
      .WillOnce(testing::Invoke([](std::string_view query, int num_candidates,
                                   RequestCandidatesCallback done_callback) {
        std::move(done_callback)
            .Run(base::unexpected(
                LobsterError(LobsterErrorCode::kUnknown, "unknown error")));
      }));

  LobsterSessionImpl session(std::move(lobster_client));

  base::test::TestFuture<const LobsterResult&> future;

  session.RequestCandidates(/*query=*/"a nice blueberry", /*num_candidates=*/1,
                            future.GetCallback());

  EXPECT_EQ(future.Get().error(),
            LobsterError(LobsterErrorCode::kUnknown, "unknown error"));
}

TEST_F(LobsterSessionImplTest, CanNotDownloadACandidateBeforeAnyRequest) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  ON_CALL(*lobster_client, InflateCandidate(1, testing::_, testing::_))
      .WillByDefault([](uint32_t seed, std::string_view query,
                        InflateCandidateCallback done_callback) {
        std::vector<LobsterImageCandidate> inflated_candidates = {
            LobsterImageCandidate(1, "a1b2c3", 30, "a nice raspberry")};
        std::move(done_callback).Run(inflated_candidates);
      });

  LobsterSessionImpl session(std::move(lobster_client));

  base::test::TestFuture<bool> future;
  session.DownloadCandidate(/*id=*/1, future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(LobsterSessionImplTest,
       CanNotDownloadACandidateIfNotAvailableInPastRequest) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  ON_CALL(*lobster_client,
          RequestCandidates("a nice strawberry", 2, testing::_))
      .WillByDefault([](std::string_view query, int num_candidates,
                        RequestCandidatesCallback done_callback) {
        std::vector<LobsterImageCandidate> image_candidates = {
            LobsterImageCandidate(/*id=*/0, /*image_bytes=*/"a1b2c3",
                                  /*seed=*/20,
                                  /*query=*/"a nice strawberry"),
            LobsterImageCandidate(/*id=*/1, /*image_bytes=*/"d4e5f6",
                                  /*seed=*/21,
                                  /*query=*/"a nice strawberry")};
        std::move(done_callback).Run(std::move(image_candidates));
      });

  LobsterSessionImpl session(std::move(lobster_client));
  session.RequestCandidates("a nice strawberry", 2,
                            base::BindOnce([](const LobsterResult&) {}));
  RunUntilIdle();

  base::test::TestFuture<bool> future;

  session.DownloadCandidate(/*id=*/3, future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(LobsterSessionImplTest, CanDownloadACandiateIfIdAvailableInPastRequest) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  ON_CALL(*lobster_client,
          RequestCandidates("a nice strawberry", 2, testing::_))
      .WillByDefault([](std::string_view query, int num_candidates,
                        RequestCandidatesCallback done_callback) {
        std::vector<LobsterImageCandidate> image_candidates = {
            LobsterImageCandidate(/*id=*/0, /*image_bytes=*/"a1b2c3",
                                  /*seed=*/20,
                                  /*query=*/"a nice strawberry"),
            LobsterImageCandidate(/*id=*/1, /*image_bytes=*/"d4e5f6",
                                  /*seed=*/21,
                                  /*query=*/"a nice strawberry")};

        std::move(done_callback).Run(std::move(image_candidates));
      });

  ON_CALL(*lobster_client,
          InflateCandidate(/*seed=*/21, testing::_, testing::_))
      .WillByDefault([](uint32_t seed, std::string_view query,
                        InflateCandidateCallback done_callback) {
        std::vector<LobsterImageCandidate> inflated_candidates = {
            LobsterImageCandidate(/*id=*/0, /*image_bytes=*/"a1b2c3",
                                  /*seed=*/30,
                                  /*query=*/"a nice strawberry")};
        std::move(done_callback).Run(std::move(inflated_candidates));
      });

  LobsterSessionImpl session(std::move(lobster_client));
  session.RequestCandidates("a nice strawberry", 2,
                            base::BindOnce([](const LobsterResult&) {}));
  RunUntilIdle();

  base::test::TestFuture<bool> future;

  session.DownloadCandidate(/*id=*/1, future.GetCallback());

  EXPECT_TRUE(future.Get());
}

}  // namespace

}  // namespace ash
