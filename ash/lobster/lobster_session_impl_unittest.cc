// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_session_impl.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "ash/lobster/lobster_candidate_store.h"
#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_metrics_state_enums.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "ash/public/cpp/lobster/lobster_text_input_context.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/account_id/account_id.h"
#include "components/feedback/feedback_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/fake_text_input_client.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Optional;

LobsterCandidateStore GetDummyLobsterCandidateStore() {
  LobsterCandidateStore store;
  store.Cache(
      LobsterImageCandidate(/*id=*/0,
                            /*image_bytes=*/"a1b2c3",
                            /*seed=*/20,
                            /*user_query=*/"a nice raspberry",
                            /*rewritten_query=*/"rewritten: a nice raspberry"));
  store.Cache(
      LobsterImageCandidate(/*id=*/1,
                            /*image_bytes=*/"d4e5f6",
                            /*seed=*/21,
                            /*query=*/"a nice raspberry",
                            /*rewritten_query=*/"rewritten: a nice raspberry"));

  return store;
}

class MockLobsterClient : public LobsterClient {
 public:
  MockLobsterClient() {
    ON_CALL(*this, GetAccountId).WillByDefault(&EmptyAccountId);
  }

  MockLobsterClient(const MockLobsterClient&) = delete;
  MockLobsterClient& operator=(const MockLobsterClient&) = delete;

  ~MockLobsterClient() override = default;

  MOCK_METHOD(void, SetActiveSession, (LobsterSession * session), (override));
  MOCK_METHOD(LobsterSystemState,
              GetSystemState,
              (const LobsterTextInputContext& text_input_context),
              (override));
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
  MOCK_METHOD(void,
              QueueInsertion,
              (const std::string& image_bytes,
               StatusCallback insert_status_callback),
              (override));
  MOCK_METHOD(void, ShowDisclaimerUI, (), (override));
  MOCK_METHOD(void,
              LoadUI,
              (std::optional<std::string> query,
               LobsterMode mode,
               const gfx::Rect& caret_bounds),
              (override));
  MOCK_METHOD(void, ShowUI, (), (override));
  MOCK_METHOD(void, CloseUI, (), (override));
  MOCK_METHOD(const AccountId&, GetAccountId, (), (override));
  MOCK_METHOD(void, AnnounceLater, (const std::u16string& message), (override));
};

class LobsterSessionImplTest : public AshTestBase {
 public:
  LobsterSessionImplTest() = default;
  LobsterSessionImplTest(const LobsterSessionImplTest&) = delete;
  LobsterSessionImplTest& operator=(const LobsterSessionImplTest&) = delete;
  ~LobsterSessionImplTest() override = default;

  void SetUp() override {
    auto shell_delegate = std::make_unique<TestShellDelegate>();
    shell_delegate->SetSendSpecializedFeatureFeedbackCallback(
        mock_send_specialized_feature_feedback_.Get());
    set_shell_delegate(std::move(shell_delegate));
    AshTestBase::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  base::FilePath GetDownloadPath() { return scoped_temp_dir_.GetPath(); }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  ui::InputMethod& ime() { return ime_; }

  base::MockCallback<TestShellDelegate::SendSpecializedFeatureFeedbackCallback>&
  mock_send_specialized_feature_feedback() {
    return mock_send_specialized_feature_feedback_;
  }

 private:
  InputMethodAsh ime_{nullptr};
  base::ScopedTempDir scoped_temp_dir_;
  base::HistogramTester histogram_tester_;

  base::MockCallback<TestShellDelegate::SendSpecializedFeatureFeedbackCallback>
      mock_send_specialized_feature_feedback_;
};

TEST_F(LobsterSessionImplTest, RequestCandidatesWithThreeResults) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  EXPECT_CALL(*lobster_client,
              RequestCandidates(/*query=*/"a nice strawberry",
                                /*num_candidates=*/3, testing::_))
      .WillOnce([](std::string_view query, int num_candidates,
                   RequestCandidatesCallback done_callback) {
        std::vector<LobsterImageCandidate> image_candidates = {
            LobsterImageCandidate(
                /*id=*/0, /*image_bytes=*/"a1b2c3",
                /*seed=*/20,
                /*user_query=*/"a nice strawberry",
                /*rewritten_query=*/"rewritten: a nice strawberry"),
            LobsterImageCandidate(
                /*id=*/1, /*image_bytes=*/"d4e5f6",
                /*seed=*/21,
                /*user_query=*/"a nice strawberry",
                /*rewritten_query=*/"rewritten: a nice strawberry"),
            LobsterImageCandidate(
                /*id=*/2, /*image_bytes=*/"g7h8i9",
                /*seed=*/22,
                /*user_query=*/"a nice strawberry",
                /*rewritten_query=*/"rewritten: a nice strawberry")};
        std::move(done_callback).Run(std::move(image_candidates));
      });

