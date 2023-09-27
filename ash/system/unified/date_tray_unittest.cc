// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/date_tray.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/fake_glanceables_tasks_client.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/style/combobox.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/time/calendar_view.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/unified/classroom_bubble_student_view.h"
#include "ash/system/unified/classroom_bubble_teacher_view.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/system/unified/tasks_bubble_view.h"
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
#include "ui/views/view_utils.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

namespace ash {

namespace {

std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
CreateAssignmentsForTeachers(int count) {
  std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments;
  for (int i = 0; i < count; ++i) {
    assignments.push_back(std::make_unique<GlanceablesClassroomAssignment>(
        base::StringPrintf("Course %d", i),
        base::StringPrintf("Course work %d", i), GURL(), absl::nullopt,
        base::Time(), GlanceablesClassroomAggregatedSubmissionsState(2, 2, 0)));
  }
  return assignments;
}

std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
CreateAssignmentsForStudents(int count) {
  std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments;
  for (int i = 0; i < count; ++i) {
    assignments.push_back(std::make_unique<GlanceablesClassroomAssignment>(
        base::StringPrintf("Course %d", i),
        base::StringPrintf("Course work %d", i), GURL(), absl::nullopt,
        base::Time(), absl::nullopt));
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
    EXPECT_TRUE(features::AreGlanceablesV2Enabled());
  }

  // GlanceablesClassroomClient:
  void IsStudentRoleActive(
      GlanceablesClassroomClient::IsRoleEnabledCallback cb) override {
    pending_is_student_role_enabled_callbacks_.push_back(std::move(cb));
  }
  void IsTeacherRoleActive(
      GlanceablesClassroomClient::IsRoleEnabledCallback cb) override {
    pending_is_teacher_role_enabled_callbacks_.push_back(std::move(cb));
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
  void GetTeacherAssignmentsWithApproachingDueDate(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    pending_teacher_assignments_callbacks_.push_back(std::move(cb));
  }
  void GetTeacherAssignmentsRecentlyDue(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    pending_teacher_assignments_callbacks_.push_back(std::move(cb));
  }
  void GetTeacherAssignmentsWithoutDueDate(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    pending_teacher_assignments_callbacks_.push_back(std::move(cb));
  }
  void GetGradedTeacherAssignments(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    pending_teacher_assignments_callbacks_.push_back(std::move(cb));
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

  void RespondToPendingIsTeacherRoleEnabledCallbacks(bool is_active) {
    for (auto& cb : pending_is_teacher_role_enabled_callbacks_) {
      std::move(cb).Run(is_active);
    }
    pending_is_teacher_role_enabled_callbacks_.clear();
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

  bool RespondToNextPendingTeacherAssignmentsCallback(
      std::vector<std::unique_ptr<GlanceablesClassroomAssignment>>
          assignments) {
    if (pending_teacher_assignments_callbacks_.empty()) {
      return false;
    }

    auto callback = std::move(pending_teacher_assignments_callbacks_.front());
    pending_teacher_assignments_callbacks_.pop_front();
    std::move(callback).Run(/*success=*/true, std::move(assignments));
    return true;
  }

 private:
  std::vector<GlanceablesClassroomClient::IsRoleEnabledCallback>
      pending_is_student_role_enabled_callbacks_;
  std::vector<GlanceablesClassroomClient::IsRoleEnabledCallback>
      pending_is_teacher_role_enabled_callbacks_;

  std::list<GlanceablesClassroomClient::GetAssignmentsCallback>
      pending_student_assignments_callbacks_;
  std::list<GlanceablesClassroomClient::GetAssignmentsCallback>
      pending_teacher_assignments_callbacks_;

  // Number of times `OnGlanceablesBubbleClosed()` has been called.
  int bubble_closed_count_ = 0;
};

}  // namespace

class DateTrayTest
    : public AshTestBase,
      public wm::ActivationChangeObserver,
      public testing::WithParamInterface</*glanceables_v2_enabled=*/bool> {
 public:
  DateTrayTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kGlanceablesV2, AreGlanceablesV2Enabled()},
         {features::kGlanceablesV2ClassroomTeacherView,
          AreGlanceablesV2Enabled()}});
  }

