// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "ash/public/cpp/locale_update_controller.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"

namespace ash {
namespace {

class LocaleDetailedViewPixelTest : public AshTestBase {
 public:
  LocaleDetailedViewPixelTest() = default;

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }
};

TEST_F(LocaleDetailedViewPixelTest, Basics) {
  // Setup two locales in the locale list.
  std::vector<LocaleInfo> locale_list;
  locale_list.emplace_back("en-US", u"English (United States)");
  locale_list.emplace_back("fr-FR", u"French (France)");
  Shell::Get()->system_tray_model()->SetLocaleList(std::move(locale_list),
                                                   "en-US");

  // Show the detailed view.
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());
  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowLocaleDetailedView();

  // Compare pixels.
  TrayDetailedView* detailed_view =
      system_tray->bubble()
          ->quick_settings_view()
          ->GetDetailedViewForTest<TrayDetailedView>();
  ASSERT_TRUE(detailed_view);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/10, detailed_view));
}

}  // namespace
}  // namespace ash