  LobsterSessionImpl session(std::move(lobster_client),
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

  base::test::TestFuture<const LobsterResult&> future;

  session.RequestCandidates(/*query=*/"a nice strawberry", /*num_candidates=*/3,
                            future.GetCallback());

  EXPECT_THAT(future.Get().value(),
              testing::ElementsAre(
                  LobsterImageCandidate(
                      /*expected_id=*/0,
                      /*expected_image_bytes=*/"a1b2c3",
                      /*seed=*/20, /*user_query=*/"a nice strawberry",
                      /*rewritten_query=*/"rewritten: a nice strawberry"),
                  LobsterImageCandidate(
                      /*expected_id=*/1,
                      /*expected_image_bytes=*/"d4e5f6",
                      /*seed=*/21, /*user_query=*/"a nice strawberry",
                      /*rewritten_query=*/"rewritten: a nice strawberry"),
                  LobsterImageCandidate(
                      /*expected_id=*/2,
                      /*expected_image_bytes=*/"g7h8i9",
                      /*seed=*/22, /*user_query=*/"a nice strawberry",
                      /*rewritten_query=*/"rewritten: a nice strawberry")));
}

TEST_F(LobsterSessionImplTest, RequestCandidatesReturnsUnknownError) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  EXPECT_CALL(*lobster_client,
              RequestCandidates(/*query=*/"a nice blueberry",
                                /*num_candidates=*/1, testing::_))
      .WillOnce([](std::string_view query, int num_candidates,
                   RequestCandidatesCallback done_callback) {
        std::move(done_callback)
            .Run(base::unexpected(
                LobsterError(LobsterErrorCode::kUnknown, "unknown error")));
      });

  LobsterSessionImpl session(std::move(lobster_client),
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

  base::test::TestFuture<const LobsterResult&> future;

  session.RequestCandidates(/*query=*/"a nice blueberry", /*num_candidates=*/1,
                            future.GetCallback());

  EXPECT_EQ(future.Get().error(),
            LobsterError(LobsterErrorCode::kUnknown, "unknown error"));
}

TEST_F(LobsterSessionImplTest, CanNotDownloadACandidateIfItIsNotCached) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();
  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

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
                                  /*seed=*/201,
                                  /*user_query=*/"a nice raspberry",
                                  /*rewritten_query=*/"a nice raspberry")};
        std::move(done_callback).Run(std::move(inflated_candidates));
      });

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);
  session.RequestCandidates("a nice raspberry", 2,
                            base::BindOnce([](const LobsterResult&) {}));

  base::test::TestFuture<bool> future;

  session.DownloadCandidate(/*id=*/1, GetDownloadPath(), future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_TRUE(
      base::PathExists(GetDownloadPath().Append("a nice raspberry.jpeg")));
}

TEST_F(LobsterSessionImplTest,
       CanNotPreviewFeedbackForACandidateIfItIsNotCached) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();
  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);
  base::test::TestFuture<const LobsterFeedbackPreviewResponse&> future;

  session.PreviewFeedback(/*id=*/2, future.GetCallback());

  EXPECT_EQ(future.Get().error(), "No candidate found.");
}

