// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/common/test/glanceables_test_new_window_delegate.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/combobox.h"
#include "ash/style/counter_expand_button.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::_;

class TestClient : public GlanceablesClassroomClient {
 public:
  bool IsDisabledByAdmin() const override { return false; }

  MOCK_METHOD(void,
              IsStudentRoleActive,
              (GlanceablesClassroomClient::IsRoleEnabledCallback),
              (override));

  MOCK_METHOD(void,
              GetCompletedStudentAssignments,
              (GlanceablesClassroomClient::GetAssignmentsCallback),
              (override));
  MOCK_METHOD(void,
              GetStudentAssignmentsWithApproachingDueDate,
              (GlanceablesClassroomClient::GetAssignmentsCallback),
              (override));
  MOCK_METHOD(void,
              GetStudentAssignmentsWithMissedDueDate,
              (GlanceablesClassroomClient::GetAssignmentsCallback),
              (override));
  MOCK_METHOD(void,
              GetStudentAssignmentsWithoutDueDate,
              (GlanceablesClassroomClient::GetAssignmentsCallback),
              (override));

  MOCK_METHOD(void, OnGlanceablesBubbleClosed, (), (override));
};

std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> CreateAssignments(
    int count) {
  std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments;
  for (int i = 0; i < count; ++i) {
    assignments.push_back(std::make_unique<GlanceablesClassroomAssignment>(
        "Course title", base::StringPrintf("Course work title %d", i + 1),
        GURL(base::StringPrintf("https://classroom.google.com/test-link-%d",
                                i + 1)),
        std::nullopt, base::Time(), std::nullopt));
  }
  return assignments;
}

}  // namespace

class GlanceablesClassroomStudentViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    SimulateUserLogin(account_id_);
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesController::ClientsRegistration{
                         .classroom_client = &classroom_client_});
    ASSERT_TRUE(Shell::Get()->glanceables_controller()->GetClassroomClient());

    // `view_` gets student assignments with approaching due date during
    // initialization.
    EXPECT_CALL(classroom_client_,
                GetStudentAssignmentsWithApproachingDueDate(_))
        .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
          std::move(cb).Run(/*success=*/true, CreateAssignments(1));
        });

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    view_ = widget_->SetContentsView(
        std::make_unique<GlanceablesClassroomStudentView>());
  }

  const views::View* GetHeaderIcon() const {
    return views::AsViewClass<views::View>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleHeaderIcon)));
  }

  Combobox* GetComboBoxView() {
    return views::AsViewClass<Combobox>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTimeManagementBubbleComboBox)));
  }

  views::ScrollView* GetScrollView() const {
    return views::AsViewClass<views::ScrollView>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kContentsScrollView)));
  }

  const CounterExpandButton* GetCounterExpandButton() const {
    return views::AsViewClass<CounterExpandButton>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleExpandButton)));
  }

  const views::View* GetListContainerView() const {
    return views::AsViewClass<views::View>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleListContainer)));
  }

  const views::View* GetEmptyListLabel() const {
    return views::AsViewClass<views::View>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kClassroomBubbleEmptyListLabel)));
  }

  views::View* GetListFooter() const {
    return views::AsViewClass<views::View>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementBubbleListFooter)));
  }

  const views::Label* GetListFooterLabel() const {
    return views::AsViewClass<views::Label>(GetListFooter()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kListFooterTitleLabel)));
  }

  const views::LabelButton* GetListFooterSeeAllButton() const {
    return views::AsViewClass<views::LabelButton>(GetListFooter()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kListFooterSeeAllButton)));
  }

  const views::ProgressBar* GetProgressBar() const {
    return views::AsViewClass<views::ProgressBar>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kProgressBar)));
  }

  const ErrorMessageToast* GetErrorMessage() const {
    return views::AsViewClass<ErrorMessageToast>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kTimeManagementErrorMessageToast)));
  }

  const views::LabelButton* GetMessageErrorDismissButton() const {
    return GetErrorMessage()->GetButtonForTest();
  }

  int GetLastSelectedAssignmentsListPrefValue() const {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetInteger(
            "ash.glanceables.classroom.student.last_selected_assignments_list");
  }

 protected:
  testing::StrictMock<TestClient> classroom_client_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<GlanceablesClassroomStudentView> view_;
  const GlanceablesTestNewWindowDelegate new_window_delegate_;

 private:
  AccountId account_id_ = AccountId::FromUserEmail("test_user@gmail.com");
};

