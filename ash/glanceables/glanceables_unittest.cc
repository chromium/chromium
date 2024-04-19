// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/test/glanceables_tasks_test_util.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class GlanceablesTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    const auto account_id =
        AccountId::FromUserEmailGaiaId("test_user@gmail.com", "123456");
    SimulateUserLogin(account_id);

    tasks_client_ = glanceables_tasks_test_util::InitializeFakeTasksClient(
        base::Time::Now());
    Shell::Get()->glanceables_controller()->UpdateClientsRegistration(
        account_id, GlanceablesController::ClientsRegistration{
                        .tasks_client = tasks_client_.get()});
  }

  api::FakeTasksClient* tasks_client() const { return tasks_client_.get(); }

 private:
  base::test::ScopedFeatureList features{
      features::kGlanceablesTimeManagementTasksView};
  std::unique_ptr<api::FakeTasksClient> tasks_client_;
};

TEST_F(GlanceablesTest, DoesNotAddTasksViewWhenDisabledByAdmin) {
  auto* const date_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->date_tray();

  // Open Glanceables via Search + C, make sure the bubble is shown with the
  // Tasks view available.
  EXPECT_FALSE(date_tray->is_active());
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(date_tray->is_active());
  EXPECT_TRUE(date_tray->glanceables_bubble_for_test()->GetTasksView());

  // Close Glanceables.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));

  // Simulate that admin disables the integration.
  tasks_client()->set_is_disabled_by_admin(true);

  // Open Glanceables via Search + C again, make sure the bubble no longer
  // contains the Tasks view.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(date_tray->is_active());
  EXPECT_FALSE(date_tray->glanceables_bubble_for_test()->GetTasksView());

  // Close Glanceables.
  ShellTestApi().PressAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN));
}

}  // namespace ash
