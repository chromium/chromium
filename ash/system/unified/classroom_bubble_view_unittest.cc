// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/unified/classroom_bubble_base_view.h"
#include "ash/system/unified/classroom_bubble_student_view.h"
#include "ash/system/unified/classroom_bubble_teacher_view.h"
#include "ash/test/ash_test_base.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
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
              IsTeacherRoleActive,
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

  MOCK_METHOD(void,
              GetTeacherAssignmentsWithApproachingDueDate,
              (GlanceablesClassroomClient::GetAssignmentsCallback),
              (override));
  MOCK_METHOD(void,
              GetTeacherAssignmentsRecentlyDue,
              (GlanceablesClassroomClient::GetAssignmentsCallback),
              (override));
  MOCK_METHOD(void,
              GetTeacherAssignmentsWithoutDueDate,
              (GlanceablesClassroomClient::GetAssignmentsCallback),
              (override));
  MOCK_METHOD(void,
              GetGradedTeacherAssignments,
              (GlanceablesClassroomClient::GetAssignmentsCallback),
              (override));

  MOCK_METHOD(void, OpenUrl, (const GURL&), (const override));
  MOCK_METHOD(void, OnGlanceablesBubbleClosed, (), (override));
};

}  // namespace

class ClassroomBubbleViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    SimulateUserLogin(account_id_);
    Shell::Get()->glanceables_v2_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesV2Controller::ClientsRegistration{
                         .classroom_client = &classroom_client_});
    ASSERT_TRUE(
        Shell::Get()->glanceables_v2_controller()->GetClassroomClient());

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  views::Combobox* GetComboBoxView() {
    return views::AsViewClass<views::Combobox>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kClassroomBubbleComboBox)));
  }

  const views::View* GetListContainerView() const {
    return views::AsViewClass<views::View>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kClassroomBubbleListContainer)));
  }

  const views::Label* GetListFooterItemsCountLabel() const {
    return views::AsViewClass<views::Label>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kListFooterItemsCountLabel)));
  }

  views::LabelButton* GetListFooterSeeAllButton() const {
    return views::AsViewClass<views::LabelButton>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kListFooterSeeAllButton)));
  }

 protected:
  testing::StrictMock<TestClient> classroom_client_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<ClassroomBubbleBaseView, ExperimentalAsh> view_;
  DetailedViewDelegate detailed_view_delegate_{nullptr};

 private:
  base::test::ScopedFeatureList feature_list_{features::kGlanceablesV2};
  AccountId account_id_ = AccountId::FromUserEmail("test_user@gmail.com");
};

class ClassroomBubbleStudentViewTest : public ClassroomBubbleViewTest {
 public:
  void SetUp() override {
    ClassroomBubbleViewTest::SetUp();
    // `view_` gets student assignments with approaching due date during
    // initialization.
    EXPECT_CALL(classroom_client_,
                GetStudentAssignmentsWithApproachingDueDate(_))
        .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
          std::move(cb).Run({});
        });
    view_ = widget_->SetContentsView(
        std::make_unique<ClassroomBubbleStudentView>(&detailed_view_delegate_));
  }
};

class ClassroomBubbleTeacherViewTest : public ClassroomBubbleViewTest {
 public:
  void SetUp() override {
    ClassroomBubbleViewTest::SetUp();
    // `view_` gets teacher assignments with approaching due date during
    // initialization.
    EXPECT_CALL(classroom_client_,
                GetTeacherAssignmentsWithApproachingDueDate(_))
        .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
          std::move(cb).Run({});
        });
    view_ = widget_->SetContentsView(
        std::make_unique<ClassroomBubbleTeacherView>(&detailed_view_delegate_));
  }
};

TEST_F(ClassroomBubbleStudentViewTest, RendersComboBoxView) {
  ASSERT_TRUE(GetComboBoxView());
  ASSERT_EQ(GetComboBoxView()->GetModel()->GetItemCount(), 4u);

  EXPECT_EQ(GetComboBoxView()->GetModel()->GetItemAt(0), u"Assigned");
  EXPECT_EQ(GetComboBoxView()->GetModel()->GetItemAt(1), u"No due date");
  EXPECT_EQ(GetComboBoxView()->GetModel()->GetItemAt(2), u"Missing");
  EXPECT_EQ(GetComboBoxView()->GetModel()->GetItemAt(3), u"Done");

  EXPECT_EQ(GetComboBoxView()->GetSelectedIndex(), 0u);
}