TEST_F(LobsterSessionImplTest, CanPreviewFeedbackForACandidateIfItIsInCache) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();
  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);
  base::test::TestFuture<const LobsterFeedbackPreviewResponse&> future;

  session.PreviewFeedback(/*id=*/1, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->preview_image_bytes, "d4e5f6");
  std::map<std::string, std::string> expected_feedback_preview_fields = {
      {"Query and image", "a nice raspberry"}};
  EXPECT_EQ(future.Get()->fields, expected_feedback_preview_fields);
}

TEST_F(LobsterSessionImplTest,
       LoadUIFromCachedContextIsCalledUponTheCachedContext) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();
  auto lobster_client = std::make_unique<MockLobsterClient>();
  ON_CALL(*lobster_client, ShowDisclaimerUI).WillByDefault(testing::Return());

  EXPECT_CALL(*lobster_client,
              LoadUI(testing::Optional(std::string("a nice strawberry")),
                     LobsterMode::kInsert, gfx::Rect(1, 2, 3, 4)))
      .Times(1);

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

  session.ShowDisclaimerUIAndCacheContext("a nice strawberry",
                                          gfx::Rect(1, 2, 3, 4));
  session.LoadUIFromCachedContext();
}

TEST_F(LobsterSessionImplTest, SubmitFeedbackUsesClientAccountId) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();
  auto lobster_client = std::make_unique<MockLobsterClient>();
  AccountId fake_account_id =
      AccountId::FromUserEmailGaiaId("user@test.com", GaiaId("fakegaia"));
  EXPECT_CALL(*lobster_client, GetAccountId)
      .WillOnce(testing::ReturnRef(fake_account_id));

  EXPECT_CALL(mock_send_specialized_feature_feedback(),
              Run(fake_account_id, feedback::kLobsterFeedbackProductId,
                  /*description=*/
                  "model_input: a nice raspberry\n"
                  "model_version: dummy_version\n"
                  "user_description: Awesome raspberry",
                  /*image_bytes=*/Optional(Eq("a1b2c3")),
                  /*image_mime_type=*/Eq(std::nullopt)))
      .WillOnce(testing::Return(true));

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);
  EXPECT_TRUE(session.SubmitFeedback(/*candidate_id=*/0,
                                     /*description=*/"Awesome raspberry"));
}

TEST_F(LobsterSessionImplTest,
       CanNotSubmitFeedbackForACandiateIfItIsNotCached) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

  ON_CALL(mock_send_specialized_feature_feedback(),
          Run(_, feedback::kLobsterFeedbackProductId,
              /*description=*/
              "model_input: a nice raspberry\n"
              "model_version: dummy_version\n"
              "user_description: Awesome raspberry",
              /*image_bytes=*/Optional(Eq("a1b2c3")),
              /*image_mime_type=*/Eq(std::nullopt)))
      .WillByDefault(testing::Return(true));

  ON_CALL(mock_send_specialized_feature_feedback(),
          Run(_, feedback::kLobsterFeedbackProductId,
              /*description=*/
              "model_input: a nice raspberry\n"
              "model_version: dummy_version\n"
              "user_description: Awesome raspberry",
              /*image_bytes=*/Optional(Eq("d4e5f6")),
              /*image_mime_type=*/Eq(std::nullopt)))
      .WillByDefault(testing::Return(true));

  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);
  EXPECT_FALSE(session.SubmitFeedback(/*candidate_id*/ 2,
                                      /*description=*/"Awesome raspberry"));
}

TEST_F(LobsterSessionImplTest,
       CanNotSubmitFeedbackForACandiateIfSubmissionFails) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

  ON_CALL(mock_send_specialized_feature_feedback(),
          Run(_, feedback::kLobsterFeedbackProductId,
              /*description=*/
              "model_input: a nice raspberry\n"
              "model_version: dummy_version\n"
              "user_description: Awesome raspberry",
              /*image_bytes=*/Optional(Eq("a1b2c3")),
              /*image_mime_type=*/Eq(std::nullopt)))
      .WillByDefault(testing::Return(false));

  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);
  EXPECT_FALSE(session.SubmitFeedback(/*candidate_id*/ 0,
                                      /*description=*/"Awesome raspberry"));
}

