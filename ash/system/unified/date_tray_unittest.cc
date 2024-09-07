// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/date_tray.h"

#include <memory>
#include <vector>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/test/glanceables_tasks_test_util.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shell.h"
#include "ash/style/combobox.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/time/calendar_view.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

namespace ash {

namespace {

std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
CreateAssignmentsForStudents(int count) {
  std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments;
  for (int i = 0; i < count; ++i) {
    assignments.push_back(std::make_unique<GlanceablesClassroomAssignment>(
        base::StringPrintf("Course %d", i),
        base::StringPrintf("Course work %d", i), GURL(), std::nullopt,
        base::Time(), std::nullopt));
  }
  return assignments;
}

void WaitForTimeBetweenButtonOnClicks() {
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), views::kMinimumTimeBetweenButtonClicks);
  loop.Run();
}

class TestGlanceablesClassroomClient : public GlanceablesClassroomClient {
 public:
  TestGlanceablesClassroomClient() {
    EXPECT_TRUE(
        features::IsGlanceablesTimeManagementClassroomStudentViewEnabled());
  }

  // GlanceablesClassroomClient:
  bool IsDisabledByAdmin() const override { return false; }
  void IsStudentRoleActive(
      GlanceablesClassroomClient::IsRoleEnabledCallback cb) override {
    pending_is_student_role_enabled_callbacks_.push_back(std::move(cb));
  }
  void GetCompletedStudentAssignments(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    pending_student_assignments_callbacks_.push_back(std::move(cb));
  }
  void GetStudentAssignmentsWithApproachingDueDate(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    pending_student_assignments_callbacks_.push_back(std::move(cb));
  }
  void GetStudentAssignmentsWithMissedDueDate(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    pending_student_assignments_callbacks_.push_back(std::move(cb));
  }
  void GetStudentAssignmentsWithoutDueDate(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    pending_student_assignments_callbacks_.push_back(std::move(cb));
  }
  void OnGlanceablesBubbleClosed() override { ++bubble_closed_count_; }

  // Returns `bubble_closed_count_`, while also resetting the counter.
  int GetAndResetBubbleClosedCount() {
    int result = bubble_closed_count_;
    bubble_closed_count_ = 0;
    return result;
  }

  void RespondToPendingIsStudentRoleEnabledCallbacks(bool is_active) {
    for (auto& cb : pending_is_student_role_enabled_callbacks_) {
      std::move(cb).Run(is_active);
    }
    pending_is_student_role_enabled_callbacks_.clear();
  }

  bool RespondToNextPendingStudentAssignmentsCallback(
      std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
          assignments) {
    if (pending_student_assignments_callbacks_.empty()) {
      return false;
    }

    auto callback = std::move(pending_student_assignments_callbacks_.front());
    pending_student_assignments_callbacks_.pop_front();
    std::move(callback).Run(/*success=*/true, std::move(assignments));
    return true;
  }

 private:
  std::vector<GlanceablesClassroomClient::IsRoleEnabledCallback>
      pending_is_student_role_enabled_callbacks_;
  std::list<GlanceablesClassroomClient::GetAssignmentsCallback>
      pending_student_assignments_callbacks_;

  // Number of times `OnGlanceablesBubbleClosed()` has been called.
  int bubble_closed_count_ = 0;
};

}  // namespace

