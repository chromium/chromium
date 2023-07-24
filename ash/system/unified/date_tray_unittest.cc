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
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/glanceables/tasks/fake_glanceables_tasks_client.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/system/unified/tasks_bubble_view.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_utils.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

namespace ash {

namespace {

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
    std::move(cb).Run({});
  }
  void GetStudentAssignmentsWithApproachingDueDate(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    std::move(cb).Run({});
  }
  void GetStudentAssignmentsWithMissedDueDate(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    std::move(cb).Run({});
  }
  void GetStudentAssignmentsWithoutDueDate(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    std::move(cb).Run({});
  }
  void GetTeacherAssignmentsWithApproachingDueDate(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    std::move(cb).Run({});
  }
  void GetTeacherAssignmentsRecentlyDue(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    std::move(cb).Run({});
  }
  void GetTeacherAssignmentsWithoutDueDate(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    std::move(cb).Run({});
  }
  void GetGradedTeacherAssignments(
      GlanceablesClassroomClient::GetAssignmentsCallback cb) override {
    std::move(cb).Run({});
  }
  void OpenUrl(const GURL& url) const override {}
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

 private:
  std::vector<GlanceablesClassroomClient::IsRoleEnabledCallback>
      pending_is_student_role_enabled_callbacks_;
  std::vector<GlanceablesClassroomClient::IsRoleEnabledCallback>
      pending_is_teacher_role_enabled_callbacks_;

  // Number of times `OnGlanceablesBubbleClosed()` has been called.
  int bubble_closed_count_ = 0;
};

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

}  // namespace

