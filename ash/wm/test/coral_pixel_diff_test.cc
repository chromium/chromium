// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/birch/birch_item_remover.h"
#include "ash/birch/birch_model.h"
#include "ash/birch/test_birch_client.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/wm/coral/coral_test_util.h"
#include "ash/wm/overview/birch/birch_privacy_nudge_controller.h"
#include "ash/wm/overview/birch/tab_app_selection_host.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class CoralPixelDiffTest : public AshTestBase {
 public:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

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

  base::test::ScopedFeatureList scoped_features_{features::kCoralFeature};
};

TEST_F(CoralPixelDiffTest, CoralSelectorView) {
  // Mark the privacy nudge as shown otherwise it will occlude parts of the
  // selector.
  BirchPrivacyNudgeController::DidShowContextMenu();

  UpdateDisplay("1600x1000");

  TabAppSelectionHost* menu = ShowAndGetSelectorMenu(GetEventGenerator());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "coral_selector_view", /*revision_number=*/3, menu));
}

}  // namespace ash
