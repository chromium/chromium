// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_ui_controller.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_view.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "ash/system/mahi/test/mahi_test_util.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/system/mahi/test/mock_mahi_ui_controller_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_forward.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/session_manager/session_manager_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
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

// Utilities -------------------------------------------------------------------

void ChangeLockState(bool locked) {
  SessionInfo info;
  info.state = locked ? session_manager::SessionState::LOCKED
                      : session_manager::SessionState::ACTIVE;
  Shell::Get()->session_controller()->SetSessionInfo(info);
}

void UpdateSession(uint32_t session_id, const std::string& email) {
  UserSession session;
  session.session_id = session_id;
  session.user_info.type = user_manager::UserType::kRegular;
  session.user_info.account_id = AccountId::FromUserEmail(email);
  session.user_info.display_name = email;
  session.user_info.display_email = email;
  session.user_info.is_new_profile = false;

  SessionController::Get()->UpdateUserSession(session);
}

}  // namespace

class MahiUiControllerTest : public AshTestBase {
 public:
  MahiUiControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  MahiUiControllerTest(const MahiUiControllerTest&) = delete;
  MahiUiControllerTest& operator=(const MahiUiControllerTest&) = delete;

  ~MahiUiControllerTest() override = default;

 protected:
  MockMahiUiControllerDelegate& delegate() { return delegate_; }
  MockView& delegate_view() { return delegate_view_; }
  MockMahiManager& mock_mahi_manager() { return mock_mahi_manager_; }
  MahiUiController& ui_controller() { return ui_controller_; }

 private:
  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kMahi,
                              chromeos::features::kFeatureManagementMahi},
        /*disabled_features=*/{});

    ON_CALL(delegate_, GetView).WillByDefault(Return(&delegate_view_));
    AshTestBase::SetUp();
    scoped_setter_ = std::make_unique<chromeos::ScopedMahiManagerSetter>(
        &mock_mahi_manager_);
  }

  void TearDown() override {
    scoped_setter_.reset();
    AshTestBase::TearDown();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
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

  const std::u16string summary(u"fake summary");
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault(
          [&summary](chromeos::MahiManager::MahiSummaryCallback callback) {
            std::move(callback).Run(summary, MahiResponseStatus::kSuccess);
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

  // `kSummaryLoaded` update should not be called when
  // `update_summary_after_answer_question` is false.
  EXPECT_CALL(delegate(),
              OnUpdated(Property(&MahiUiUpdate::type,
                                 Eq(MahiUiUpdateType::kSummaryLoaded))))
      .Times(0);

  ui_controller().SendQuestion(question, /*current_panel_content=*/true,
                               MahiUiController::QuestionSource::kPanel,
                               /*update_summary_after_answer_question=*/false);
  Mock::VerifyAndClearExpectations(&delegate());

  EXPECT_CALL(delegate(),
              OnUpdated(AllOf(Property(&MahiUiUpdate::type,
                                       Eq(MahiUiUpdateType::kQuestionPosted)),
                              Property(&MahiUiUpdate::GetQuestion, question))));
  EXPECT_CALL(delegate(),
              OnUpdated(AllOf(Property(&MahiUiUpdate::type,
                                       Eq(MahiUiUpdateType::kAnswerLoaded)),
                              Property(&MahiUiUpdate::GetAnswer, answer))));

  // `kSummaryLoaded` update should be called when
  // `update_summary_after_answer_question` is true.
  EXPECT_CALL(delegate(),
              OnUpdated(AllOf(Property(&MahiUiUpdate::type,
                                       Eq(MahiUiUpdateType::kSummaryLoaded)),
                              Property(&MahiUiUpdate::GetSummary, summary))));

  ui_controller().SendQuestion(question, /*current_panel_content=*/true,
                               MahiUiController::QuestionSource::kPanel,
                               /*update_summary_after_answer_question=*/true);
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

// Tests to make sure that when navigating back to summary view, the view is not
// stuck at loading (b/345621992).
TEST_F(MahiUiControllerTest, UpdateSummaryAfterAnswerQuestionAsync) {
  auto delay_time = base::Milliseconds(10);
  // Configs the mock mahi manager to respond async-ly.
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault(
          [delay_time](chromeos::MahiManager::MahiSummaryCallback callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                FROM_HERE,
                base::BindOnce(
                    [](chromeos::MahiManager::MahiSummaryCallback callback) {
                      std::move(callback).Run(u"fake summary",
                                              MahiResponseStatus::kSuccess);
                    },
                    std::move(callback)),
                delay_time);
          });

  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault([delay_time](
                         const std::u16string& question,
                         bool current_panel_content,
                         chromeos::MahiManager::MahiAnswerQuestionCallback
                             callback) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                [](chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
                  std::move(callback).Run(u"fake answer",
                                          MahiResponseStatus::kSuccess);
                },
                std::move(callback)),
            delay_time);
      });

  MahiPanelView panel_view(&ui_controller());
  ui_controller().SendQuestion(u"fake question", /*current_panel_content=*/true,
                               MahiUiController::QuestionSource::kMenuView,
                               /*update_summary_after_answer_question=*/true);

  auto* question_answer_view =
      panel_view.GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  auto* summary_outlines_section =
      panel_view.GetViewByID(mahi_constants::ViewId::kSummaryOutlinesSection);

  EXPECT_TRUE(question_answer_view->GetVisible());
  EXPECT_FALSE(summary_outlines_section->GetVisible());

  ui_controller().NavigateToSummaryOutlinesSection();
  EXPECT_FALSE(question_answer_view->GetVisible());
  EXPECT_TRUE(summary_outlines_section->GetVisible());

  // Summary is loading after answering a question.
  EXPECT_TRUE(
      panel_view
          .GetViewByID(mahi_constants::ViewId::kSummaryLoadingAnimatedImage)
          ->GetVisible());
  EXPECT_FALSE(panel_view.GetViewByID(mahi_constants::ViewId::kSummaryLabel)
                   ->GetVisible());

  // Fast forward until summary is loaded.
  task_environment()->FastForwardBy(base::Milliseconds(30));

  // The loading image should not be visible now since summary is fully loaded.
  EXPECT_FALSE(
      panel_view
          .GetViewByID(mahi_constants::ViewId::kSummaryLoadingAnimatedImage)
          ->GetVisible());
  EXPECT_TRUE(panel_view.GetViewByID(mahi_constants::ViewId::kSummaryLabel)
                  ->GetVisible());
}

