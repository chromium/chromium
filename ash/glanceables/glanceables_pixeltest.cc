// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/test/glanceables_tasks_test_util.h"
#include "ash/shelf/shelf.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/system/unified/glanceable_tray_bubble_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"

namespace {
constexpr char due_date[] = "2 Aug 2025 10:00 GMT";
}

namespace ash {

class GlanceablesPixelTest : public AshTestBase {
 public:
  GlanceablesPixelTest() {
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
    ASSERT_TRUE(base::Time::FromString(due_date, &date));
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

  void OpenGlanceables() { LeftClickOn(GetDateTray()); }

 private:
  base::test::ScopedFeatureList features_{
      ash::features::kGlanceablesTimeManagementTasksView};
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;
  AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("test_user@gmail.com", "123456");
  std::unique_ptr<api::FakeTasksClient> fake_glanceables_tasks_client_;
};

// Pixel test for default / basic glanceables functionality.
TEST_F(GlanceablesPixelTest, Smoke) {
  ASSERT_FALSE(GetDateTray()->is_active());
  OpenGlanceables();
  ASSERT_TRUE(GetDateTray()->is_active());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "glanceables_smoke", /*revision_number=*/3,
      GetDateTray()->glanceables_bubble_for_test()->GetBubbleView()));
}

}  // namespace ash
