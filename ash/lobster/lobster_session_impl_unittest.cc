// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session_impl.h"

#include <optional>
#include <string_view>

#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_metrics_state_enums.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/fake_text_input_client.h"

namespace ash {
namespace {

LobsterCandidateStore GetDummyLobsterCandidateStore() {
  LobsterCandidateStore store;
  store.Cache({.id = 0,
               .image_bytes = "a1b2c3",
               .seed = 20,
               .query = "a nice raspberry"});
  store.Cache({.id = 1,
               .image_bytes = "d4e5f6",
               .seed = 21,
               .query = "a nice raspberry"});

  return store;
}

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
  MOCK_METHOD(void,
              QueueInsertion,
              (const std::string& image_bytes,
               StatusCallback insert_status_callback),
              (override));
  MOCK_METHOD(void,
              LoadUI,
              (std::optional<std::string> query, LobsterMode mode),
              (override));
  MOCK_METHOD(void, ShowUI, (), (override));
  MOCK_METHOD(void, CloseUI, (), (override));
  MOCK_METHOD(bool, UserHasAccess, (), (override));
};

class LobsterSessionImplTest : public AshTestBase {
 public:
  LobsterSessionImplTest() = default;
  LobsterSessionImplTest(const LobsterSessionImplTest&) = delete;
  LobsterSessionImplTest& operator=(const LobsterSessionImplTest&) = delete;
  ~LobsterSessionImplTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  base::FilePath GetDownloadPath() { return scoped_temp_dir_.GetPath(); }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  ui::InputMethod& ime() { return ime_; }

 private:
  InputMethodAsh ime_{nullptr};
  base::ScopedTempDir scoped_temp_dir_;
  base::HistogramTester histogram_tester_;
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

  LobsterSessionImpl session(std::move(lobster_client),
                             LobsterEntryPoint::kPicker);

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

  LobsterSessionImpl session(std::move(lobster_client),
                             LobsterEntryPoint::kPicker);

  base::test::TestFuture<const LobsterResult&> future;

  session.RequestCandidates(/*query=*/"a nice blueberry", /*num_candidates=*/1,
                            future.GetCallback());

  EXPECT_EQ(future.Get().error(),
            LobsterError(LobsterErrorCode::kUnknown, "unknown error"));
}

TEST_F(LobsterSessionImplTest, CanNotDownloadACandidateIfItIsNotCached) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();
  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store,
                             LobsterEntryPoint::kPicker);

  base::test::TestFuture<bool> future;
  session.DownloadCandidate(/*id=*/2, GetDownloadPath(), future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(LobsterSessionImplTest, CanDownloadACandidateIfItIsInCache) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

  ON_CALL(*lobster_client,
          InflateCandidate(/*seed=*/21, testing::_, testing::_))
      .WillByDefault([](uint32_t seed, std::string_view query,
                        InflateCandidateCallback done_callback) {
        std::vector<LobsterImageCandidate> inflated_candidates = {
            LobsterImageCandidate(/*id=*/1, /*image_bytes=*/"a1b2c3",
                                  /*seed=*/30,
                                  /*query=*/"a nice strawberry")};
        std::move(done_callback).Run(std::move(inflated_candidates));
      });

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kPicker);
  session.RequestCandidates("a nice strawberry", 2,
                            base::BindOnce([](const LobsterResult&) {}));

  base::test::TestFuture<bool> future;

  session.DownloadCandidate(/*id=*/1, GetDownloadPath(), future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_TRUE(
      base::PathExists(GetDownloadPath().Append("a nice strawberry-1.jpeg")));
}

TEST_F(LobsterSessionImplTest,
       CanNotPreviewFeedbackForACandidateIfItIsNotCached) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();
  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store,
                             LobsterEntryPoint::kPicker);
  base::test::TestFuture<const LobsterFeedbackPreviewResponse&> future;

  session.PreviewFeedback(/*id=*/2, future.GetCallback());

  EXPECT_EQ(future.Get().error(), "No candidate found.");
}

TEST_F(LobsterSessionImplTest, CanPreviewFeedbackForACandidateIfItIsInCache) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();
  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store,
                             LobsterEntryPoint::kPicker);
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
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

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

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kPicker);
  EXPECT_FALSE(session.SubmitFeedback(/*candidate_id*/ 2,
                                      /*description=*/"Awesome raspberry"));
}

