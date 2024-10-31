// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_controller.h"

#include "ash/birch/birch_item_remover.h"
#include "ash/birch/birch_model.h"
#include "ash/birch/test_birch_client.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/coral/coral_test_util.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/view_utils.h"

namespace ash {

class CoralControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    // Create test birch client.
    auto* birch_model = Shell::Get()->birch_model();
    birch_client_ = std::make_unique<TestBirchClient>(birch_model);
    birch_model->SetClientAndInit(birch_client_.get());

    base::RunLoop run_loop;
    birch_model->GetItemRemoverForTest()->SetProtoInitCallbackForTest(
        run_loop.QuitClosure());
    run_loop.Run();

    // Prepare a coral response so we have a coral glanceable to click.
    std::vector<coral::mojom::GroupPtr> test_groups;
    test_groups.push_back(CreateDefaultTestGroup());
    OverrideTestResponse(std::move(test_groups));
  }

  void TearDown() override {
    Shell::Get()->birch_model()->SetClientAndInit(nullptr);
    birch_client_.reset();
    AshTestBase::TearDown();
  }

 private:
  std::unique_ptr<TestBirchClient> birch_client_;

  base::test::ScopedFeatureList feature_list_{features::kCoralFeature};
};

// Tests that clicking the coral chip and then it's addon view will not crash.
// Regression test for crbug.com/376549527.
TEST_F(CoralControllerTest, ClickChipWithMaxDesks) {
  // Add desks until no longer possible.
  while (DesksController::Get()->CanCreateDesks()) {
    NewDesk();
  }

  // Click the coral button and then the addon view when we have max desks.
  // Verify that there is no crash and the expected toast is shown.
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);
  BirchChipButton* coral_button = GetFirstCoralButton();
  LeftClickOn(coral_button);
  LeftClickOn(coral_button->addon_view());
  EXPECT_TRUE(
      Shell::Get()->toast_manager()->IsToastShown("coral_max_desks_toast"));
}

}  // namespace ash