TEST_F(GlanceablesClassroomStudentViewTest, Basics) {
  // Check that `GlanceablesClassroomStudentView` by itself doesn't have a
  // background.
  EXPECT_FALSE(view_->GetBackground());

  // Check that the expand button is not visible when
  // `GlanceablesClassroomStudentView` is created alone.
  auto* expand_button = view_->GetViewByID(base::to_underlying(
      GlanceablesViewId::kTimeManagementBubbleExpandButton));
  EXPECT_TRUE(expand_button);
  EXPECT_FALSE(expand_button->GetVisible());
}

TEST_F(GlanceablesClassroomStudentViewTest, RendersComboBoxView) {
  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithoutDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, {});
      });
  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithMissedDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, {});
      });
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, {});
      });
  Combobox* combobox_view = GetComboBoxView();
  ASSERT_TRUE(combobox_view);
  combobox_view->RequestFocus();
  ASSERT_TRUE(combobox_view->GetSelectedIndex());
  EXPECT_EQ(0u, *combobox_view->GetSelectedIndex());
  EXPECT_EQ(u"Due soon", combobox_view->GetTextForRow(0u));
  EXPECT_EQ(0, GetLastSelectedAssignmentsListPrefValue());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  ASSERT_TRUE(combobox_view->GetSelectedIndex());
  EXPECT_EQ(1u, *combobox_view->GetSelectedIndex());
  EXPECT_EQ(u"No due date", combobox_view->GetTextForRow(1u));
  EXPECT_EQ(1, GetLastSelectedAssignmentsListPrefValue());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  ASSERT_TRUE(combobox_view->GetSelectedIndex());
  EXPECT_EQ(2u, *combobox_view->GetSelectedIndex());
  EXPECT_EQ(u"Missing", combobox_view->GetTextForRow(2u));
  EXPECT_EQ(2, GetLastSelectedAssignmentsListPrefValue());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  ASSERT_TRUE(combobox_view->GetSelectedIndex());
  EXPECT_EQ(3u, *combobox_view->GetSelectedIndex());
  EXPECT_EQ(u"Done", combobox_view->GetTextForRow(3u));
  EXPECT_EQ(3, GetLastSelectedAssignmentsListPrefValue());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  ASSERT_TRUE(combobox_view->GetSelectedIndex());
  EXPECT_EQ(3u, *combobox_view->GetSelectedIndex());
}

TEST_F(GlanceablesClassroomStudentViewTest,
       ScrollViewResetPositionAfterSwitchingLists) {
  auto* scroll_bar = GetScrollView()->vertical_scroll_bar();

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithoutDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(101));
      });
  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithMissedDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, {CreateAssignments(101)});
      });

  GetComboBoxView()->SelectMenuItemForTest(1);

  EXPECT_EQ(scroll_bar->GetPosition(), scroll_bar->GetMinPosition());
  ASSERT_TRUE(scroll_bar->GetVisible());
  scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kEnd);
  EXPECT_GT(scroll_bar->GetPosition(), scroll_bar->GetMinPosition());

  GetComboBoxView()->SelectMenuItemForTest(2);
  EXPECT_EQ(scroll_bar->GetPosition(), scroll_bar->GetMinPosition());
}

TEST_F(GlanceablesClassroomStudentViewTest, RecordShowTimeHistogramOnClose) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Classroom.TotalShowTime", 0);
  view_ = nullptr;
  widget_.reset();
  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Classroom.TotalShowTime", 1);
}