class DateTrayTest
    : public AshTestBase,
      public wm::ActivationChangeObserver,
      public testing::WithParamInterface</*glanceables_enabled=*/bool> {
 public:
  DateTrayTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kGlanceablesTimeManagementClassroomStudentView,
          IsGlanceablesClassroomEnabled()},
         {features::kGlanceablesTimeManagementTasksView, false}});
  }

  void SetUp() override {
    // Set time override.
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time date;
          bool result = base::Time::FromString("24 Aug 2021 10:00 GMT", &date);
          DCHECK(result);
          return date;
        },
        /*time_ticks_override=*/nullptr,
        /*thread_ticks_override=*/nullptr);

    AshTestBase::SetUp();

    SimulateUserLogin(account_id_);

    date_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()->date_tray();
    date_tray_->SetVisiblePreferred(true);
    date_tray_->unified_system_tray_->SetVisiblePreferred(true);

    if (IsGlanceablesClassroomEnabled()) {
      glanceables_classroom_client_ =
          std::make_unique<TestGlanceablesClassroomClient>();
      fake_glanceables_tasks_client_ =
          glanceables_tasks_test_util::InitializeFakeTasksClient(
              base::Time::Now());
      Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
          account_id_,
          GlanceablesController::ClientsRegistration{
              .classroom_client = glanceables_classroom_client_.get(),
              .tasks_client = fake_glanceables_tasks_client_.get()});
    }
  }

  void TearDown() override {
    if (IsGlanceablesClassroomEnabled()) {
      RemoveGlanceablesClients();
    }

    date_tray_ = nullptr;
    if (observering_activation_changes_) {
      Shell::Get()->activation_client()->RemoveObserver(this);
    }
    AshTestBase::TearDown();
  }

  bool IsGlanceablesClassroomEnabled() { return GetParam(); }

  DateTray* GetDateTray() { return date_tray_; }

  UnifiedSystemTray* GetUnifiedSystemTray() {
    return date_tray_->unified_system_tray_;
  }

  GlanceableTrayBubble* GetGlanceableTrayBubble() const {
    return date_tray_->bubble_.get();
  }

  bool IsBubbleShown() {
    if (IsGlanceablesClassroomEnabled()) {
      return !!GetGlanceableTrayBubble();
    }
    return GetUnifiedSystemTray()->IsBubbleShown();
  }

  bool AreContentsViewShown() {
    if (IsGlanceablesClassroomEnabled()) {
      return !!GetGlanceableTrayBubble();
    }
    return GetUnifiedSystemTray()->IsShowingCalendarView();
  }

  views::View* GetBubbleView() {
    if (IsGlanceablesClassroomEnabled()) {
      return GetGlanceableTrayBubble()->GetBubbleView();
    }
    return GetUnifiedSystemTray()->bubble()->GetBubbleView();
  }

  void LeftClickOnOpenBubble() { LeftClickOn(GetBubbleView()); }

  std::u16string GetTimeViewText() {
    return date_tray_->time_view_->time_view()
        ->GetHorizontalDateLabelForTesting()
        ->GetText();
  }

  void ImmediatelyCloseBubbleOnActivation() {
    Shell::Get()->activation_client()->AddObserver(this);
    observering_activation_changes_ = true;
  }

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    if (IsGlanceablesClassroomEnabled()) {
      GetDateTray()->HideGlanceableBubble();
    }
    GetUnifiedSystemTray()->CloseBubble();
  }

  TestGlanceablesClassroomClient* glanceables_classroom_client() {
    return glanceables_classroom_client_.get();
  }

  api::FakeTasksClient* fake_glanceables_tasks_client() {
    return fake_glanceables_tasks_client_.get();
  }

  void RemoveGlanceablesClients() {
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesController::ClientsRegistration{
                         .classroom_client = nullptr, .tasks_client = nullptr});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("test_user@gmail.com", "123456");
  std::unique_ptr<TestGlanceablesClassroomClient> glanceables_classroom_client_;
  std::unique_ptr<api::FakeTasksClient> fake_glanceables_tasks_client_;
  bool observering_activation_changes_ = false;

  // Owned by status area widget.
  raw_ptr<DateTray> date_tray_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(GlanceablesClassroom, DateTrayTest, testing::Bool());

using GlanceablesDateTrayTest = DateTrayTest;
INSTANTIATE_TEST_SUITE_P(GlanceablesClassroom,
                         GlanceablesDateTrayTest,
                         testing::Values(true));

// Tests that toggling the `CalendarView` via the date tray accelerator does not
// result in a crash when the unified system tray bubble is set to immediately
// close upon activation. See crbug/1419499 for details.
TEST_P(DateTrayTest, AcceleratorOpenAndImmediateCloseDoesNotCrash) {
  ImmediatelyCloseBubbleOnActivation();
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  if (IsGlanceablesClassroomEnabled()) {
    // The glanceables bubble cannot be closed during activation, so expect it
    // to still be shown.
    EXPECT_TRUE(IsBubbleShown());
  } else {
    EXPECT_FALSE(IsBubbleShown());
  }
}

// Test that search + c shows and hides a glanceables or calendar bubble.
TEST_P(DateTrayTest, AcceleratorTogglesBubble) {
  EXPECT_FALSE(IsBubbleShown());

  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(IsBubbleShown());

  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_FALSE(IsBubbleShown());
}

// Test the initial state.
TEST_P(DateTrayTest, InitialState) {
  // Show the mock time now Month and day.
  EXPECT_EQ(u"Aug 24", GetTimeViewText());

  // Initial state: not showing the calendar bubble.
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(AreContentsViewShown());
  if (IsGlanceablesClassroomEnabled()) {
    EXPECT_EQ(0,
              fake_glanceables_tasks_client()->GetAndResetBubbleClosedCount());
    EXPECT_EQ(0,
              glanceables_classroom_client()->GetAndResetBubbleClosedCount());
  }
}

