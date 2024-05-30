// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_ui_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/system/mahi/mahi_ui_update.h"
#include "ash/system/mahi/test/mahi_test_util.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/system/mahi/test/mock_mahi_ui_controller_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Aliases ---------------------------------------------------------------------

using chromeos::MahiResponseStatus;
using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;

// MockView --------------------------------------------------------------------

class MockView : public views::View {
 public:
  MOCK_METHOD(void, VisibilityChanged, (views::View*, bool), (override));
};

}  // namespace

class MahiUiControllerTest : public AshTestBase {
 protected:
  MockMahiUiControllerDelegate& delegate() { return delegate_; }
  MockView& delegate_view() { return delegate_view_; }
  MockMahiManager& mock_mahi_manager() { return mock_mahi_manager_; }
  MahiUiController& ui_controller() { return ui_controller_; }

 private:
  // AshTestBase:
  void SetUp() override {
    ON_CALL(delegate_, GetView).WillByDefault(Return(&delegate_view_));
    AshTestBase::SetUp();
    scoped_setter_ = std::make_unique<chromeos::ScopedMahiManagerSetter>(
        &mock_mahi_manager_);
  }

  void TearDown() override {
    scoped_setter_.reset();
    AshTestBase::TearDown();
  }

  NiceMock<MockView> delegate_view_;
  NiceMock<MahiUiController> ui_controller_;
  NiceMock<MockMahiUiControllerDelegate> delegate_{&ui_controller_};
  NiceMock<MockMahiManager> mock_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
};

// Checks `MahiUiController::Delegate` when navigating to the state that the
// view displaying questions and answers should show.
TEST_F(MahiUiControllerTest, NavigateToQuestionAnswerView) {
  EXPECT_CALL(delegate(),
              OnUpdated(Property(
                  &MahiUiUpdate::type,
                  Eq(MahiUiUpdateType::kQuestionAndAnswerViewNavigated))));
  EXPECT_CALL(delegate_view(),
              VisibilityChanged(&delegate_view(), /*is_visible=*/false));
  ui_controller().NavigateToQuestionAnswerView();
  Mock::VerifyAndClearExpectations(&delegate());
  Mock::VerifyAndClearExpectations(&delegate_view());

  // Config the delegate's visibility state and expect the delegate view to be
  // visible after navigation.
  ON_CALL(delegate(), GetViewVisibility)
      .WillByDefault([](VisibilityState state) {
        return state == VisibilityState::kQuestionAndAnswer;
      });

  EXPECT_CALL(delegate_view(),
              VisibilityChanged(&delegate_view(), /*is_visible=*/true));
  EXPECT_CALL(delegate(),
              OnUpdated(Property(
                  &MahiUiUpdate::type,
                  Eq(MahiUiUpdateType::kQuestionAndAnswerViewNavigated))));
  ui_controller().NavigateToQuestionAnswerView();
  Mock::VerifyAndClearExpectations(&delegate());
  Mock::VerifyAndClearExpectations(&delegate_view());
}

// Checks `MahiUiController::Delegate` when navigating to the state that the
// view displaying summary and outlines should show.
TEST_F(MahiUiControllerTest, NavigateToSummaryOutlinesSection) {
  EXPECT_CALL(delegate(),
              OnUpdated(Property(
                  &MahiUiUpdate::type,
                  Eq(MahiUiUpdateType::kSummaryAndOutlinesSectionNavigated))));
  EXPECT_CALL(delegate_view(),
              VisibilityChanged(&delegate_view(), /*is_visible=*/false));
  ui_controller().NavigateToSummaryOutlinesSection();
  Mock::VerifyAndClearExpectations(&delegate());
  Mock::VerifyAndClearExpectations(&delegate_view());

  // Config the delegate's visibility state and expect the delegate view to be
  // visible after navigation.
  ON_CALL(delegate(), GetViewVisibility)
      .WillByDefault([](VisibilityState state) {
        return state == VisibilityState::kSummaryAndOutlines;
      });

  EXPECT_CALL(delegate_view(),
              VisibilityChanged(&delegate_view(), /*is_visible=*/true));
  EXPECT_CALL(delegate(),
              OnUpdated(Property(
                  &MahiUiUpdate::type,
                  Eq(MahiUiUpdateType::kSummaryAndOutlinesSectionNavigated))));
  ui_controller().NavigateToSummaryOutlinesSection();
  Mock::VerifyAndClearExpectations(&delegate());
  Mock::VerifyAndClearExpectations(&delegate_view());
}

