// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/glanceables/classroom/fake_glanceables_classroom_client.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/test/glanceables_tasks_test_util.h"
#include "ash/shelf/shelf.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/system/unified/glanceable_tray_bubble_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "ui/views/controls/scroll_view.h"

namespace {
constexpr char kDueDate[] = "2 Aug 2025 10:00 GMT";
}

namespace ash {

class GlanceablesTasksPixelTest : public AshTestBase {
 public:
  GlanceablesTasksPixelTest() {
    time_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        []() {
          base::Time date;
          bool result = base::Time::FromString("28 Jul 2023 10:00 GMT", &date);
          DCHECK(result);
          return date;
        },
        /*time_ticks_override=*/nullptr,
        /*thread_ticks_override=*/nullptr);
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    SimulateUserLogin(account_id_);

    base::Time date;
    ASSERT_TRUE(base::Time::FromString(kDueDate, &date));
    fake_glanceables_tasks_client_ =
        glanceables_tasks_test_util::InitializeFakeTasksClient(date);
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesController::ClientsRegistration{
                         .tasks_client = fake_glanceables_tasks_client_.get()});
  }

  // AshTestBase:
  void TearDown() override {
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesController::ClientsRegistration{});
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

  void ToggleGlanceables() { LeftClickOn(GetDateTray()); }

 protected:
  const AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("test_user@gmail.com", "123456");
  std::unique_ptr<api::FakeTasksClient> fake_glanceables_tasks_client_;

 private:
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;
};

// Pixel test for default / basic glanceables functionality.
TEST_F(GlanceablesTasksPixelTest, Smoke) {
  ASSERT_FALSE(GetDateTray()->is_active());
  ToggleGlanceables();
  ASSERT_TRUE(GetDateTray()->is_active());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "glanceables_smoke", /*revision_number=*/0,
      GetDateTray()->glanceables_bubble_for_test()->GetBubbleView()));
}

class GlanceablesTimeManagementPixelTest : public GlanceablesTasksPixelTest {
 public:
  // AshTestBase:
  void SetUp() override {
    GlanceablesTasksPixelTest::SetUp();

    base::Time date;
    ASSERT_TRUE(base::Time::FromString(kDueDate, &date));
    // tasks client was initialized in GlanceablesTasksPixelTest.
    classroom_client_ = std::make_unique<FakeGlanceablesClassroomClient>();
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesController::ClientsRegistration{
                         .classroom_client = classroom_client_.get(),
                         .tasks_client = fake_glanceables_tasks_client_.get()});

    ASSERT_TRUE(Shell::Get()->glanceables_controller()->GetClassroomClient());
    ASSERT_TRUE(Shell::Get()->glanceables_controller()->GetTasksClient());

    GetDateTray()->ShowGlanceableBubble(/*from_keyboard=*/false);
    view_ = views::AsViewClass<GlanceableTrayBubbleView>(
        GetDateTray()->glanceables_bubble_for_test()->GetBubbleView());
  }

  void TearDown() override {
    GetDateTray()->HideGlanceableBubble();
    view_ = nullptr;
    GlanceablesTasksPixelTest::TearDown();
  }

  GlanceablesTasksView* GetTasksView() const {
    return views::AsViewClass<GlanceablesTasksView>(view_->GetTasksView());
  }

  views::ScrollView* GetTasksScrollView() const {
    return views::AsViewClass<views::ScrollView>(GetTasksView()->GetViewByID(
        base::to_underlying(GlanceablesViewId::kContentsScrollView)));
  }

  GlanceablesClassroomStudentView* GetClassroomView() const {
    return views::AsViewClass<GlanceablesClassroomStudentView>(
        view_->GetClassroomStudentView());
  }

  views::ScrollView* GetClassroomScrollView() const {
    return views::AsViewClass<views::ScrollView>(
        GetClassroomView()->GetViewByID(
            base::to_underlying(GlanceablesViewId::kContentsScrollView)));
  }

 private:
  std::unique_ptr<FakeGlanceablesClassroomClient> classroom_client_;
  raw_ptr<GlanceableTrayBubbleView> view_ = nullptr;
};

// Pixel test for default / basic glanceables functionality.
TEST_F(GlanceablesTimeManagementPixelTest, Smoke) {
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "expanded_tasks_top", /*revision_number=*/1,
      GetDateTray()->glanceables_bubble_for_test()->GetBubbleView()));
  GetTasksScrollView()->ScrollToPosition(
      GetTasksScrollView()->vertical_scroll_bar(), INT_MAX);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "expanded_tasks_bottom", /*revision_number=*/1,
      GetDateTray()->glanceables_bubble_for_test()->GetBubbleView()));

  GetClassroomView()->SetExpandState(true, /*expand_by_overscroll=*/false);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "expanded_classroom_top", /*revision_number=*/1,
      GetDateTray()->glanceables_bubble_for_test()->GetBubbleView()));
  GetClassroomScrollView()->ScrollToPosition(
      GetClassroomScrollView()->vertical_scroll_bar(), INT_MAX);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "expanded_classroom_bottom", /*revision_number=*/1,
      GetDateTray()->glanceables_bubble_for_test()->GetBubbleView()));
}

}  // namespace ash