// Tests clicking/tapping the DateTray shows/closes the calendar bubble.
TEST_P(DateTrayTest, ShowCalendarBubble) {
  base::HistogramTester histogram_tester;
  // Clicking on the `DateTray` -> show the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  histogram_tester.ExpectTotalCount("Ash.Calendar.ShowSource.TimeView",
                                    IsGlanceablesClassroomEnabled() ? 0 : 1);

  // Clicking on the `DateTray` again -> close the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
  if (IsGlanceablesClassroomEnabled()) {
    EXPECT_EQ(1,
              fake_glanceables_tasks_client()->GetAndResetBubbleClosedCount());
    EXPECT_EQ(1,
              glanceables_classroom_client()->GetAndResetBubbleClosedCount());
  }

  // Tapping on the `DateTray` again -> open the calendar bubble.
  GestureTapOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  histogram_tester.ExpectTotalCount("Ash.Calendar.ShowSource.TimeView",
                                    IsGlanceablesClassroomEnabled() ? 0 : 2);

  // Tapping on the `DateTray` again -> close the calendar bubble.
  GestureTapOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
  if (IsGlanceablesClassroomEnabled()) {
    EXPECT_EQ(1,
              fake_glanceables_tasks_client()->GetAndResetBubbleClosedCount());
    EXPECT_EQ(1,
              glanceables_classroom_client()->GetAndResetBubbleClosedCount());
  }
}

TEST_P(DateTrayTest, DontActivateBubbleIfShownByTap) {
  // Clicking on the `DateTray` -> show the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_TRUE(GetDateTray()->is_active());

  views::Widget* const bubble_widget = GetBubbleView()->GetWidget();
  // The bubble should not be activated if the calendar/glanceables bubble gets
  // shown by tapping the date tray.
  EXPECT_FALSE(bubble_widget->IsActive());

  if (IsGlanceablesClassroomEnabled()) {
    glanceables_classroom_client()
        ->RespondToPendingIsStudentRoleEnabledCallbacks(
            /*is_active=*/true);
    ASSERT_TRUE(glanceables_classroom_client()
                    ->RespondToNextPendingStudentAssignmentsCallback(
                        CreateAssignmentsForStudents(/*count=*/1)));
  }
  EXPECT_FALSE(bubble_widget->IsActive());

  // The user should be able to activate the bubble
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);

  EXPECT_TRUE(bubble_widget->IsActive());

  views::View* const focused_view =
      bubble_widget->GetFocusManager()->GetFocusedView();
  ASSERT_TRUE(focused_view);
  // Verify that the calendar view gets the focus.
  if (IsGlanceablesClassroomEnabled()) {
    EXPECT_TRUE(
        GetGlanceableTrayBubble()->GetCalendarView()->Contains(focused_view));
  }
  EXPECT_STREQ("CalendarDateCellView", focused_view->GetClassName());
}

TEST_P(DateTrayTest, ActivateBubbleIfShownByKeyboard) {
  GetPrimaryShelf()->shelf_focus_cycler()->FocusStatusArea(
      false /* last_element */);
  GetDateTray()->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_TRUE(GetDateTray()->is_active());

  views::Widget* const bubble_widget = GetBubbleView()->GetWidget();
  // Verify that the bubble gets activated if opened via keyboard.
  EXPECT_TRUE(bubble_widget->IsActive());

  if (IsGlanceablesClassroomEnabled()) {
    glanceables_classroom_client()
        ->RespondToPendingIsStudentRoleEnabledCallbacks(
            /*is_active=*/true);
    ASSERT_TRUE(glanceables_classroom_client()
                    ->RespondToNextPendingStudentAssignmentsCallback(
                        CreateAssignmentsForStudents(/*count=*/1)));
  }
  EXPECT_TRUE(bubble_widget->IsActive());

  views::View* const focused_view =
      bubble_widget->GetFocusManager()->GetFocusedView();
  ASSERT_TRUE(focused_view);
  // Verify that the calendar view gets the focus.
  if (IsGlanceablesClassroomEnabled()) {
    EXPECT_TRUE(
        GetGlanceableTrayBubble()->GetCalendarView()->Contains(focused_view));
  }
  EXPECT_STREQ("CalendarDateCellView", focused_view->GetClassName());
}