class DateTrayTest
    : public AshTestBase,
      public wm::ActivationChangeObserver,
      public testing::WithParamInterface</*glanceables_v2_enabled=*/bool> {
 public:
  DateTrayTest() {
    scoped_feature_list_.InitWithFeatureState(features::kGlanceablesV2,
                                              GetParam());

    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    window_delegate_provider_ =
        std::make_unique<ash::TestNewWindowDelegateProvider>(
            std::move(delegate));
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
          std::make_unique<FakeGlanceablesTasksClient>();
      Shell::Get()->glanceables_v2_controller()->UpdateClientsRegistration(
          account_id_,
          GlanceablesV2Controller::ClientsRegistration{
              .classroom_client = glanceables_classroom_client_.get(),
              .tasks_client = fake_glanceables_tasks_client_.get()});
    }
  }

  void TearDown() override {
    if (AreGlanceablesV2Enabled()) {
      Shell::Get()->glanceables_v2_controller()->UpdateClientsRegistration(
          account_id_, GlanceablesV2Controller::ClientsRegistration{});
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

  GlanceableTrayBubble* GetGlanceableTrayBubble() {
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

  void LeftClickOnOpenBubble() {
    if (AreGlanceablesV2Enabled()) {
      LeftClickOn(GetGlanceableTrayBubble()->GetBubbleView());

    } else {
      LeftClickOn(GetUnifiedSystemTray()->bubble()->GetBubbleView());
    }
  }

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
    GetUnifiedSystemTray()->CloseBubble();
  }

  TestGlanceablesClassroomClient* glanceables_classroom_client() {
    return glanceables_classroom_client_.get();
  }

  FakeGlanceablesTasksClient* fake_glanceables_tasks_client() {
    return fake_glanceables_tasks_client_.get();
  }

  MockNewWindowDelegate* new_window_delegate() { return new_window_delegate_; }

  void RemoveGlanceablesClients() {
    Shell::Get()->glanceables_v2_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesV2Controller::ClientsRegistration{
                         .classroom_client = nullptr, .tasks_client = nullptr});
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("test_user@gmail.com", "123456");
  std::unique_ptr<TestGlanceablesClassroomClient> glanceables_classroom_client_;
  std::unique_ptr<FakeGlanceablesTasksClient> fake_glanceables_tasks_client_;
  bool observering_activation_changes_ = false;

  // Owned by `widget_`.
  raw_ptr<DateTray, ExperimentalAsh> date_tray_ = nullptr;

  raw_ptr<UnifiedSystemTray, ExperimentalAsh> unified_system_tray_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<ash::TestNewWindowDelegateProvider> window_delegate_provider_;
  raw_ptr<MockNewWindowDelegate, DanglingUntriaged> new_window_delegate_ =
      nullptr;
};

INSTANTIATE_TEST_SUITE_P(GlanceablesV2, DateTrayTest, testing::Bool());

// Tests that toggling the `CalendarView` via the date tray accelerator does not
// result in a crash when the unified system tray bubble is set to immediately
// close upon activation. See crrev/c/1419499 for details.
TEST_P(DateTrayTest, AcceleratorOpenAndImmediateCloseDoesNotCrash) {
  ImmediatelyCloseBubbleOnActivation();
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

TEST_P(DateTrayTest, ShowTasksComboModel) {
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  if (!AreGlanceablesV2Enabled()) {
    EXPECT_EQ(GetGlanceableTrayBubble(), nullptr);
  } else {
    auto* tasks_view = GetGlanceableTrayBubble()->GetTasksView();
    EXPECT_TRUE(tasks_view->GetVisible());
    EXPECT_FALSE(tasks_view->IsMenuRunning());
    EXPECT_TRUE(tasks_view->task_list_combo_box_view()->GetVisible());
    tasks_view->GetWidget()->LayoutRootViewIfNecessary();

    EXPECT_EQ(tasks_view->task_items_container_view()->children().size(), 2u);

    // Verify that tapping on combobox opens the selection menu.
    GestureTapOn(tasks_view->task_list_combo_box_view());
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(tasks_view->IsMenuRunning());

    // Select the next task list using keyboard navigation.
    PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
    PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);

    // Verify the number of items in task_items_container_view()->children().
    EXPECT_EQ(tasks_view->task_items_container_view()->children().size(), 3u);
  }
}

TEST_P(DateTrayTest, MarkTaskAsComplete) {
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  if (!AreGlanceablesV2Enabled()) {
    EXPECT_EQ(GetGlanceableTrayBubble(), nullptr);
  } else {
    EXPECT_TRUE(GetGlanceableTrayBubble()->GetTasksView()->GetVisible());
    EXPECT_FALSE(GetGlanceableTrayBubble()->GetTasksView()->IsMenuRunning());
    EXPECT_TRUE(GetGlanceableTrayBubble()
                    ->GetTasksView()
                    ->task_list_combo_box_view()
                    ->GetVisible());
    EXPECT_EQ(GetGlanceableTrayBubble()
                  ->GetTasksView()
                  ->task_items_container_view()
                  ->children()
                  .size(),
              2u);

    // Verify that tapping on combobox opens the selection menu.
    GlanceablesTaskView* task_view = views::AsViewClass<GlanceablesTaskView>(
        GetGlanceableTrayBubble()
            ->GetTasksView()
            ->task_items_container_view()
            ->children()[0]);

    ASSERT_TRUE(task_view);
    task_view->GetWidget()->LayoutRootViewIfNecessary();
    ASSERT_FALSE(task_view->GetCompletedForTest());
    ASSERT_EQ(0u, fake_glanceables_tasks_client()->completed_tasks().size());
    GestureTapOn(task_view->GetButtonForTest());
    ASSERT_TRUE(task_view->GetCompletedForTest());
    ASSERT_EQ(1u, fake_glanceables_tasks_client()->completed_tasks().size());
    ASSERT_EQ("TaskListID1:TaskListItem1",
              fake_glanceables_tasks_client()->completed_tasks().front());
  }
}

// Tests that tapping the tasks glanceable action button opens a browser page.
TEST_P(DateTrayTest, ShowTasksWebUI) {
  LeftClickOn(GetDateTray());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsBubbleShown());
  EXPECT_TRUE(AreContentsViewShown());

  if (!AreGlanceablesV2Enabled()) {
    EXPECT_EQ(GetGlanceableTrayBubble(), nullptr);
  } else {
    const auto* const see_all_button = views::AsViewClass<views::LabelButton>(
        GetGlanceableTrayBubble()->GetTasksView()->GetViewByID(
            base::to_underlying(GlanceablesViewId::kListFooterSeeAllButton)));
    EXPECT_CALL(*new_window_delegate(), OpenUrl).Times(1);
    GestureTapOn(see_all_button);
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

}  // namespace ash