// Test suite where the UI controller and its delegate are initialized on
// `SetUp` so ash::Shell can be initialized and the session state observed.
class MahiUiControllerWithSessionTest : public AshTestBase {
 protected:
  MockView& delegate_view() { return delegate_view_; }
  MockMahiManager& mock_mahi_manager() { return mock_mahi_manager_; }

  MockMahiUiControllerDelegate* delegate() { return delegate_.get(); }
  MahiUiController* ui_controller() { return ui_controller_.get(); }

 private:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kMahi, chromeos::features::kFeatureManagementMahi},
        {});
    ui_controller_ = std::make_unique<NiceMock<MahiUiController>>();
    delegate_ = std::make_unique<NiceMock<MockMahiUiControllerDelegate>>(
        ui_controller_.get());
    scoped_setter_ = std::make_unique<chromeos::ScopedMahiManagerSetter>(
        &mock_mahi_manager_);

    ON_CALL(mock_mahi_manager_, IsEnabled).WillByDefault(Return(true));
    ON_CALL(*delegate(), GetView).WillByDefault(Return(&delegate_view_));
  }

  void TearDown() override {
    delegate_.reset();
    ui_controller_.reset();
    scoped_setter_.reset();
    AshTestBase::TearDown();
  }

  NiceMock<MockView> delegate_view_;
  NiceMock<MockMahiManager> mock_mahi_manager_;
  std::unique_ptr<NiceMock<MahiUiController>> ui_controller_;
  std::unique_ptr<NiceMock<MockMahiUiControllerDelegate>> delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
};

// Tests that the `TimesPanelOpenedPerSession` metric records the amount of
// times the panel was opened and emits when the screen is locked.
TEST_F(MahiUiControllerWithSessionTest, TimesPanelOpenedPerSessionMetric) {
  base::HistogramTester histogram_tester;
  const int kTimesPanelWillOpen = 3;

  // Test that locking the screen will record the amount of times the panel
  // was opened while the session was active.
  for (int i = 0; i < kTimesPanelWillOpen; i++) {
    ui_controller()->OpenMahiPanel(GetPrimaryDisplay().id(), gfx::Rect());
    // Immediately close the widget to avoid a dangling pointer.
    ui_controller()->mahi_panel_widget()->CloseNow();
  }
  histogram_tester.ExpectBucketCount(
      mahi_constants::kTimesMahiPanelOpenedPerSessionHistogramName,
      /*sample=*/kTimesPanelWillOpen, /*expected_count=*/0);
  ChangeLockState(/*locked=*/true);
  histogram_tester.ExpectBucketCount(
      mahi_constants::kTimesMahiPanelOpenedPerSessionHistogramName,
      /*sample=*/kTimesPanelWillOpen, /*expected_count=*/1);

  // Test that the metric does not get recorded if the panel was never opened.
  ChangeLockState(/*locked=*/false);
  ChangeLockState(/*locked=*/true);
  histogram_tester.ExpectBucketCount(
      mahi_constants::kTimesMahiPanelOpenedPerSessionHistogramName,
      /*sample=*/0, /*expected_count=*/0);
}

