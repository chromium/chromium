// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/ime_info.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {
namespace {

class IMEDetailedViewPixelTest : public AshTestBase {
 public:
  IMEDetailedViewPixelTest() {
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, chromeos::features::kJelly}, {});
  }

  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  base::test::ScopedFeatureList feature_list_;
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
  auto* ime_controller = Shell::Get()->ime_controller();
  ime_controller->RefreshIme(ime1.id, std::move(available_imes),
                             std::vector<ImeMenuItem>());

  // Show the enterprise management icon.
  ime_controller->SetImesManagedByPolicy(true);

  // Show the detailed view.
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());
  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowIMEDetailedView();

  // Compare pixels.
  TrayDetailedView* detailed_view =
      system_tray->bubble()->quick_settings_view()->GetDetailedViewForTest();
  ASSERT_TRUE(detailed_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/3, detailed_view));
}

}  // namespace
}  // namespace ash
