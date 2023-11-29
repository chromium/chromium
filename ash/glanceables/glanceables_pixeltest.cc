// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/shelf/shelf.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/system/unified/glanceable_tray_bubble_view.h"
#include "ash/system/unified/tasks_bubble_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_utils.h"

namespace {
constexpr char due_date[] = "2 Aug 2025 10:00 GMT";
}

namespace ash {

class GlanceablesPixelTest : public AshTestBase {
 public:
  GlanceablesPixelTest() = default;
  GlanceablesPixelTest(const GlanceablesPixelTest&) = delete;
  GlanceablesPixelTest& operator=(const GlanceablesPixelTest&) = delete;
  ~GlanceablesPixelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    SimulateUserLogin(account_id_);

    widget_ = CreateFramelessTestWidget();
    widget_->SetContentsView(std::make_unique<views::View>());
    widget_->SetFullscreen(true);
    widget_->GetContentsView()->AddChildView(GetDateTray());
    GetDateTray()->SetVisiblePreferred(true);

    base::Time date;
    ASSERT_TRUE(base::Time::FromString(due_date, &date));
    fake_glanceables_tasks_client_ =
        std::make_unique<api::FakeTasksClient>(date);
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesController::ClientsRegistration{
                         .tasks_client = fake_glanceables_tasks_client_.get()});
  }

  // AshTestBase:
  void TearDown() override {
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesController::ClientsRegistration{});
    widget_.reset();
    AshTestBase::TearDown();
  }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  DateTray* GetDateTray() {
    return GetPrimaryShelf()->GetStatusAreaWidget()->date_tray();
  }

  GlanceableTrayBubble* GetGlanceableTrayBubble() {
    return GetDateTray()->bubble_.get();
  }

  void OpenGlanceables() { LeftClickOn(GetDateTray()); }

  api::FakeTasksClient* fake_glanceables_tasks_client() {
    return fake_glanceables_tasks_client_.get();
  }

 protected:
  base::test::ScopedFeatureList features_{ash::features::kGlanceablesV2};

 private:
  std::unique_ptr<views::Widget> widget_;
  AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("test_user@gmail.com", "123456");
  std::unique_ptr<api::FakeTasksClient> fake_glanceables_tasks_client_;
};

// Pixel test for glanceables when no data is available.
// Test disabled due to not taking dark/light mode into consideration.
// http://b/294612234
TEST_F(GlanceablesPixelTest, GlanceablesZeroState) {
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time date;
        bool result = base::Time::FromString("28 Jul 2023 10:00 GMT", &date);
        DCHECK(result);
        return date;
      },
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  ASSERT_FALSE(GetDateTray()->is_active());
  OpenGlanceables();
  ASSERT_TRUE(GetDateTray()->is_active());
  // Scroll to the top of the glanceables view.
  GetGlanceableTrayBubble()->GetTasksView()->ScrollViewToVisible();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "glanceables_zero_state", /*revision_number=*/6,
      GetGlanceableTrayBubble()->GetBubbleView()));
}

// Pixel test verifying initial UI for tasks glanceable as well as UI updates
// when a task is marked as completed.
// Test disabled due to not taking dark/light mode into consideration.
// http://b/294612234
TEST_F(GlanceablesPixelTest, GlanceablesTasksMarkAsCompleted) {
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time date;
        bool result = base::Time::FromString("28 Jul 2023 10:00 GMT", &date);
        DCHECK(result);
        return date;
      },
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  ASSERT_FALSE(GetDateTray()->is_active());
  OpenGlanceables();
  ASSERT_TRUE(GetDateTray()->is_active());
  // Scroll to the top of the glanceables view.
  GetGlanceableTrayBubble()->GetTasksView()->ScrollViewToVisible();

  GlanceablesTaskView* task_view = views::AsViewClass<GlanceablesTaskView>(
      views::AsViewClass<views::View>(
          GetGlanceableTrayBubble()->GetTasksView()->GetViewByID(
              base::to_underlying(
                  GlanceablesViewId::kTasksBubbleListContainer)))
          ->children()[0]);
  ASSERT_TRUE(task_view);
  task_view->GetWidget()->LayoutRootViewIfNecessary();
  ASSERT_FALSE(task_view->GetCompletedForTest());
  ASSERT_EQ(0u,
            fake_glanceables_tasks_client()->pending_completed_tasks().size());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "glanceables_task_view_no_completed_tasks", /*revision_number=*/3,
      GetGlanceableTrayBubble()->GetTasksView()));

  GestureTapOn(task_view->GetButtonForTest());
  ASSERT_TRUE(task_view->GetCompletedForTest());
  ASSERT_EQ(1u,
            fake_glanceables_tasks_client()->pending_completed_tasks().size());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "glanceables_task_view_one_completed_task", /*revision_number=*/3,
      GetGlanceableTrayBubble()->GetTasksView()));
}

// Pixel test for calendar bubble height with `kGlanceablesV2CalendarView`
// enabled.
TEST_F(GlanceablesPixelTest, GlanceablesCalendarHeight) {
  features_.Reset();
  features_.InitWithFeatures({ash::features::kGlanceablesV2,
                              ash::features::kGlanceablesV2CalendarView},
                             /*disabled_features=*/{});

  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time date;
        bool result = base::Time::FromString("28 Jul 2023 10:00 GMT", &date);
        DCHECK(result);
        return date;
      },
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  ASSERT_FALSE(GetDateTray()->is_active());
  OpenGlanceables();
  ASSERT_TRUE(GetDateTray()->is_active());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "glanceables_calendar_height", /*revision_number=*/1,
      GetGlanceableTrayBubble()->GetBubbleView()));
}

}  // namespace ash
