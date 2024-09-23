// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/ime_info.h"
#include "ash/shell.h"
#include "ash/system/ime/ime_detailed_view.h"
#include "ash/system/ime_menu/ime_list_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"

namespace ash {
namespace {

class IMEDetailedViewPixelTest : public AshTestBase {
 public:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }
};

TEST_F(IMEDetailedViewPixelTest, Basics) {
  // Set up some IMEs.
  std::vector<ImeInfo> available_imes;
  ImeInfo ime1;
  ime1.id = "ime1";
  ime1.name = u"English";
  ime1.short_name = u"US";
  available_imes.push_back(ime1);
  ImeInfo ime2;
  ime2.id = "ime2";
  ime2.name = u"Spanish";
  ime2.short_name = u"ES";
  available_imes.push_back(ime2);

  // Show the enterprise management icon.
  Shell::Get()->ime_controller()->SetImesManagedByPolicy(true);

  // Show the detailed view.
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());
  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowIMEDetailedView();

  // Compare pixels.
  TrayDetailedView* detailed_view =
      system_tray->bubble()
          ->quick_settings_view()
          ->GetDetailedViewForTest<TrayDetailedView>();

  // Show the keyboard toggle with ime list.
  static_cast<IMEDetailedView*>(detailed_view)
      ->Update(ime1.id, std::move(available_imes), std::vector<ImeMenuItem>(),
               /*show_keyboard_toggle=*/true,
               /*single_ime_behavior=*/ImeListView::SHOW_SINGLE_IME);

  ASSERT_TRUE(detailed_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/11, detailed_view));
}

}  // namespace
}  // namespace ash