TEST_F(ClassroomBubbleTeacherViewTest, RendersComboBoxView) {
  ASSERT_TRUE(GetComboBoxView());
  ASSERT_EQ(GetComboBoxView()->GetModel()->GetItemCount(), 4u);

  EXPECT_EQ(GetComboBoxView()->GetModel()->GetItemAt(0), u"Due Soon");
  EXPECT_EQ(GetComboBoxView()->GetModel()->GetItemAt(1), u"Recently Due");
  EXPECT_EQ(GetComboBoxView()->GetModel()->GetItemAt(2), u"No Due Date");
  EXPECT_EQ(GetComboBoxView()->GetModel()->GetItemAt(3), u"Graded");

  EXPECT_EQ(GetComboBoxView()->GetSelectedIndex(), 0u);
}

TEST_F(ClassroomBubbleStudentViewTest,
       CallsClassroomClientAfterChangingActiveList) {
  ASSERT_TRUE(GetComboBoxView());
  ASSERT_TRUE(GetListFooterSeeAllButton());

  EXPECT_CALL(
      classroom_client_,
      OpenUrl(GURL("https://classroom.google.com/u/0/a/not-turned-in/all")));
  LeftClickOn(GetListFooterSeeAllButton());

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithoutDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run({});
      });
  GetComboBoxView()->MenuSelectionAt(1);
  EXPECT_CALL(
      classroom_client_,
      OpenUrl(GURL("https://classroom.google.com/u/0/a/not-turned-in/all")));
  LeftClickOn(GetListFooterSeeAllButton());

  EXPECT_CALL(classroom_client_, GetStudentAssignmentsWithMissedDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run({});
      });
  GetComboBoxView()->MenuSelectionAt(2);
  EXPECT_CALL(classroom_client_,
              OpenUrl(GURL("https://classroom.google.com/u/0/a/missing/all")));
  LeftClickOn(GetListFooterSeeAllButton());

  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run({});
      });
  GetComboBoxView()->MenuSelectionAt(3);
  EXPECT_CALL(
      classroom_client_,
      OpenUrl(GURL("https://classroom.google.com/u/0/a/turned-in/all")));
  LeftClickOn(GetListFooterSeeAllButton());
}

TEST_F(ClassroomBubbleTeacherViewTest,
       CallsClassroomClientAfterChangingActiveList) {
  ASSERT_TRUE(GetComboBoxView());
  ASSERT_TRUE(GetListFooterSeeAllButton());

  EXPECT_CALL(
      classroom_client_,
      OpenUrl(GURL("https://classroom.google.com/u/0/ta/not-reviewed/all")));
  LeftClickOn(GetListFooterSeeAllButton());

  EXPECT_CALL(classroom_client_, GetTeacherAssignmentsRecentlyDue(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run({});
      });
  GetComboBoxView()->MenuSelectionAt(1);
  EXPECT_CALL(
      classroom_client_,
      OpenUrl(GURL("https://classroom.google.com/u/0/ta/not-reviewed/all")));
  LeftClickOn(GetListFooterSeeAllButton());

  EXPECT_CALL(classroom_client_, GetTeacherAssignmentsWithoutDueDate(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run({});
      });
  GetComboBoxView()->MenuSelectionAt(2);
  EXPECT_CALL(
      classroom_client_,
      OpenUrl(GURL("https://classroom.google.com/u/0/ta/not-reviewed/all")));
  LeftClickOn(GetListFooterSeeAllButton());

  EXPECT_CALL(classroom_client_, GetGradedTeacherAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::move(cb).Run({});
      });
  GetComboBoxView()->MenuSelectionAt(3);
  EXPECT_CALL(
      classroom_client_,
      OpenUrl(GURL("https://classroom.google.com/u/0/ta/reviewed/all")));
  LeftClickOn(GetListFooterSeeAllButton());
}

