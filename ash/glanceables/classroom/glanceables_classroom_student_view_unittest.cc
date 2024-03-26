// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/common/test/glanceables_test_new_window_delegate.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/shell.h"
#include "ash/style/combobox.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::_;

class TestClient : public GlanceablesClassroomClient {
 public:
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
  GlanceablesClassroomStudentViewTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlanceablesV2},
        /*disabled_features=*/{});
  }

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
    return views::AsViewClass<views::View>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kClassroomBubbleHeaderIcon)));
  }

  Combobox* GetComboBoxView() {
    return views::AsViewClass<Combobox>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kClassroomBubbleComboBox)));
  }

  const views::View* GetListContainerView() const {
    return views::AsViewClass<views::View>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kClassroomBubbleListContainer)));
  }

  const views::View* GetEmptyListLabel() const {
    return views::AsViewClass<views::View>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kClassroomBubbleEmptyListLabel)));
  }

  const views::Label* GetListFooterItemsCountLabel() const {
    return views::AsViewClass<views::Label>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kListFooterItemsCountLabel)));
  }

  const views::View* GetListFooter() const {
    return views::AsViewClass<views::View>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kClassroomBubbleListFooter)));
  }

  views::LabelButton* GetListFooterSeeAllButton() const {
    return views::AsViewClass<views::LabelButton>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kListFooterSeeAllButton)));
  }

  const views::ProgressBar* GetProgressBar() const {
    return views::AsViewClass<views::ProgressBar>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kProgressBar)));
  }

  const views::View* GetMessageError() const {
    return views::AsViewClass<views::View>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kGlanceablesErrorMessageView)));
  }

  const views::LabelButton* GetMessageErrorDismissButton() const {
    return views::AsViewClass<views::LabelButton>(
        view_->GetViewByID(base::to_underlying(
            GlanceablesViewId::kGlanceablesErrorMessageButton)));
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
  base::test::ScopedFeatureList feature_list_;
  AccountId account_id_ = AccountId::FromUserEmail("test_user@gmail.com");
};

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
  EXPECT_TRUE(GetListFooter()->GetVisible());
  histogram_tester.ExpectUniqueSample(
      "Ash.Glanceables.Classroom.Student.ListSelected", 0,
      /*expected_bucket_count=*/0);

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithoutDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(3));
      });

  GetComboBoxView()->SelectMenuItemForTest(1);
  // Trigger layout after receiving new items.
  widget_->LayoutRootViewIfNecessary();
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.Classroom.Student.ListSelected", 1,
      /*expected_count=*/1);
  EXPECT_TRUE(GetListFooter()->GetVisible());
  LeftClickOn(GetListFooterSeeAllButton());
  EXPECT_EQ(new_window_delegate_.GetLastOpenedUrl(),
            "https://classroom.google.com/u/0/a/not-turned-in/all");

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithMissedDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(3));
      });
  GetComboBoxView()->SelectMenuItemForTest(2);
  // Trigger layout after receiving new items.
  widget_->LayoutRootViewIfNecessary();
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.Classroom.Student.ListSelected", 2,
      /*expected_count=*/1);
  LeftClickOn(GetListFooterSeeAllButton());
  EXPECT_EQ(new_window_delegate_.GetLastOpenedUrl(),
            "https://classroom.google.com/u/0/a/missing/all");

  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(3));
      });
  GetComboBoxView()->SelectMenuItemForTest(3);
  histogram_tester.ExpectBucketCount(
      "Ash.Glanceables.Classroom.Student.ListSelected", 3,
      /*expected_count=*/1);
  // Trigger layout after receiving new items.
  widget_->LayoutRootViewIfNecessary();

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
        std::move(cb).Run(/*success=*/true, CreateAssignments(5));
      });
  ASSERT_TRUE(GetComboBoxView());
  ASSERT_TRUE(GetListContainerView());
  EXPECT_TRUE(GetListFooter()->GetVisible());

  GetComboBoxView()->SelectMenuItemForTest(3);
  EXPECT_EQ(GetListContainerView()->children().size(), 3u);  // No more than 3.

  EXPECT_TRUE(GetListFooter()->GetVisible());
  ASSERT_TRUE(GetListFooterItemsCountLabel());
  EXPECT_EQ(GetListFooterItemsCountLabel()->GetText(), u"Showing 3 out of 5");
}

TEST_F(GlanceablesClassroomStudentViewTest, RendersEmptyListLabel) {
  ASSERT_TRUE(GetComboBoxView());
  ASSERT_TRUE(GetListContainerView());
  EXPECT_FALSE(GetEmptyListLabel()->GetVisible());
  EXPECT_TRUE(GetListFooter()->GetVisible());
  EXPECT_EQ(GetListFooterItemsCountLabel()->GetText(), u"Showing 1 out of 1");
  EXPECT_EQ(GetListContainerView()->children().size(), 1u);

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithoutDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, {});
      });
  GetComboBoxView()->SelectMenuItemForTest(1);
  EXPECT_EQ(GetListContainerView()->children().size(), 0u);

  // The empty list label should be shown, and the footer hidden.
  EXPECT_FALSE(GetListFooter()->GetVisible());
  EXPECT_TRUE(GetEmptyListLabel()->GetVisible());

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithMissedDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, {});
      });
  GetComboBoxView()->SelectMenuItemForTest(2);
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
        EXPECT_FALSE(GetMessageError());

        std::move(cb).Run(/*success=*/false, {});

        // Error message is created and visible after receiving unsuccessful
        // response.
        ASSERT_TRUE(GetMessageError());
        EXPECT_TRUE(GetMessageError()->GetVisible());
      });

  ASSERT_FALSE(GetMessageError());
  ASSERT_TRUE(GetComboBoxView());
  GetComboBoxView()->SelectMenuItemForTest(3);
}

TEST_F(GlanceablesClassroomStudentViewTest, DismissErrorMessageBubble) {
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([&](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/false, {});
        ASSERT_TRUE(GetMessageError()->GetVisible());
      });

  ASSERT_FALSE(GetMessageError());
  ASSERT_TRUE(GetComboBoxView());
  GetComboBoxView()->SelectMenuItemForTest(3);

  // Trigger layout after receiving unsuccessful response.
  widget_->LayoutRootViewIfNecessary();
  ASSERT_TRUE(GetMessageError());

  LeftClickOn(GetMessageErrorDismissButton());
  EXPECT_FALSE(GetMessageError());
}

TEST_F(GlanceablesClassroomStudentViewTest,
       DismissErrorMessageBubbleAfterSuccess) {
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([&](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/false, {});
      });

  ASSERT_FALSE(GetMessageError());
  ASSERT_TRUE(GetComboBoxView());
  GetComboBoxView()->SelectMenuItemForTest(3);

  // Trigger layout after receiving unsuccessful response.
  widget_->LayoutRootViewIfNecessary();
  ASSERT_TRUE(GetMessageError());

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithoutDueDate(_))
      .WillOnce([&](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run(/*success=*/true, CreateAssignments(1));
      });
  GetComboBoxView()->SelectMenuItemForTest(1);

  // Trigger layout after receiving successful response.
  widget_->LayoutRootViewIfNecessary();
  EXPECT_FALSE(GetMessageError());
}

}  // namespace ash