// Tests the behavior when clicking on different areas.
TEST_P(DateTrayTest, ClickingArea) {
  // Clicking on the `DateTray` -> show the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  // Clicking on the bubble area -> not close the calendar bubble.
  LeftClickOnOpenBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  // Clicking on the `UnifiedSystemTray` -> switch to QS bubble.
  LeftClickOn(GetUnifiedSystemTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetUnifiedSystemTray()->IsBubbleShown());
  EXPECT_TRUE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
  if (IsGlanceablesClassroomEnabled()) {
    EXPECT_EQ(1,
              fake_glanceables_tasks_client()->GetAndResetBubbleClosedCount());
    EXPECT_EQ(1,
              glanceables_classroom_client()->GetAndResetBubbleClosedCount());
  }

  // Clicking on the `DateTray` -> switch to the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  // Clicking on `DateTray` closes the bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->IsBubbleShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
  if (IsGlanceablesClassroomEnabled()) {
    EXPECT_EQ(1,
              fake_glanceables_tasks_client()->GetAndResetBubbleClosedCount());
    EXPECT_EQ(1,
              glanceables_classroom_client()->GetAndResetBubbleClosedCount());
  }
}

TEST_P(DateTrayTest, EscapeKeyForClose) {
  base::HistogramTester histogram_tester;
  // Clicking on the `DateTray` -> show the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  histogram_tester.ExpectTotalCount("Ash.Calendar.ShowSource.TimeView",
                                    IsGlanceablesClassroomEnabled() ? 0 : 1);

  // Hitting escape key -> close and deactivate the calendar bubble.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
  if (IsGlanceablesClassroomEnabled()) {
    EXPECT_EQ(1,
              fake_glanceables_tasks_client()->GetAndResetBubbleClosedCount());
    EXPECT_EQ(1,
              glanceables_classroom_client()->GetAndResetBubbleClosedCount());
  }
}