// Tests that the metric is recorded after a state change from `ACTIVE` to
// any other session state.
TEST_F(MahiUiControllerWithSessionTest,
       TimesPanelOpenedPerSessionMetric_AllSessionStates) {
  base::HistogramTester histogram_tester;

  std::vector<session_manager::SessionState> non_active_session_states = {
      session_manager::SessionState::UNKNOWN,
      session_manager::SessionState::OOBE,
      session_manager::SessionState::LOGIN_PRIMARY,
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE,
      session_manager::SessionState::LOCKED,
      session_manager::SessionState::LOGIN_SECONDARY,
      session_manager::SessionState::RMA};

  SessionInfo info;
  for (size_t i = 0; i < non_active_session_states.size(); i++) {
    // Set the session to active so the count to be recorded can be accumulated.
    info.state = session_manager::SessionState::ACTIVE;
    Shell::Get()->session_controller()->SetSessionInfo(info);

    // Open and close the mahi panel. The metric should not be recorded yet.
    ui_controller()->OpenMahiPanel(GetPrimaryDisplay().id(), gfx::Rect());
    // Immediately close the widget to avoid a dangling pointer.
    ui_controller()->mahi_panel_widget()->CloseNow();
    histogram_tester.ExpectBucketCount(
        mahi_constants::kTimesMahiPanelOpenedPerSessionHistogramName,
        /*sample=*/1, /*expected_count=*/i);

    // Set the session to a non-active state. The metric should be recorded.
    info.state = non_active_session_states[i];
    Shell::Get()->session_controller()->SetSessionInfo(info);
    histogram_tester.ExpectBucketCount(
        mahi_constants::kTimesMahiPanelOpenedPerSessionHistogramName,
        /*sample=*/1, /*expected_count=*/i + 1);
  }
}

TEST_F(MahiUiControllerWithSessionTest, PanelCloseOnSessionStateChanged) {
  std::vector<session_manager::SessionState> non_active_session_states = {
      session_manager::SessionState::UNKNOWN,
      session_manager::SessionState::OOBE,
      session_manager::SessionState::LOGIN_PRIMARY,
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE,
      session_manager::SessionState::LOCKED,
      session_manager::SessionState::LOGIN_SECONDARY,
      session_manager::SessionState::RMA};

  SessionInfo info;
  for (size_t i = 0; i < non_active_session_states.size(); i++) {
    info.state = session_manager::SessionState::ACTIVE;
    Shell::Get()->session_controller()->SetSessionInfo(info);

    ui_controller()->OpenMahiPanel(GetPrimaryDisplay().id(), gfx::Rect());
    EXPECT_TRUE(ui_controller()->IsMahiPanelOpen());

    // Set the session to a non-active state, the panel should be closed.
    auto state = non_active_session_states[i];
    info.state = state;
    Shell::Get()->session_controller()->SetSessionInfo(info);
    base::test::RunUntil([&state] {
      return Shell::Get()->session_controller()->GetSessionState() == state;
    });
    EXPECT_FALSE(ui_controller()->IsMahiPanelOpen());
  }
}

TEST_F(MahiUiControllerWithSessionTest, PanelCloseOnActiveUserChanged) {
  // Set up two users, user1 is the active user.
  UpdateSession(1u, "user1@test.com");
  UpdateSession(2u, "user2@test.com");
  std::vector<uint32_t> order = {1u, 2u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::test::RunUntil(
      [&] { return Shell::Get()->session_controller()->IsUserPrimary(); });

  ui_controller()->OpenMahiPanel(GetPrimaryDisplay().id(), gfx::Rect());
  EXPECT_TRUE(ui_controller()->IsMahiPanelOpen());

  // Make user2 the active user, the panel should be closed.
  order = {2u, 1u};
  SessionController::Get()->SetUserSessionOrder(order);
  base::test::RunUntil(
      [&] { return !Shell::Get()->session_controller()->IsUserPrimary(); });
  EXPECT_FALSE(ui_controller()->IsMahiPanelOpen());
}

}  // namespace ash