  DateTrayTest(const DateTrayTest&) = delete;
  DateTrayTest& operator=(const DateTrayTest&) = delete;
  ~DateTrayTest() override = default;

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

    widget_ = CreateFramelessTestWidget();
    widget_->SetContentsView(std::make_unique<views::View>());
    widget_->SetFullscreen(true);
    date_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()->date_tray();
    unified_system_tray_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget()
                               ->unified_system_tray();
    widget_->GetContentsView()->AddChildView(date_tray_.get());
    widget_->GetContentsView()->AddChildView(unified_system_tray_.get());
    date_tray_->SetVisiblePreferred(true);
    date_tray_->unified_system_tray_->SetVisiblePreferred(true);

    if (AreGlanceablesV2Enabled()) {
      glanceables_classroom_client_ =
          std::make_unique<TestGlanceablesClassroomClient>();
      fake_glanceables_tasks_client_ =
          std::make_unique<FakeGlanceablesTasksClient>(base::Time::Now());
      Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
          account_id_,
          GlanceablesController::ClientsRegistration{
              .classroom_client = glanceables_classroom_client_.get(),
              .tasks_client = fake_glanceables_tasks_client_.get()});
    }
  }

  void TearDown() override {
    if (AreGlanceablesV2Enabled()) {
      RemoveGlanceablesClients();
    }

    widget_.reset();
    date_tray_ = nullptr;
    if (observering_activation_changes_) {
      Shell::Get()->activation_client()->RemoveObserver(this);
    }
    AshTestBase::TearDown();
  }

  bool AreGlanceablesV2Enabled() { return GetParam(); }

  DateTray* GetDateTray() { return date_tray_; }

  UnifiedSystemTray* GetUnifiedSystemTray() {
    return date_tray_->unified_system_tray_;
  }

  GlanceableTrayBubble* GetGlanceableTrayBubble() const {
    return date_tray_->bubble_.get();
  }

  bool IsBubbleShown() {
    if (AreGlanceablesV2Enabled()) {
      return !!GetGlanceableTrayBubble();
    }
    return GetUnifiedSystemTray()->IsBubbleShown();
  }

  bool AreContentsViewShown() {
    if (AreGlanceablesV2Enabled()) {
      return !!GetGlanceableTrayBubble();
    }
    return GetUnifiedSystemTray()->IsShowingCalendarView();
  }

  views::View* GetBubbleView() {
    if (AreGlanceablesV2Enabled()) {
      return GetGlanceableTrayBubble()->GetBubbleView();
    }
    return GetUnifiedSystemTray()->bubble()->GetBubbleView();
  }

  void LeftClickOnOpenBubble() { LeftClickOn(GetBubbleView()); }

  std::u16string GetTimeViewText() {
    return date_tray_->time_view_->time_view()
        ->horizontal_label_date_for_test()
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
    if (AreGlanceablesV2Enabled()) {
      GetDateTray()->HideGlanceableBubble();
    }
    GetUnifiedSystemTray()->CloseBubble();
  }

  TestGlanceablesClassroomClient* glanceables_classroom_client() {
    return glanceables_classroom_client_.get();
  }

  FakeGlanceablesTasksClient* fake_glanceables_tasks_client() {
    return fake_glanceables_tasks_client_.get();
  }

  void RemoveGlanceablesClients() {
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesController::ClientsRegistration{
                         .classroom_client = nullptr, .tasks_client = nullptr});
  }

  TasksBubbleView* GetTasksView() const {
    return GetGlanceableTrayBubble()->GetTasksView();
  }

  Combobox* GetTasksComboBoxView() const {
    return views::AsViewClass<Combobox>(
        GetGlanceableTrayBubble()->GetTasksView()->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTasksBubbleComboBox)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<views::Widget> widget_;
  AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("test_user@gmail.com", "123456");
  std::unique_ptr<TestGlanceablesClassroomClient> glanceables_classroom_client_;
  std::unique_ptr<FakeGlanceablesTasksClient> fake_glanceables_tasks_client_;
  bool observering_activation_changes_ = false;

  // Owned by `widget_`.
  raw_ptr<DateTray, DanglingUntriaged | ExperimentalAsh> date_tray_ = nullptr;

  raw_ptr<UnifiedSystemTray, DanglingUntriaged | ExperimentalAsh>
      unified_system_tray_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(GlanceablesV2, DateTrayTest, testing::Bool());

using GlanceablesDateTrayTest = DateTrayTest;
INSTANTIATE_TEST_SUITE_P(GlanceablesV2,
                         GlanceablesDateTrayTest,
                         testing::Values(true));

// Tests that toggling the `CalendarView` via the date tray accelerator does not
// result in a crash when the unified system tray bubble is set to immediately
// close upon activation. See crbug/1419499 for details.
TEST_P(DateTrayTest, AcceleratorOpenAndImmediateCloseDoesNotCrash) {
  ImmediatelyCloseBubbleOnActivation();
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  if (AreGlanceablesV2Enabled()) {
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
  if (AreGlanceablesV2Enabled()) {
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
                                    AreGlanceablesV2Enabled() ? 0 : 1);

  // Clicking on the `DateTray` again -> close the calendar bubble.
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
  if (AreGlanceablesV2Enabled()) {
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
                                    AreGlanceablesV2Enabled() ? 0 : 2);

  // Tapping on the `DateTray` again -> close the calendar bubble.
  GestureTapOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
  if (AreGlanceablesV2Enabled()) {
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

  if (AreGlanceablesV2Enabled()) {
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
  if (AreGlanceablesV2Enabled()) {
    EXPECT_TRUE(
        GetGlanceableTrayBubble()->GetCalendarView()->Contains(focused_view));
  }
  EXPECT_STREQ("CalendarDateCellView", focused_view->GetClassName());
}

TEST_P(DateTrayTest, ActivateBubbleIfShownByKeyboard) {
  GetDateTray()->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());
  EXPECT_TRUE(GetDateTray()->is_active());

  views::Widget* const bubble_widget = GetBubbleView()->GetWidget();
  // Verify that the bubble gets activated if opened via keyboard.
  EXPECT_TRUE(bubble_widget->IsActive());

  if (AreGlanceablesV2Enabled()) {
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
  if (AreGlanceablesV2Enabled()) {
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
  if (AreGlanceablesV2Enabled()) {
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
  if (AreGlanceablesV2Enabled()) {
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
                                    AreGlanceablesV2Enabled() ? 0 : 1);

  // Hitting escape key -> close and deactivate the calendar bubble.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(AreContentsViewShown());
  EXPECT_FALSE(GetUnifiedSystemTray()->is_active());
  EXPECT_FALSE(GetDateTray()->is_active());
  if (AreGlanceablesV2Enabled()) {
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
  if (AreGlanceablesV2Enabled()) {
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
  if (AreGlanceablesV2Enabled()) {
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

  if (!AreGlanceablesV2Enabled()) {
    EXPECT_FALSE(GetGlanceableTrayBubble());
    return;
  }

  glanceables_classroom_client()->RespondToPendingIsStudentRoleEnabledCallbacks(
      false);
  glanceables_classroom_client()->RespondToPendingIsTeacherRoleEnabledCallbacks(
      false);

  // Only static bubbles are rendered in `scroll_view` (tasks and calendar).
  const auto* const scroll_view = views::AsViewClass<views::ScrollView>(
      GetGlanceableTrayBubble()->GetBubbleView()->children().at(0));
  ASSERT_TRUE(scroll_view);
  EXPECT_EQ(scroll_view->contents()->children().size(), 2u);
}

TEST_P(DateTrayTest, RendersClassroomBubblesForActiveRoles) {
  LeftClickOn(GetDateTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  if (!AreGlanceablesV2Enabled()) {
    EXPECT_FALSE(GetGlanceableTrayBubble());
    return;
  }

  // Only static bubbles are rendered in `scroll_view` (tasks and calendar).
  const auto* const scroll_view = views::AsViewClass<views::ScrollView>(
      GetGlanceableTrayBubble()->GetBubbleView()->children().at(0));
  ASSERT_TRUE(scroll_view);
  EXPECT_EQ(scroll_view->contents()->children().size(), 2u);

  // Classroom student bubble is added.
  glanceables_classroom_client()->RespondToPendingIsStudentRoleEnabledCallbacks(
      true);
  EXPECT_EQ(scroll_view->contents()->children().size(), 3u);

  // Classroom teacher bubble is added.
  glanceables_classroom_client()->RespondToPendingIsTeacherRoleEnabledCallbacks(
      true);
  EXPECT_EQ(scroll_view->contents()->children().size(), 4u);
}

TEST_P(DateTrayTest, EmptyClientsFallbackToLegacyDateBubble) {
  LeftClickOn(GetDateTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  if (!AreGlanceablesV2Enabled()) {
    EXPECT_FALSE(GetGlanceableTrayBubble());
    return;
  }

  // Remove glanceables clients and click on the date tray to close the bubble
  // again.
  RemoveGlanceablesClients();
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(AreContentsViewShown());
  EXPECT_FALSE(GetGlanceableTrayBubble());

  // Click on the date tray again, now, the unified system tray calendar view
  // should show instead of the glanceables tray bubble.
  LeftClickOn(GetDateTray());
  EXPECT_TRUE(GetUnifiedSystemTray()->IsBubbleShown());
  EXPECT_TRUE(GetUnifiedSystemTray()->IsShowingCalendarView());
  EXPECT_FALSE(GetGlanceableTrayBubble());
}

TEST_P(GlanceablesDateTrayTest, TrayBubbleGrowsWithTeacherGlanceableViews) {
  UpdateDisplay("512x1536");

  LeftClickOn(GetDateTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  ASSERT_TRUE(GetGlanceableTrayBubble());

  auto* const scroll_view = views::AsViewClass<views::ScrollView>(
      GetGlanceableTrayBubble()->GetBubbleView()->children().at(0));
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();

  glanceables_classroom_client()->RespondToPendingIsTeacherRoleEnabledCallbacks(
      true);
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();

  auto* teacher_view = GetGlanceableTrayBubble()->GetClassroomTeacherView();
  ASSERT_TRUE(teacher_view);

  auto* calendar_view = GetGlanceableTrayBubble()->GetCalendarView();
  ASSERT_TRUE(calendar_view);

  auto* tasks_view = GetTasksView();
  ASSERT_TRUE(tasks_view);

  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      teacher_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      tasks_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  ASSERT_TRUE(glanceables_classroom_client()
                  ->RespondToNextPendingTeacherAssignmentsCallback(
                      CreateAssignmentsForTeachers(/*count=*/3)));

  // Verify that the glanceable bubble expands so both teacher view and calendar
  // view remain in the scroll view viewport.
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      teacher_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      tasks_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  const int calendar_view_bottom = calendar_view->GetBoundsInScreen().bottom();
  const int tasks_view_top = tasks_view->GetBoundsInScreen().y();

  Combobox* assignment_selector =
      views::AsViewClass<Combobox>(teacher_view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kClassroomBubbleComboBox)));
  ASSERT_TRUE(assignment_selector);

  assignment_selector->SelectMenuItemForTest(2);
  ASSERT_TRUE(glanceables_classroom_client()
                  ->RespondToNextPendingTeacherAssignmentsCallback(
                      CreateAssignmentsForTeachers(/*count=*/1)));

  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      teacher_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      tasks_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  EXPECT_EQ(calendar_view_bottom, calendar_view->GetBoundsInScreen().bottom());
  EXPECT_LT(tasks_view_top, tasks_view->GetBoundsInScreen().y());
}

TEST_P(GlanceablesDateTrayTest, TrayBubbleGrowsWithStudentGlanceableView) {
  UpdateDisplay("512x1536");

  LeftClickOn(GetDateTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  ASSERT_TRUE(GetGlanceableTrayBubble());

  auto* const scroll_view = views::AsViewClass<views::ScrollView>(
      GetGlanceableTrayBubble()->GetBubbleView()->children().at(0));
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();

  glanceables_classroom_client()->RespondToPendingIsStudentRoleEnabledCallbacks(
      true);
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();

  auto* student_view = GetGlanceableTrayBubble()->GetClassroomStudentView();
  ASSERT_TRUE(student_view);

  auto* calendar_view = GetGlanceableTrayBubble()->GetCalendarView();
  ASSERT_TRUE(calendar_view);

  auto* tasks_view = GetTasksView();
  ASSERT_TRUE(tasks_view);

  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      student_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      tasks_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  ASSERT_TRUE(glanceables_classroom_client()
                  ->RespondToNextPendingStudentAssignmentsCallback(
                      CreateAssignmentsForStudents(/*count=*/3)));

  // Verify that the glanceable bubble expands so both student view and calendar
  // view remain in the scroll view viewport.
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      student_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      tasks_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  Combobox* assignment_selector =
      views::AsViewClass<Combobox>(student_view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kClassroomBubbleComboBox)));
  ASSERT_TRUE(assignment_selector);

  const int calendar_view_bottom = calendar_view->GetBoundsInScreen().bottom();
  const int tasks_view_top = tasks_view->GetBoundsInScreen().y();

  assignment_selector->SelectMenuItemForTest(2);
  ASSERT_TRUE(glanceables_classroom_client()
                  ->RespondToNextPendingStudentAssignmentsCallback(
                      CreateAssignmentsForStudents(/*count=*/1)));

  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      student_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      tasks_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  EXPECT_EQ(calendar_view_bottom, calendar_view->GetBoundsInScreen().bottom());
  EXPECT_LT(tasks_view_top, tasks_view->GetBoundsInScreen().y());
}

TEST_P(GlanceablesDateTrayTest, TrayBubbleGrowsUpward) {
  UpdateDisplay("1024x512");

  LeftClickOn(GetDateTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  ASSERT_TRUE(GetGlanceableTrayBubble());

  auto* const scroll_view = views::AsViewClass<views::ScrollView>(
      GetGlanceableTrayBubble()->GetBubbleView()->children().at(0));
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();

  glanceables_classroom_client()->RespondToPendingIsTeacherRoleEnabledCallbacks(
      true);
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();

  auto* teacher_view = GetGlanceableTrayBubble()->GetClassroomTeacherView();
  ASSERT_TRUE(teacher_view);

  auto* calendar_view = GetGlanceableTrayBubble()->GetCalendarView();
  ASSERT_TRUE(calendar_view);

  // The display size cannot accommodate both teacher view and the calendar view
  // - calendar view should be visible in the scroll view's viewport.
  EXPECT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      teacher_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  ASSERT_TRUE(glanceables_classroom_client()
                  ->RespondToNextPendingTeacherAssignmentsCallback(
                      CreateAssignmentsForTeachers(/*count=*/3)));

  // The display size is not sufficient to fit both teacher glanceable and the
  // calendar view. Verify that it's scrolled so the calendar remains visible.
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      teacher_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  Combobox* assignment_selector =
      views::AsViewClass<Combobox>(teacher_view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kClassroomBubbleComboBox)));
  ASSERT_TRUE(assignment_selector);

  teacher_view->ScrollViewToVisible();

  const int calendar_view_bottom = calendar_view->GetBoundsInScreen().bottom();
  assignment_selector->SelectMenuItemForTest(2);
  ASSERT_TRUE(glanceables_classroom_client()
                  ->RespondToNextPendingTeacherAssignmentsCallback(
                      CreateAssignmentsForTeachers(/*count=*/2)));

  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      teacher_view->GetBoundsInScreen()));
  EXPECT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  EXPECT_EQ(calendar_view_bottom, calendar_view->GetBoundsInScreen().bottom());
}

TEST_P(GlanceablesDateTrayTest,
       TeacherGlanceableGrowthDoesNotMoveFocusedViewOffscreen) {
  UpdateDisplay("1024x512");

  LeftClickOn(GetDateTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  ASSERT_TRUE(GetGlanceableTrayBubble());

  auto* const scroll_view = views::AsViewClass<views::ScrollView>(
      GetGlanceableTrayBubble()->GetBubbleView()->children().at(0));
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();

  glanceables_classroom_client()->RespondToPendingIsTeacherRoleEnabledCallbacks(
      true);
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();

  auto* teacher_view = GetGlanceableTrayBubble()->GetClassroomTeacherView();
  ASSERT_TRUE(teacher_view);

  auto* calendar_view = GetGlanceableTrayBubble()->GetCalendarView();
  ASSERT_TRUE(calendar_view);

  // The display size cannot accommodate both teacher view and the calendar view
  // - calendar view should be visible in the scroll view's viewport.
  EXPECT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      teacher_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  ASSERT_TRUE(glanceables_classroom_client()
                  ->RespondToNextPendingTeacherAssignmentsCallback(
                      CreateAssignmentsForTeachers(/*count=*/1)));

  // The display size is not sufficient to fit both teacher glanceable and the
  // calendar view. Verify that it's scrolled so the calendar remains visible.
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      teacher_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  Combobox* assignment_selector =
      views::AsViewClass<Combobox>(teacher_view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kClassroomBubbleComboBox)));
  ASSERT_TRUE(assignment_selector);

  // Focus the selector, and increase the glanceable size in response to the
  // selection change - verify that the focused selector remains visible.
  assignment_selector->ScrollViewToVisible();
  assignment_selector->RequestFocus();
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      assignment_selector->GetBoundsInScreen()));

  assignment_selector->SelectMenuItemForTest(2);

  ASSERT_TRUE(glanceables_classroom_client()
                  ->RespondToNextPendingTeacherAssignmentsCallback(
                      CreateAssignmentsForTeachers(/*count=*/3)));

  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      assignment_selector->GetBoundsInScreen()));
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

  // Constant `kRevampedTrayMenuWidth`.
  EXPECT_EQ(old_view_bounds.width(), new_view_bounds.width());

  // Margins between the top, right and bottom edges of the view and the
  // corresponding work area edges are the same, meaning that the view is
  // correctly repositioned (including changing its height) after changing
  // display configuration / zoom level.
  EXPECT_EQ(old_view_bounds.y(), new_view_bounds.y());
  EXPECT_EQ(old_work_area.width() - old_view_bounds.right(),
            new_work_area.width() - new_view_bounds.right());
  EXPECT_EQ(old_work_area.height() - old_view_bounds.bottom(),
            new_work_area.height() - new_view_bounds.bottom());
}

TEST_P(GlanceablesDateTrayTest,
       BubbleClosedUsingTrayButtonWithPendingTasksRequests) {
  fake_glanceables_tasks_client()->set_paused(true);

  LeftClickOn(GetDateTray());
  ASSERT_TRUE(IsBubbleShown());
  ASSERT_TRUE(GetGlanceableTrayBubble());

  EXPECT_EQ(1u,
            fake_glanceables_tasks_client()->RunPendingGetTaskListsCallbacks());

  LeftClickOn(GetDateTray());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsBubbleShown());
}