// Tests that calling `DateTray::CloseBubble()` actually closes the bubble.
TEST_P(DateTrayTest, CloseBubble) {
  ASSERT_FALSE(IsBubbleShown());
  // Clicking on the `DateTray` -> show the calendar bubble.
  LeftClickOn(GetDateTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_TRUE(GetDateTray()->is_active());

  // Calling `DateTray::CloseBubble()` should close the bubble.
  GetDateTray()->CloseBubble();
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
  base::RunLoop().RunUntilIdle();
  if (IsGlanceablesClassroomEnabled()) {
    EXPECT_EQ(1,
              fake_glanceables_tasks_client()->GetAndResetBubbleClosedCount());
    EXPECT_EQ(1,
              glanceables_classroom_client()->GetAndResetBubbleClosedCount());
  }

  // Calling `DateTray::CloseBubble()` on an already-closed bubble should do
  // nothing.
  GetDateTray()->CloseBubble();
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
  base::RunLoop().RunUntilIdle();
  if (IsGlanceablesClassroomEnabled()) {
    EXPECT_EQ(0,
              fake_glanceables_tasks_client()->GetAndResetBubbleClosedCount());
    EXPECT_EQ(0,
              glanceables_classroom_client()->GetAndResetBubbleClosedCount());
  }
}

TEST_P(DateTrayTest, DoesNotRenderClassroomBubblesForInactiveRoles) {
  LeftClickOn(GetDateTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  if (!IsGlanceablesClassroomEnabled()) {
    EXPECT_FALSE(GetGlanceableTrayBubble());
    return;
  }

  glanceables_classroom_client()->RespondToPendingIsStudentRoleEnabledCallbacks(
      false);

  // Only calendar is rendered in `GlanceableTrayBubbleView`.
  EXPECT_EQ(GetGlanceableTrayBubble()->GetBubbleView()->children().size(), 1u);
}

TEST_P(DateTrayTest, RendersClassroomBubblesForActiveRoles) {
  LeftClickOn(GetDateTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  if (!IsGlanceablesClassroomEnabled()) {
    EXPECT_FALSE(GetGlanceableTrayBubble());
    return;
  }

  // Only calendar is rendered in `GlanceableTrayBubbleView`.
  EXPECT_EQ(GetGlanceableTrayBubble()->GetBubbleView()->children().size(), 1u);

  // Classroom student bubble is added in TimeManagementContainer.
  glanceables_classroom_client()->RespondToPendingIsStudentRoleEnabledCallbacks(
      true);
  // Both calendar and the `TimeManagementContainer` should be rendered in
  // `GlanceableTrayBubbleView`.
  EXPECT_EQ(GetGlanceableTrayBubble()->GetBubbleView()->children().size(), 2u);
  EXPECT_STREQ("TimeManagementContainer", GetGlanceableTrayBubble()
                                              ->GetBubbleView()
                                              ->children()
                                              .at(0)
                                              ->GetClassName());
}

TEST_P(GlanceablesDateTrayTest,
       TrayBubbleUpdatesBoundsOnDisplayConfigurationUpdate) {
  LeftClickOn(GetDateTray());
  ASSERT_TRUE(IsBubbleShown());
  ASSERT_TRUE(GetGlanceableTrayBubble());

  glanceables_classroom_client()->RespondToPendingIsStudentRoleEnabledCallbacks(
      /*is_active=*/true);

  UpdateDisplay("1240x700");
  const auto old_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const auto old_view_bounds =
      GetGlanceableTrayBubble()->GetBubbleView()->GetBoundsInScreen();

  UpdateDisplay("800x480");
  const auto new_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const auto new_view_bounds =
      GetGlanceableTrayBubble()->GetBubbleView()->GetBoundsInScreen();

  // Constant `kWideTrayMenuWidth`.
  EXPECT_EQ(old_view_bounds.width(), new_view_bounds.width());

  // Margins between the top, right and bottom edges of the view and the
  // corresponding work area edges are the same, meaning that the view is
  // correctly repositioned (including changing its height) after changing
  // display configuration / zoom level.
  EXPECT_GT(old_view_bounds.height(), new_view_bounds.height());
  EXPECT_EQ(old_work_area.width() - old_view_bounds.right(),
            new_work_area.width() - new_view_bounds.right());
  EXPECT_EQ(old_work_area.height() - old_view_bounds.bottom(),
            new_work_area.height() - new_view_bounds.bottom());
}

TEST_P(GlanceablesDateTrayTest, AssignmentListFetchedWhileBubbleClosing) {
  LeftClickOn(GetDateTray());
  ASSERT_TRUE(IsBubbleShown());
  ASSERT_TRUE(GetGlanceableTrayBubble());

  glanceables_classroom_client()->RespondToPendingIsStudentRoleEnabledCallbacks(
      /*is_active=*/true);

  GetGlanceableTrayBubble()->GetBubbleWidget()->Close();
  ASSERT_TRUE(glanceables_classroom_client()
                  ->RespondToNextPendingStudentAssignmentsCallback(
                      CreateAssignmentsForStudents(/*count=*/3)));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsBubbleShown());
}

TEST_P(GlanceablesDateTrayTest, ClickOutsideComboboxMenu) {
  EXPECT_FALSE(GetGlanceableTrayBubble());

  // Click the date tray to show the glanceable bubbles.
  LeftClickOn(GetDateTray());
  EXPECT_TRUE(GetGlanceableTrayBubble());

  glanceables_classroom_client()->RespondToPendingIsStudentRoleEnabledCallbacks(
      /*is_active=*/true);
  EXPECT_TRUE(GetGlanceableTrayBubble()->GetClassroomStudentView());

  // Click on the combo box to show the assignment types list.
  const auto* combobox = views::AsViewClass<Combobox>(
      GetGlanceableTrayBubble()->GetClassroomStudentView()->GetViewByID(
          base::to_underlying(
              GlanceablesViewId::kTimeManagementBubbleComboBox)));
  LeftClickOn(combobox);
  EXPECT_TRUE(combobox->IsMenuRunning());
  EXPECT_TRUE(GetGlanceableTrayBubble());

  // Click at the top of the classroom view and make sure the menu gets
  // closed.
  GetEventGenerator()->MoveMouseTo(GetGlanceableTrayBubble()
                                       ->GetClassroomStudentView()
                                       ->GetBoundsInScreen()
                                       .top_center());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(combobox->IsMenuRunning());
  EXPECT_TRUE(GetGlanceableTrayBubble());

  WaitForTimeBetweenButtonOnClicks();

  // Click on the combo box to show the assignment types list again.
  LeftClickOn(combobox);
  EXPECT_TRUE(combobox->IsMenuRunning());
  EXPECT_TRUE(GetGlanceableTrayBubble());

  const gfx::Point left_side_of_screen =
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().left_center();

  // Click outside of the glanceables bubbles, on the left side of the screen.
  GetEventGenerator()->MoveMouseTo(left_side_of_screen);
  GetEventGenerator()->ClickLeftButton();

  // Check that the combobox menu is closed, but that the glaneable bubbles
  // are still open.
  EXPECT_FALSE(combobox->IsMenuRunning());
  EXPECT_TRUE(GetGlanceableTrayBubble());

  // Click outside the glanceabes bubble again to ensure closing occurs.
  GetEventGenerator()->MoveMouseTo(left_side_of_screen);
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(GetGlanceableTrayBubble());
}

}  // namespace ash