TEST_F(ClassroomBubbleStudentViewTest, RendersListItems) {
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
            assignments;
        for (size_t i = 0; i < 5; ++i) {
          assignments.push_back(
              std::make_unique<GlanceablesClassroomAssignment>(
                  "Course title",
                  base::StringPrintf("Course work title %zu", i + 1),
                  GURL(base::StringPrintf(
                      "https://classroom.google.com/test-link-%zu", i + 1)),
                  absl::nullopt, absl::nullopt));
        }
        std::move(cb).Run(std::move(assignments));
      });
  ASSERT_TRUE(GetComboBoxView());
  ASSERT_TRUE(GetListContainerView());

  GetComboBoxView()->MenuSelectionAt(3);
  EXPECT_EQ(GetListContainerView()->children().size(), 3u);  // No more than 3.

  ASSERT_TRUE(GetListFooterItemsCountLabel());
  EXPECT_EQ(GetListFooterItemsCountLabel()->GetText(), u"Showing 3 out of 5");
}

TEST_F(ClassroomBubbleTeacherViewTest, RendersListItems) {
  EXPECT_CALL(classroom_client_, GetGradedTeacherAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
            assignments;
        for (size_t i = 0; i < 5; ++i) {
          assignments.push_back(
              std::make_unique<GlanceablesClassroomAssignment>(
                  "Course title",
                  base::StringPrintf("Course work title %zu", i + 1),
                  GURL(base::StringPrintf(
                      "https://classroom.google.com/test-link-%zu", i + 1)),
                  absl::nullopt,
                  GlanceablesClassroomAggregatedSubmissionsState(0, 0, 0)));
        }
        std::move(cb).Run(std::move(assignments));
      });
  ASSERT_TRUE(GetComboBoxView());
  ASSERT_TRUE(GetListContainerView());

  GetComboBoxView()->MenuSelectionAt(3);
  EXPECT_EQ(GetListContainerView()->children().size(), 3u);  // No more than 3.

  ASSERT_TRUE(GetListFooterItemsCountLabel());
  EXPECT_EQ(GetListFooterItemsCountLabel()->GetText(), u"Showing 3 out of 5");
}

TEST_F(ClassroomBubbleStudentViewTest, OpensClassroomUrlForListItem) {
  EXPECT_CALL(classroom_client_, GetCompletedStudentAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
            assignments;
        assignments.push_back(std::make_unique<GlanceablesClassroomAssignment>(
            "Course title", "Course work title",
            GURL("https://classroom.google.com/test-link"), absl::nullopt,
            absl::nullopt));
        std::move(cb).Run(std::move(assignments));
      });
  ASSERT_TRUE(GetComboBoxView());
  GetComboBoxView()->MenuSelectionAt(3);

  // Trigger layout for `GetListContainerView()` after receiving new items.
  widget_->LayoutRootViewIfNecessary();

  ASSERT_TRUE(GetListContainerView());
  ASSERT_EQ(GetListContainerView()->children().size(), 1u);

  EXPECT_CALL(classroom_client_,
              OpenUrl(GURL("https://classroom.google.com/test-link")));
  LeftClickOn(GetListContainerView()->children().at(0));
}

TEST_F(ClassroomBubbleTeacherViewTest, OpensClassroomUrlForListItem) {
  EXPECT_CALL(classroom_client_, GetGradedTeacherAssignments(_))
      .WillOnce([](GlanceablesClassroomClient::GetAssignmentsCallback cb) {
        std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
            assignments;
        assignments.push_back(std::make_unique<GlanceablesClassroomAssignment>(
            "Course title", "Course work title",
            GURL("https://classroom.google.com/test-link"), absl::nullopt,
            GlanceablesClassroomAggregatedSubmissionsState(0, 0, 0)));
        std::move(cb).Run(std::move(assignments));
      });
  ASSERT_TRUE(GetComboBoxView());
  GetComboBoxView()->MenuSelectionAt(3);

  // Trigger layout for `GetListContainerView()` after receiving new items.
  widget_->LayoutRootViewIfNecessary();

  ASSERT_TRUE(GetListContainerView());
  ASSERT_EQ(GetListContainerView()->children().size(), 1u);

  EXPECT_CALL(classroom_client_,
              OpenUrl(GURL("https://classroom.google.com/test-link")));
  LeftClickOn(GetListContainerView()->children().at(0));
}

}  // namespace ash