// Checks `MahiUiController::Delegate` when the refresh availability updates.
TEST_F(MahiUiControllerTest, NotifyRefreshAvailabilityChanged) {
  // Check when the refresh availability becomes false.
  EXPECT_CALL(delegate(),
              OnUpdated(AllOf(
                  Property(&MahiUiUpdate::type,
                           Eq(MahiUiUpdateType::kRefreshAvailabilityUpdated)),
                  Property(&MahiUiUpdate::GetRefreshAvailability, false))));
  ui_controller().NotifyRefreshAvailabilityChanged(/*available=*/false);
  Mock::VerifyAndClearExpectations(&delegate());

  // Check when the refresh availability becomes true.
  EXPECT_CALL(delegate(),
              OnUpdated(AllOf(
                  Property(&MahiUiUpdate::type,
                           Eq(MahiUiUpdateType::kRefreshAvailabilityUpdated)),
                  Property(&MahiUiUpdate::GetRefreshAvailability, true))));
  ui_controller().NotifyRefreshAvailabilityChanged(/*available=*/true);
  Mock::VerifyAndClearExpectations(&delegate());
}

// Checks `MahiUiController::Delegate` when the contents get refreshed.
TEST_F(MahiUiControllerTest, RefreshContents) {
  InSequence s;
  EXPECT_CALL(delegate(),
              OnUpdated(Property(
                  &MahiUiUpdate::type,
                  Eq(MahiUiUpdateType::kSummaryAndOutlinesSectionNavigated))));
  EXPECT_CALL(
      delegate(),
      OnUpdated(Property(&MahiUiUpdate::type,
                         Eq(MahiUiUpdateType::kContentsRefreshInitiated))));

  ui_controller().RefreshContents();
  Mock::VerifyAndClearExpectations(&delegate());
}

// Checks `MahiUiController::Delegate` when retrying summary and outlines.
TEST_F(MahiUiControllerTest, RetrySummaryAndOutlines) {
  EXPECT_CALL(
      delegate(),
      OnUpdated(Property(&MahiUiUpdate::type,
                         Eq(MahiUiUpdateType::kSummaryAndOutlinesReloaded))));

  ui_controller().Retry(VisibilityState::kSummaryAndOutlines);
  Mock::VerifyAndClearExpectations(&delegate());
}

// Checks `MahiUiController::Delegate` when retrying the previous question.
TEST_F(MahiUiControllerTest, RetrySendQuestion) {
  // Send a question before retrying to ensure a previous question is available.
  const std::u16string question(u"fake question");
  const bool current_panel_content = true;
  ui_controller().SendQuestion(question, current_panel_content,
                               MahiUiController::QuestionSource::kPanel);

  EXPECT_CALL(
      delegate(),
      OnUpdated(AllOf(
          Property(&MahiUiUpdate::type, Eq(MahiUiUpdateType::kQuestionReAsked)),
          Property(&MahiUiUpdate::GetReAskQuestionParams,
                   AllOf(Field(&MahiQuestionParams::current_panel_content,
                               current_panel_content),
                         Field(&MahiQuestionParams::question, question))))));

  ui_controller().Retry(VisibilityState::kQuestionAndAnswer);
  Mock::VerifyAndClearExpectations(&delegate());
}

// Checks `MahiUiController::Delegate` when sending a question.
TEST_F(MahiUiControllerTest, SendQuestion) {
  const std::u16string answer(u"fake answer");
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [&answer](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(answer, MahiResponseStatus::kSuccess);
          });

  InSequence s;
  const std::u16string question(u"fake question");
  EXPECT_CALL(delegate(),
              OnUpdated(AllOf(Property(&MahiUiUpdate::type,
                                       Eq(MahiUiUpdateType::kQuestionPosted)),
                              Property(&MahiUiUpdate::GetQuestion, question))));
  EXPECT_CALL(delegate(),
              OnUpdated(AllOf(Property(&MahiUiUpdate::type,
                                       Eq(MahiUiUpdateType::kAnswerLoaded)),
                              Property(&MahiUiUpdate::GetAnswer, answer))));

  ui_controller().SendQuestion(question, /*current_panel_content=*/true,
                               MahiUiController::QuestionSource::kPanel);
  Mock::VerifyAndClearExpectations(&delegate());
}

