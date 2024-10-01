// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session_impl.h"

#include <optional>
#include <string_view>

#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "base/files/file_util.h"
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
  MOCK_METHOD(bool,
              SubmitFeedback,
              (const std::string& query,
               const std::string& model_input,
               const std::string& description,
               const std::string& image_bytes),
              (override));
  MOCK_METHOD(void, LoadUI, (std::optional<std::string> query), (override));
  MOCK_METHOD(void, ShowUI, (), (override));
  MOCK_METHOD(void, CloseUI, (), (override));
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

TEST_F(LobsterSessionImplTest, CanNotDownloadACandidateIfItIsNotCached) {
  LobsterCandidateStore store;
  store.Cache({.id = 0,
               .image_bytes = "a1b2c3",
               .seed = 20,
               .query = "a nice raspberry"});
  store.Cache({.id = 1,
               .image_bytes = "d4e5f6",
               .seed = 21,
               .query = "a nice raspberry"});

  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store);

  base::test::TestFuture<bool> future;
  session.DownloadCandidate(/*id=*/2, base::FilePath("dummy_path"),
                            future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(LobsterSessionImplTest, CanDownloadACandiateIfItIsInCache) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store;
  store.Cache({.id = 0,
               .image_bytes = "a1b2c3",
               .seed = 20,
               .query = "a nice strawberry"});
  store.Cache({.id = 1,
               .image_bytes = "d4e5f6",
               .seed = 21,
               .query = "a nice strawberry"});

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

  LobsterSessionImpl session(std::move(lobster_client), store);
  session.RequestCandidates("a nice strawberry", 2,
                            base::BindOnce([](const LobsterResult&) {}));
  RunUntilIdle();

  base::test::TestFuture<bool> future;

  session.DownloadCandidate(/*id=*/1, base::FilePath("dummy_path"),
                            future.GetCallback());

  EXPECT_TRUE(future.Get());
}

TEST_F(LobsterSessionImplTest,
       CanNotPreviewFeedbackForACandidateIfItIsNotCached) {
  LobsterCandidateStore store;
  store.Cache({.id = 0,
               .image_bytes = "a1b2c3",
               .seed = 20,
               .query = "a nice raspberry"});
  store.Cache({.id = 1,
               .image_bytes = "d4e5f6",
               .seed = 21,
               .query = "a nice raspberry"});

  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store);
  base::test::TestFuture<const LobsterFeedbackPreviewResponse&> future;

  session.PreviewFeedback(/*id=*/2, future.GetCallback());

  EXPECT_EQ(future.Get().error(), "No candidate found.");
}

TEST_F(LobsterSessionImplTest, CanPreviewFeedbackForACandidateIfItIsInCache) {
  LobsterCandidateStore store;
  store.Cache({.id = 0,
               .image_bytes = "a1b2c3",
               .seed = 20,
               .query = "a nice raspberry"});
  store.Cache({.id = 1,
               .image_bytes = "d4e5f6",
               .seed = 21,
               .query = "a nice raspberry"});
  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store);
  base::test::TestFuture<const LobsterFeedbackPreviewResponse&> future;

  session.PreviewFeedback(/*id=*/1, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->preview_image_bytes, "d4e5f6");
  std::map<std::string, std::string> expected_feedback_preview_fields = {
      {"model_version", "dummy_version"}, {"model_input", "a nice raspberry"}};
  EXPECT_EQ(future.Get()->fields, expected_feedback_preview_fields);
}

TEST_F(LobsterSessionImplTest,
       CanNotSubmitFeedbackForACandiateIfItIsNotCached) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store;
  store.Cache({.id = 0,
               .image_bytes = "a1b2c3",
               .seed = 20,
               .query = "a nice raspberry"});
  store.Cache({.id = 1,
               .image_bytes = "d4e5f6",
               .seed = 21,
               .query = "a nice raspberry"});

  ON_CALL(*lobster_client, SubmitFeedback(/*query=*/"a nice raspberry",
                                          /*model_input=*/"dummy_version",
                                          /*description=*/"Awesome raspberry",
                                          /*image_bytes=*/"a1b2c3"))
      .WillByDefault(testing::Return(true));

  ON_CALL(*lobster_client, SubmitFeedback(/*query=*/"a nice raspberry",
                                          /*model_input=*/"dummy_version",
                                          /*description=*/"Awesome raspberry",
                                          /*image_bytes=*/"d4e5f6"))
      .WillByDefault(testing::Return(true));

  LobsterSessionImpl session(std::move(lobster_client), store);
  EXPECT_FALSE(session.SubmitFeedback(/*candidate_id*/ 2,
                                      /*description=*/"Awesome raspberry"));
}

TEST_F(LobsterSessionImplTest,
       CanNotSubmitFeedbackForACandiateIfSubmissionFails) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store;
  store.Cache({.id = 0,
               .image_bytes = "a1b2c3",
               .seed = 20,
               .query = "a nice raspberry"});
  store.Cache({.id = 1,
               .image_bytes = "d4e5f6",
               .seed = 21,
               .query = "a nice raspberry"});

  ON_CALL(*lobster_client, SubmitFeedback(/*query=*/"a nice raspberry",
                                          /*model_input=*/"dummy_version",
                                          /*description=*/"Awesome raspberry",
                                          /*image_bytes=*/"a1b2c3"))
      .WillByDefault(testing::Return(false));

  LobsterSessionImpl session(std::move(lobster_client), store);
  EXPECT_FALSE(session.SubmitFeedback(/*candidate_id*/ 0,
                                      /*description=*/"Awesome raspberry"));
}

TEST_F(LobsterSessionImplTest, CanSubmitFeedbackForACandiateIfItIsInCache) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store;
  store.Cache({.id = 0,
               .image_bytes = "a1b2c3",
               .seed = 20,
               .query = "a nice raspberry"});
  store.Cache({.id = 1,
               .image_bytes = "d4e5f6",
               .seed = 21,
               .query = "a nice raspberry"});

  EXPECT_CALL(*lobster_client,
              SubmitFeedback(/*query=*/"a nice raspberry",
                             /*model_input=*/"dummy_version",
                             /*description=*/"Awesome raspberry",
                             /*image_bytes=*/"a1b2c3"))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(*lobster_client,
              SubmitFeedback(/*query=*/"a nice raspberry",
                             /*model_input=*/"dummy_version",
                             /*description=*/"Awesome raspberry",
                             /*image_bytes=*/"d4e5f6"))
      .WillOnce(testing::Return(true));

  LobsterSessionImpl session(std::move(lobster_client), store);
  EXPECT_TRUE(session.SubmitFeedback(/*candidate_id*/ 0,
                                     /*description=*/"Awesome raspberry"));
  EXPECT_TRUE(session.SubmitFeedback(/*candidate_id*/ 1,
                                     /*description=*/"Awesome raspberry"));
}

}  // namespace

}  // namespace ash
