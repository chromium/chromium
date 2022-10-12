// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tiles_container_view.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/test/views_test_utils.h"

namespace ash {

// TODO(crbug/1368717): use FeatureTile.
class FeatureTilesContainerViewTest : public NoSessionAshTestBase,
                                      public views::ViewObserver {
 public:
  FeatureTilesContainerViewTest() = default;

  FeatureTilesContainerViewTest(const FeatureTilesContainerViewTest&) = delete;
  FeatureTilesContainerViewTest& operator=(
      const FeatureTilesContainerViewTest&) = delete;

  ~FeatureTilesContainerViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    container_ = std::make_unique<FeatureTilesContainerView>(controller());
    container_->AddObserver(this);
  }

  void TearDown() override {
    container_->RemoveObserver(this);
    container_.reset();
    GetPrimaryUnifiedSystemTray()->CloseBubble();
    NoSessionAshTestBase::TearDown();
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override {
    ++preferred_size_changed_count_;
  }

  FeatureTilesContainerView* container() { return container_.get(); }

  UnifiedSystemTrayController* controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  int preferred_size_changed_count() const {
    return preferred_size_changed_count_;
  }

  int CalculateRowsFromHeight(int height) {
    return container()->CalculateRowsFromHeight(height);
  }

  // TODO(crbug/1368717): use FeatureTile.
  std::vector<FeaturePodButton*> buttons_;

 private:
  std::unique_ptr<FeatureTilesContainerView> container_;
  int preferred_size_changed_count_ = 0;
};

TEST_F(FeatureTilesContainerViewTest, CalculateRowsFromHeight) {
  int row_height = kFeatureTileHeight;

  // Expect max number of rows even if available height could fit another row.
  EXPECT_EQ(kFeatureTileMaxRows,
            CalculateRowsFromHeight((kFeatureTileMaxRows + 1) * row_height));

  // Expect appropriate number of rows with available height.
  EXPECT_EQ(3, CalculateRowsFromHeight(3 * row_height));

  // Expect min number of rows even with zero height.
  EXPECT_EQ(kFeatureTileMinRows, CalculateRowsFromHeight(0));
}

}  // namespace ash