// Checks `MahiUiController::Delegate` when the summary and outlines update.
TEST_F(MahiUiControllerTest, UpdateSummaryAndOutlines) {
  // Config the mock mahi manager to return summary and outlines.
  const std::u16string summary(u"fake summary");
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault(
          [&summary](chromeos::MahiManager::MahiSummaryCallback callback) {
            std::move(callback).Run(summary, MahiResponseStatus::kSuccess);
          });
  ON_CALL(mock_mahi_manager(), GetOutlines)
      .WillByDefault(mahi_test_util::ReturnDefaultOutlines);

  EXPECT_CALL(delegate(),
              OnUpdated(AllOf(Property(&MahiUiUpdate::type,
                                       Eq(MahiUiUpdateType::kSummaryLoaded)),
                              Property(&MahiUiUpdate::GetSummary, summary))));
  EXPECT_CALL(
      delegate(),
      OnUpdated(AllOf(
          Property(&MahiUiUpdate::type, Eq(MahiUiUpdateType::kOutlinesLoaded)),
          Property(&MahiUiUpdate::GetOutlines,
                   testing::ElementsAreArray(
                       mahi_test_util::GetDefaultFakeOutlines())))));

  ui_controller().UpdateSummaryAndOutlines();
  Mock::VerifyAndClearExpectations(&delegate());
}

// Checks new requests can discard pending ones to avoid racing.
TEST_F(MahiUiControllerTest, RacingRequests) {
  // Configs the mock mahi manager to respond async-ly.
  base::test::TestFuture<void> summary_waiter;
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([&summary_waiter](
                         chromeos::MahiManager::MahiSummaryCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](base::OnceClosure unblock_closure,
                   chromeos::MahiManager::MahiSummaryCallback callback) {
                  std::move(callback).Run(u"fake summary",
                                          MahiResponseStatus::kSuccess);
                  std::move(unblock_closure).Run();
                },
                summary_waiter.GetCallback(), std::move(callback)));
      });

  base::test::TestFuture<void> outline_waiter;
  ON_CALL(mock_mahi_manager(), GetOutlines)
      .WillByDefault([&outline_waiter](
                         chromeos::MahiManager::MahiOutlinesCallback callback) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](base::OnceClosure unblock_closure,
                   chromeos::MahiManager::MahiOutlinesCallback callback) {
                  mahi_test_util::ReturnDefaultOutlines(std::move(callback));
                  std::move(unblock_closure).Run();
                },
                outline_waiter.GetCallback(), std::move(callback)));
      });

  base::test::TestFuture<void> answer_waiter;
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault([&answer_waiter](
                         const std::u16string& question,
                         bool current_panel_content,
                         chromeos::MahiManager::MahiAnswerQuestionCallback
                             callback) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](base::OnceClosure unblock_closure,
                   chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
                  std::move(callback).Run(u"fake answer",
                                          MahiResponseStatus::kSuccess);
                  std::move(unblock_closure).Run();
                },
                answer_waiter.GetCallback(), std::move(callback)));
      });

  // Pending `UpdateSummaryAndOutlines` is discarded on new `SendQuestion` call.
  // So that `OnUpdated` is only called with QA types.
  EXPECT_CALL(delegate(),
              OnUpdated(Property(&MahiUiUpdate::type,
                                 Eq(MahiUiUpdateType::kQuestionPosted))));
  EXPECT_CALL(delegate(),
              OnUpdated(Property(&MahiUiUpdate::type,
                                 Eq(MahiUiUpdateType::kAnswerLoaded))));

  EXPECT_CALL(delegate(),
              OnUpdated(Property(&MahiUiUpdate::type,
                                 Eq(MahiUiUpdateType::kSummaryLoaded))))
      .Times(0);
  EXPECT_CALL(delegate(),
              OnUpdated(Property(&MahiUiUpdate::type,
                                 Eq(MahiUiUpdateType::kOutlinesLoaded))))
      .Times(0);

  ui_controller().UpdateSummaryAndOutlines();
  ui_controller().SendQuestion(u"fake question", /*current_panel_content=*/true,
                               MahiUiController::QuestionSource::kPanel);

  ASSERT_TRUE(outline_waiter.Wait());
  ASSERT_TRUE(summary_waiter.Wait());
  ASSERT_TRUE(answer_waiter.Wait());
  Mock::VerifyAndClearExpectations(&delegate());
}

}  // namespace ash