TEST_F(GlanceablesClassroomStudentViewTest,
       ReadsInitialComboBoxViewValueFromPrefs) {
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .Times(2)
      .WillRepeatedly(
          [](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
            std::move(cb).Run(/*success=*/true, {});
          });

  ASSERT_TRUE(GetComboBoxView());

  // The first menu item is selected initially.
  EXPECT_EQ(GetLastSelectedAssignmentsListPrefValue(), 0);
  EXPECT_EQ(GetComboBoxView()->GetSelectedIndex(), 0u);

  // Update selection in the `combobox_view`, this should update prefs.
  GetComboBoxView()->SelectMenuItemForTest(3u);
  EXPECT_EQ(GetLastSelectedAssignmentsListPrefValue(), 3);
  EXPECT_EQ(GetComboBoxView()->GetSelectedIndex(), 3u);

  // Swap `widget_`'s content. Verify that the new `view_` contains a combobox
  // with the correct initial value.
  widget_->GetRootView()->RemoveChildViewT(std::exchange(view_, nullptr));
  view_ = widget_->SetContentsView(
      std::make_unique<GlanceablesClassroomStudentView>());
  ASSERT_TRUE(GetComboBoxView());
  EXPECT_EQ(GetLastSelectedAssignmentsListPrefValue(), 3);
  EXPECT_EQ(GetComboBoxView()->GetSelectedIndex(), 3u);
}

TEST_F(GlanceablesClassroomStudentViewTest,
       CallsClassroomClientAfterChangingActiveList) {
  base::UserActionTester user_actions;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(GetComboBoxView());
  ASSERT_TRUE(GetListFooterSeeAllButton());
  histogram_tester.ExpectUniqueSample(
      "Ash.Glanceables.Classroom.Student.ListSelected", 0,
      /*expected_bucket_count=*/0);

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithoutDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(101));
      });

  GetComboBoxView()->SelectMenuItemForTest(1);
  // Trigger layout after receiving new items.
  widget_->LayoutRootViewIfNecessary();
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.Classroom.Student.ListSelected", 1,
      /*expected_count=*/1);
  EXPECT_TRUE(GetListFooter()->GetVisible());
  GetListFooter()->ScrollViewToVisible();
  LeftClickOn(GetListFooterSeeAllButton());
  EXPECT_EQ(new_window_delegate_.GetLastOpenedUrl(),
            "https://classroom.google.com/u/0/a/not-turned-in/all");

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithMissedDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(101));
      });
  GetComboBoxView()->SelectMenuItemForTest(2);
  // Trigger layout after receiving new items.
  widget_->LayoutRootViewIfNecessary();
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.Classroom.Student.ListSelected", 2,
      /*expected_count=*/1);
  GetListFooter()->ScrollViewToVisible();
  LeftClickOn(GetListFooterSeeAllButton());
  EXPECT_EQ(new_window_delegate_.GetLastOpenedUrl(),
            "https://classroom.google.com/u/0/a/missing/all");

  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(101));
      });
  GetComboBoxView()->SelectMenuItemForTest(3);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.Classroom.Student.ListSelected", 3,
      /*expected_count=*/1);
  // Trigger layout after receiving new items.
  widget_->LayoutRootViewIfNecessary();
  GetListFooter()->ScrollViewToVisible();
  LeftClickOn(GetListFooterSeeAllButton());
  EXPECT_EQ(new_window_delegate_.GetLastOpenedUrl(),
            "https://classroom.google.com/u/0/a/turned-in/all");

  // Switch from the final assignment list back to no due date list.
  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithoutDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(3));
      });
  GetComboBoxView()->SelectMenuItemForTest(1);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.Classroom.Student.ListSelected", 1,
      /*expected_count=*/2);

  EXPECT_EQ(3,
            user_actions.GetActionCount("Glanceables_Classroom_SeeAllPressed"));
  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.Classroom.Student.AssignmentListShownTime.DefaultList."
      "Assigned",
      1);
  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.Classroom.Student.AssignmentListShownTime.ChangedList."
      "NoDueDate",
      1);
  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.Classroom.Student.AssignmentListShownTime.ChangedList."
      "Missing",
      1);
  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.Classroom.Student.AssignmentListShownTime.ChangedList."
      "Done",
      1);
}