TEST_F(LobsterSessionImplTest,
       CanNotSubmitFeedbackForACandiateIfSubmissionFails) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();
  auto lobster_client = std::make_unique<MockLobsterClient>();

  ON_CALL(*lobster_client, SubmitFeedback(/*query=*/"a nice raspberry",
                                          /*model_input=*/"dummy_version",
                                          /*description=*/"Awesome raspberry",
                                          /*image_bytes=*/"a1b2c3"))
      .WillByDefault(testing::Return(false));

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kPicker);
  EXPECT_FALSE(session.SubmitFeedback(/*candidate_id*/ 0,
                                      /*description=*/"Awesome raspberry"));
}

TEST_F(LobsterSessionImplTest, CanSubmitFeedbackForACandiateIfItIsInCache) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

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

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kPicker);
  EXPECT_TRUE(session.SubmitFeedback(/*candidate_id*/ 0,
                                     /*description=*/"Awesome raspberry"));
  EXPECT_TRUE(session.SubmitFeedback(/*candidate_id*/ 1,
                                     /*description=*/"Awesome raspberry"));
}

TEST_F(LobsterSessionImplTest, RecordMetricsForPickerEntryPoint) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  LobsterSessionImpl session(std::move(lobster_client),
                             LobsterEntryPoint::kPicker);

  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kPickerTriggerFired, 1);
}

TEST_F(LobsterSessionImplTest, RecordMetricsForRightClickEntryPoint) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  LobsterSessionImpl session(std::move(lobster_client),
                             LobsterEntryPoint::kRightClickMenu);

  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kRightClickTriggerFired, 1);
}

TEST_F(LobsterSessionImplTest,
       RecordMetricsWhenDownloadingCandidateSuccessfully) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

  ON_CALL(*lobster_client,
          InflateCandidate(/*seed=*/21, testing::_, testing::_))
      .WillByDefault([](uint32_t seed, std::string_view query,
                        InflateCandidateCallback done_callback) {
        std::vector<LobsterImageCandidate> inflated_candidates = {
            LobsterImageCandidate(/*id=*/1, /*image_bytes=*/"a1b2c3",
                                  /*seed=*/31,
                                  /*query=*/"a nice strawberry")};
        std::move(done_callback).Run(std::move(inflated_candidates));
      });

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kPicker);

  base::test::TestFuture<bool> future;

  session.DownloadCandidate(/*id=*/1, GetDownloadPath(), future.GetCallback());

  EXPECT_TRUE(future.Get());

  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCandidateDownload, 1);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCandidateDownloadSuccess, 1);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCandidateDownloadError, 0);
}

TEST_F(LobsterSessionImplTest, RecordMetricsWhenFailingToDownloadCandidate) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

  ON_CALL(*lobster_client,
          InflateCandidate(/*seed=*/21, testing::_, testing::_))
      .WillByDefault([](uint32_t seed, std::string_view query,
                        InflateCandidateCallback done_callback) {
        std::move(done_callback).Run({});
      });

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kPicker);

  base::test::TestFuture<bool> future;

  session.DownloadCandidate(/*id=*/1, GetDownloadPath(), future.GetCallback());

  EXPECT_FALSE(future.Get());

  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCandidateDownload, 1);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCandidateDownloadSuccess, 0);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCandidateDownloadError, 1);
}

TEST_F(LobsterSessionImplTest, RecordMetricsWhenCommittingAsInsert) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

  ON_CALL(*lobster_client,
          InflateCandidate(/*seed=*/21, testing::_, testing::_))
      .WillByDefault([](uint32_t seed, std::string_view query,
                        InflateCandidateCallback done_callback) {
        std::vector<LobsterImageCandidate> inflated_candidates = {
            LobsterImageCandidate(/*id=*/1, /*image_bytes=*/"a1b2c3",
                                  /*seed=*/21,
                                  /*query=*/"a nice strawberry")};
        std::move(done_callback).Run(std::move(inflated_candidates));
      });

  ON_CALL(*lobster_client, QueueInsertion(/*image_bytes=*/"a1b2c3", testing::_))
      .WillByDefault([](const std::string& image_bytes,
                        LobsterClient::StatusCallback callback) {
        std::move(callback).Run(true);
      });

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kPicker);

  base::test::TestFuture<bool> future;

  session.CommitAsInsert(/*id=*/1, future.GetCallback());

  EXPECT_TRUE(future.Get());
  histogram_tester().ExpectBucketCount("Ash.Lobster.State",
                                       LobsterMetricState::kCommitAsInsert, 1);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCommitAsInsertSuccess, 1);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCommitAsInsertError, 0);
}