TEST_P(GlanceablesDateTrayTest, TasksListFetchedWhileBubbleClosing) {
  fake_glanceables_tasks_client()->set_paused(true);

  LeftClickOn(GetDateTray());
  ASSERT_TRUE(IsBubbleShown());
  ASSERT_TRUE(GetGlanceableTrayBubble());

  EXPECT_EQ(1u,
            fake_glanceables_tasks_client()->RunPendingGetTaskListsCallbacks());

  GetGlanceableTrayBubble()->GetBubbleWidget()->Close();
  EXPECT_EQ(1u, fake_glanceables_tasks_client()->RunPendingGetTasksCallbacks());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsBubbleShown());
}

TEST_P(GlanceablesDateTrayTest, TaskListsFetchedWhileBubbleClosing) {
  fake_glanceables_tasks_client()->set_paused(true);

  LeftClickOn(GetDateTray());
  ASSERT_TRUE(IsBubbleShown());
  ASSERT_TRUE(GetGlanceableTrayBubble());

  GetGlanceableTrayBubble()->GetBubbleWidget()->Close();
  EXPECT_EQ(1u,
            fake_glanceables_tasks_client()->RunPendingGetTaskListsCallbacks());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsBubbleShown());
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

TEST_P(GlanceablesDateTrayTest,
       ScrollWholeGlanceableToVisibleWhenFocusMovesIn) {
  UpdateDisplay("1000x512");

  LeftClickOn(GetDateTray());
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  ASSERT_TRUE(GetGlanceableTrayBubble());

  auto* const scroll_view = views::AsViewClass<views::ScrollView>(
      GetGlanceableTrayBubble()->GetBubbleView()->children().at(0));
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();

  glanceables_classroom_client()->RespondToPendingIsStudentRoleEnabledCallbacks(
      true);
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();

  auto* student_view = GetGlanceableTrayBubble()->GetClassroomStudentView();
  ASSERT_TRUE(student_view);

  auto* calendar_view = GetGlanceableTrayBubble()->GetCalendarView();
  ASSERT_TRUE(calendar_view);

  ASSERT_TRUE(glanceables_classroom_client()
                  ->RespondToNextPendingStudentAssignmentsCallback(
                      CreateAssignmentsForStudents(/*count=*/3)));

  // The display is not large enough to accommodate all glanceables - verify
  // that calendar view is visible by default.
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      student_view->GetBoundsInScreen()));
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  // Focus the student assignment selector, and verify the whole student
  // glanceable becomes visible.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  Combobox* assignment_selector =
      views::AsViewClass<Combobox>(student_view->GetViewByID(
          base::to_underlying(GlanceablesViewId::kClassroomBubbleComboBox)));
  assignment_selector->RequestFocus();
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      student_view->GetBoundsInScreen()));
  EXPECT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));

  // Scroll the glanceable container so student glanceable partially moves out
  // of the view port. Verify that moving focus to the next element within the
  // glanceable does not cause the whole glanceable to appear.
  scroll_view->ScrollByOffset(gfx::PointF(0, 20));
  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  ASSERT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      student_view->GetBoundsInScreen()));

  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  ASSERT_FALSE(assignment_selector->HasFocus());

  scroll_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      student_view->GetBoundsInScreen()));
  EXPECT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      calendar_view->GetBoundsInScreen()));
}