TEST_F(LobsterSessionImplTest, CanSubmitFeedbackForACandiateIfItIsInCache) {
  LobsterCandidateStore store = GetDummyLobsterCandidateStore();

  EXPECT_CALL(mock_send_specialized_feature_feedback(),
              Run(_, feedback::kLobsterFeedbackProductId,
                  /*description=*/
                  "model_input: a nice raspberry\n"
                  "model_version: dummy_version\n"
                  "user_description: Awesome raspberry",
                  /*image_bytes=*/Optional(Eq("a1b2c3")),
                  /*image_mime_type=*/Eq(std::nullopt)))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(mock_send_specialized_feature_feedback(),
              Run(_, feedback::kLobsterFeedbackProductId,
                  /*description=*/
                  "model_input: a nice raspberry\n"
                  "model_version: dummy_version\n"
                  "user_description: Awesome raspberry",
                  /*image_bytes=*/Optional(Eq("d4e5f6")),
                  /*image_mime_type=*/Eq(std::nullopt)))
      .WillOnce(testing::Return(true));

  LobsterSessionImpl session(std::make_unique<MockLobsterClient>(), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);
  EXPECT_TRUE(session.SubmitFeedback(/*candidate_id*/ 0,
                                     /*description=*/"Awesome raspberry"));
  EXPECT_TRUE(session.SubmitFeedback(/*candidate_id*/ 1,
                                     /*description=*/"Awesome raspberry"));
}

TEST_F(LobsterSessionImplTest, RecordMetricsForPickerEntryPoint) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  LobsterSessionImpl session(std::move(lobster_client),
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kQuickInsertTriggerFired, 1);
}

TEST_F(LobsterSessionImplTest, RecordMetricsForRightClickEntryPoint) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  LobsterSessionImpl session(std::move(lobster_client),
                             LobsterEntryPoint::kRightClickMenu,
                             LobsterMode::kInsert);

  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kRightClickTriggerFired, 1);
}

TEST_F(LobsterSessionImplTest, RecordMetricsWhenDisplayingConsentScreen) {
  auto lobster_client = std::make_unique<MockLobsterClient>();

  ON_CALL(*lobster_client, ShowDisclaimerUI()).WillByDefault(testing::Return());

  LobsterSessionImpl session(std::move(lobster_client),
                             LobsterEntryPoint::kRightClickMenu,
                             LobsterMode::kInsert);

  session.ShowDisclaimerUIAndCacheContext(/*query=*/"a nice strawberry",
                                          /*anchor_bounds=*/gfx::Rect());

  histogram_tester().ExpectBucketCount(
      "Ash.Lobster.State", LobsterMetricState::kConsentScreenImpression, 1);
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
                                  /*user_query=*/"a nice strawberry",
                                  /*rewritten_query=*/"a nice strawberry")};
        std::move(done_callback).Run(std::move(inflated_candidates));
      });

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

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
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

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
                                  /*user_query=*/"a nice strawberry",
                                  /*rewritten_query=*/"a nice strawberry")};
        std::move(done_callback).Run(std::move(inflated_candidates));
      });

  ON_CALL(*lobster_client, QueueInsertion(/*image_bytes=*/"a1b2c3", testing::_))
      .WillByDefault([](const std::string& image_bytes,
                        LobsterClient::StatusCallback callback) {
        std::move(callback).Run(true);
      });

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

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
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

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
                                  /*user_query=*/"a nice strawberry",
                                  /*rewritten_query=*/"a nice strawberry")};
        std::move(done_callback).Run(std::move(inflated_candidates));
      });

  LobsterSessionImpl session(std::move(lobster_client), store,
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

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
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

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
  base::test::TaskEnvironment task_environment_;
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
                             LobsterEntryPoint::kQuickInsert,
                             LobsterMode::kInsert);

  session.RecordWebUIMetricEvent(state);

  histogram_tester().ExpectBucketCount("Ash.Lobster.State", state, 1);
}

}  // namespace

}  // namespace ash