TEST_F(LobsterSessionImplTest, RecordMetricsWhenFailingToCommitAsInsert) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

  ON_CALL(*lobster_client,
          InflateCandidate(/*seed=*/21, testing::_, testing::_))
      .WillByDefault([](uint32_t seed, std::string_view query,
                        InflateCandidateCallback done_callback) {
        std::move(done_callback).Run({});
      });

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kPicker);

  base::test::TestFuture<bool> future;

  session.CommitAsInsert(/*id=*/1, future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester().ExpectBucketCount("Ash.Lobster.State",
                                       LobsterMetricState::kCommitAsInsert, 1);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCommitAsInsertSuccess, 0);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCommitAsInsertError, 1);
}

TEST_F(LobsterSessionImplTest, RecordMetricsWhenCommittingAsDownload) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

  ON_CALL(*lobster_client,
          InflateCandidate(/*seed=*/21, testing::_, testing::_))
      .WillByDefault([](uint32_t seed, std::string_view query,
                        InflateCandidateCallback done_callback) {
        std::vector<LobsterImageCandidate> inflated_candidates = {
            LobsterImageCandidate(/*id=*/1, /*image_bytes=*/"a1b2c3",
                                  /*seed=*/21,
                                  /*query=*/"a nice strawberry")};
        std::move(done_callback).Run(std::move(inflated_candidates));
      });

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kPicker);

  base::test::TestFuture<bool> future;

  session.CommitAsDownload(/*id=*/1, GetDownloadPath(), future.GetCallback());

  EXPECT_TRUE(future.Get());
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCommitAsDownload, 1);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCommitAsDownloadSuccess, 1);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCommitAsDownloadError, 0);
}

TEST_F(LobsterSessionImplTest, RecordMetricsWhenFailingToCommitAsDownload) {
  auto lobster_client = std::make_unique<MockLobsterClient>();
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

  ON_CALL(*lobster_client,
          InflateCandidate(/*seed=*/21, testing::_, testing::_))
      .WillByDefault([](uint32_t seed, std::string_view query,
                        InflateCandidateCallback done_callback) {
        std::move(done_callback).Run({});
      });

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kPicker);

  base::test::TestFuture<bool> future;

  session.CommitAsDownload(/*id=*/1, GetDownloadPath(), future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCommitAsDownload, 1);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCommitAsDownloadSuccess, 0);
  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kCommitAsDownloadError, 1);
}

class LobsterSessionImplMetricsTest : public testing::Test {
 public:
  LobsterSessionImplMetricsTest() = default;
  ~LobsterSessionImplMetricsTest() override = default;

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

class LobsterSessionImplMetrics
    : public LobsterSessionImplMetricsTest,
      public testing::WithParamInterface<LobsterMetricState> {};

INSTANTIATE_TEST_SUITE_P(
    LobsterSessionImplMetricsTest,
    LobsterSessionImplMetrics,
    testing::ValuesIn<LobsterMetricState>({
        LobsterMetricState::kQueryPageImpression,
        LobsterMetricState::kRequestInitialCandidates,
        LobsterMetricState::kRequestInitialCandidatesSuccess,
        LobsterMetricState::kRequestInitialCandidatesError,
        LobsterMetricState::kInitialCandidatesImpression,
        LobsterMetricState::kRequestMoreCandidates,
        LobsterMetricState::kRequestMoreCandidatesSuccess,
        LobsterMetricState::kRequestMoreCandidatesError,
        LobsterMetricState::kMoreCandidatesAppended,
        LobsterMetricState::kFeedbackThumbsUp,
        LobsterMetricState::kFeedbackThumbsDown,
    }));

TEST_P(LobsterSessionImplMetrics, RecordsWebUIMetricEvent) {
  const LobsterMetricState& state = GetParam();

  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(),
                             LobsterEntryPoint::kPicker);

  session.RecordWebUIMetricEvent(state);

  histogram_tester().ExpectBucketCount("Ash.Lobster.State", state, 1);
}

}  // namespace

}  // namespace ash
