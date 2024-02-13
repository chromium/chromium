// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shelf/test/shelf_test_base.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

class ShelfAppButtonPixelTest
    : public ShelfTestBase,
      public testing::WithParamInterface</*use_rtl=*/bool> {
 public:
  ShelfAppButtonPixelTest() : use_rtl_(GetParam()) {}

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = use_rtl();
    return init_params;
  }

  // ShelfTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kCrosWebAppShortcutUiUpdate,
                              ash::features::kSeparateWebAppShortcutBadgeIcon},
        /*disabled_features=*/{});
    ShelfTestBase::SetUp();
  }

  void TearDown() override { ShelfTestBase::TearDown(); }

  ShelfAppButton* GetItemViewAt(ShelfID id) {
    return GetPrimaryShelf()
        ->shelf_widget()
        ->shelf_view_for_testing()
        ->GetShelfAppButton(id);
  }

  bool use_rtl() const { return use_rtl_; }

 private:
  const bool use_rtl_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ShelfAppButtonPixelTest,
                         /*use_rtl=*/testing::Bool());

TEST_P(ShelfAppButtonPixelTest, WebAppShortcutIconEffectsExists) {
  const ShelfItem item = AddWebAppShortcut();
  ShelfAppButton* item_view = GetItemViewAt(item.id);
  item_view->GetWidget()->LayoutRootViewIfNecessary();

  ShelfViewTestAPI shelf_view_test_api(
      GetPrimaryShelf()->GetShelfViewForTesting());
  shelf_view_test_api.RunMessageLoopUntilAnimationsDone();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "web_app_shortcut_icon_effects_exists", /*revision_number=*/1,
      item_view));
}

}  // namespace ash