TEST_F(GlanceablesClassroomStudentViewTest, RendersListItems) {
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(101));
      });
  ASSERT_TRUE(GetComboBoxView());
  ASSERT_TRUE(GetListContainerView());

  GetComboBoxView()->SelectMenuItemForTest(3);
  EXPECT_EQ(GetCounterExpandButton()->counter_for_test(), 100u);
  EXPECT_EQ(GetListContainerView()->children().size(), 100u);

  EXPECT_TRUE(GetListFooter()->GetVisible());
  GetListFooter()->ScrollViewToVisible();
  ASSERT_TRUE(GetListFooterLabel());
  EXPECT_EQ(GetListFooterLabel()->GetText(),
            u"See all assignments in Google Classroom");
}

TEST_F(GlanceablesClassroomStudentViewTest, RendersEmptyListLabel) {
  ASSERT_TRUE(GetComboBoxView());
  ASSERT_TRUE(GetListContainerView());
  EXPECT_FALSE(GetEmptyListLabel()->GetVisible());
  EXPECT_FALSE(GetListFooter()->GetVisible());
  EXPECT_EQ(GetCounterExpandButton()->counter_for_test(), 1u);
  EXPECT_EQ(GetListContainerView()->children().size(), 1u);

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithoutDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, {});
      });
  GetComboBoxView()->SelectMenuItemForTest(1);
  EXPECT_EQ(GetCounterExpandButton()->counter_for_test(), 0u);
  EXPECT_EQ(GetListContainerView()->children().size(), 0u);

  // The empty list label should be shown, and the footer hidden.
  EXPECT_FALSE(GetListFooter()->GetVisible());
  EXPECT_TRUE(GetEmptyListLabel()->GetVisible());

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithMissedDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, {});
      });
  GetComboBoxView()->SelectMenuItemForTest(2);
  EXPECT_EQ(GetCounterExpandButton()->counter_for_test(), 0u);
  EXPECT_EQ(GetListContainerView()->children().size(), 0u);

  // The empty list label should be shown, and the footer hidden.
  EXPECT_FALSE(GetListFooter()->GetVisible());
  EXPECT_TRUE(GetEmptyListLabel()->GetVisible());
}

TEST_F(GlanceablesClassroomStudentViewTest, OpensClassroomUrlForListItem) {
  base::UserActionTester user_actions;
  base::HistogramTester histogram_tester;
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(1));
      });
  ASSERT_TRUE(GetComboBoxView());
  GetComboBoxView()->SelectMenuItemForTest(3);

  // Trigger layout for `GetListContainerView()` after receiving new items.
  widget_->LayoutRootViewIfNecessary();

  ASSERT_TRUE(GetListContainerView());
  ASSERT_EQ(GetListContainerView()->children().size(), 1u);

  LeftClickOn(GetListContainerView()->children().at(0));
  EXPECT_EQ(new_window_delegate_.GetLastOpenedUrl(),
            "https://classroom.google.com/test-link-1");

  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Classroom_AssignmentPressed"));
  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Classroom.UserAction", 2);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.TimeManagement.Classroom.UserAction", 0,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.TimeManagement.Classroom.UserAction", 2,
      /*expected_count=*/1);
}

TEST_F(GlanceablesClassroomStudentViewTest, ShowsProgressBar) {
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([&](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        // Progress bar is visible before replying to pending request.
        EXPECT_TRUE(GetProgressBar()->GetVisible());

        std::move(cb).Run(/*success=*/true, {});

        // Progress bar is hidden after replying to pending request.
        EXPECT_FALSE(GetProgressBar()->GetVisible());
      });

  ASSERT_TRUE(GetProgressBar());
  ASSERT_TRUE(GetComboBoxView());
  GetComboBoxView()->SelectMenuItemForTest(3);
}