TEST_P(GlanceablesDateTrayTest, ClickOutsideComboboxMenu) {
  EXPECT_FALSE(GetGlanceableTrayBubble());

  // Click the date tray to show the glanceable bubbles.
  GetEventGenerator()->MoveMouseTo(
      GetDateTray()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(GetGlanceableTrayBubble());
  EXPECT_TRUE(GetTasksView());

  // Check that the tasks glanceable is completely shown on the primary screen.
  GetTasksView()->ScrollViewToVisible();
  EXPECT_TRUE(
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().Contains(
          GetTasksView()->GetBoundsInScreen()));

  // Click on the combo box to show the task lists.
  GetEventGenerator()->MoveMouseTo(
      GetTasksComboBoxView()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(GetTasksComboBoxView()->IsMenuRunning());
  EXPECT_TRUE(GetGlanceableTrayBubble());

  // Click at the top of the tasks view and make sure the menu gets closed.
  GetEventGenerator()->MoveMouseTo(
      GetTasksView()->GetBoundsInScreen().top_center());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(GetTasksComboBoxView()->IsMenuRunning());
  EXPECT_TRUE(GetGlanceableTrayBubble());

  WaitForTimeBetweenButtonOnClicks();

  // Click on the combo box to show the task lists again.
  GetEventGenerator()->MoveMouseTo(
      GetTasksComboBoxView()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(GetTasksComboBoxView()->IsMenuRunning());
  EXPECT_TRUE(GetGlanceableTrayBubble());

  const gfx::Point left_side_of_screen =
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().left_center();

  // Click outside of the glanceables bubbles, on the left side of the screen.
  GetEventGenerator()->MoveMouseTo(left_side_of_screen);
  GetEventGenerator()->ClickLeftButton();

  // Check that the combobox menu is closed, but that the glaneable bubbles
  // are still open.
  EXPECT_FALSE(GetTasksComboBoxView()->IsMenuRunning());
  EXPECT_TRUE(GetGlanceableTrayBubble());

  // Click outside the glanceabes bubble again to ensure closing occurs.
  GetEventGenerator()->MoveMouseTo(left_side_of_screen);
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(GetGlanceableTrayBubble());
}

}  // namespace ash