TEST_F(GlanceablesClassroomStudentViewTest, ClickHeaderIconButton) {
  base::UserActionTester user_actions;
  base::HistogramTester histogram_tester;

  LeftClickOn(GetHeaderIcon());
  EXPECT_EQ(new_window_delegate_.GetLastOpenedUrl(),
            "https://classroom.google.com/u/0/h");

  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Classroom_HeaderIconPressed"));
  histogram_tester.ExpectUniqueSample(
      "Ash.Glanceables.TimeManagement.Classroom.UserAction", 1,
      /*expected_bucket_count=*/1);
}

TEST_F(GlanceablesClassroomStudentViewTest, ClickItemViewUserAction) {
  base::UserActionTester user_actions;
  base::HistogramTester histogram_tester;

  LeftClickOn(GetListContainerView()->children().at(0));
  EXPECT_EQ(new_window_delegate_.GetLastOpenedUrl(),
            "https://classroom.google.com/test-link-1");

  EXPECT_EQ(1, user_actions.GetActionCount(
                   "Glanceables_Classroom_AssignmentPressed_DefaultList"));

  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(2));
      });
  ASSERT_TRUE(GetComboBoxView());
  GetComboBoxView()->SelectMenuItemForTest(3);

  LeftClickOn(GetListContainerView()->children().at(1));
  EXPECT_EQ(new_window_delegate_.GetLastOpenedUrl(),
            "https://classroom.google.com/test-link-2");

  EXPECT_EQ(2, user_actions.GetActionCount(
                   "Glanceables_Classroom_AssignmentPressed"));
  histogram_tester.ExpectUniqueSample(
      "Ash.Glanceables.Classroom.Student.ListSelected", 3,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Ash.Glanceables.TimeManagement.Classroom.UserAction", 3);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.TimeManagement.Classroom.UserAction", 0,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.TimeManagement.Classroom.UserAction", 2,
      /*expected_count=*/2);
}

TEST_F(GlanceablesClassroomStudentViewTest, ShowErrorMessageBubble) {
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([&](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        // Error message is not initialized before replying to pending request.
        EXPECT_FALSE(GetErrorMessage());

        std::move(cb).Run(/*success=*/false, {});

        // Error message is created and visible after receiving unsuccessful
        // response.
        ASSERT_TRUE(GetErrorMessage());
        EXPECT_TRUE(GetErrorMessage()->GetVisible());
      });

  ASSERT_FALSE(GetErrorMessage());
  ASSERT_TRUE(GetComboBoxView());
  GetComboBoxView()->SelectMenuItemForTest(3);
}

TEST_F(GlanceablesClassroomStudentViewTest, DismissErrorMessageBubble) {
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([&](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/false, {});
        ASSERT_TRUE(GetErrorMessage()->GetVisible());
      });

  ASSERT_FALSE(GetErrorMessage());
  ASSERT_TRUE(GetComboBoxView());
  GetComboBoxView()->SelectMenuItemForTest(3);

  // Trigger layout after receiving unsuccessful response.
  widget_->LayoutRootViewIfNecessary();
  ASSERT_TRUE(GetErrorMessage());

  LeftClickOn(GetMessageErrorDismissButton());
  EXPECT_FALSE(GetErrorMessage());
}

TEST_F(GlanceablesClassroomStudentViewTest,
       DismissErrorMessageBubbleAfterSuccess) {
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([&](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/false, {});
      });

  ASSERT_FALSE(GetErrorMessage());
  ASSERT_TRUE(GetComboBoxView());
  GetComboBoxView()->SelectMenuItemForTest(3);

  // Trigger layout after receiving unsuccessful response.
  widget_->LayoutRootViewIfNecessary();
  ASSERT_TRUE(GetErrorMessage());

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithoutDueDate(_))
      .WillOnce([&](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(1));
      });
  GetComboBoxView()->SelectMenuItemForTest(1);

  // Trigger layout after receiving successful response.
  widget_->LayoutRootViewIfNecessary();
  EXPECT_FALSE(GetErrorMessage());
}

}  // namespace ash
