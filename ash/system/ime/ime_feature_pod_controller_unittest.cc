// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/ime_feature_pod_controller.h"

#include <string>
#include <vector>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Tests manually control their session state.
class IMEFeaturePodControllerTest : public NoSessionAshTestBase {
 public:
  IMEFeaturePodControllerTest() = default;

  IMEFeaturePodControllerTest(const IMEFeaturePodControllerTest&) = delete;
  IMEFeaturePodControllerTest& operator=(const IMEFeaturePodControllerTest&) =
      delete;

  ~IMEFeaturePodControllerTest() override = default;

  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  void TearDown() override {
    tile_.reset();
    controller_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  void SetUpButton() {
    controller_ = std::make_unique<IMEFeaturePodController>(tray_controller());
    tile_ = controller_->CreateTile();
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  bool IsButtonVisible() { return tile_->GetVisible(); }

  const std::u16string GetTooltipText() { return tile_->GetTooltipText(); }

  const char* GetToggledOnHistogramName() {
    return "Ash.QuickSettings.FeaturePod.ToggledOn";
  }

  const char* GetToggledOffHistogramName() {
    return "Ash.QuickSettings.FeaturePod.ToggledOff";
  }

  const char* GetDiveInHistogramName() {
    return "Ash.QuickSettings.FeaturePod.DiveIn";
  }

  // Creates |count| simulated active IMEs.
  void SetActiveIMECount(int count) {
    available_imes_.resize(count);
    for (int i = 0; i < count; ++i) {
      available_imes_[i].id = base::NumberToString(i);
    }
    RefreshImeController();
  }

  void RefreshImeController() {
    std::vector<ImeInfo> available_imes;
    for (const auto& ime : available_imes_) {
      available_imes.push_back(ime);
    }

    std::vector<ImeMenuItem> menu_items;
    for (const auto& item : menu_items_) {
      menu_items_.push_back(item);
    }

    Shell::Get()->ime_controller()->RefreshIme(
        current_ime_.id, std::move(available_imes), std::move(menu_items));
  }

  void PressIcon() { controller_->OnIconPressed(); }

  void PressLabel() { controller_->OnLabelPressed(); }

  std::unique_ptr<IMEFeaturePodController> controller_;
  std::unique_ptr<FeatureTile> tile_;

  // IMEs
  ImeInfo current_ime_;
  std::vector<ImeInfo> available_imes_;
  std::vector<ImeMenuItem> menu_items_;
};

TEST_F(IMEFeaturePodControllerTest, Labels) {
  SetUpButton();

  std::u16string label = tile_->label()->GetText();

  EXPECT_EQ(label, u"Keyboard");

  SetActiveIMECount(2);
  current_ime_.id = "0";
  available_imes_[0].short_name = u"US";
  available_imes_[1].short_name = u"FR";
  RefreshImeController();
  std::u16string sub_label = tile_->sub_label()->GetText();
  EXPECT_EQ(sub_label, u"US");
}

// Tests that if the pod button is hidden if less than 2 IMEs are present.
TEST_F(IMEFeaturePodControllerTest, ButtonVisibilityIMECount) {
  SetUpButton();

  SetActiveIMECount(0);
  EXPECT_FALSE(IsButtonVisible());
  SetActiveIMECount(1);
  EXPECT_FALSE(IsButtonVisible());
  SetActiveIMECount(2);
  EXPECT_TRUE(IsButtonVisible());
}

TEST_F(IMEFeaturePodControllerTest, ButtonVisibilityImeMenuActive) {
  SetUpButton();
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);

  SetActiveIMECount(0);
  EXPECT_FALSE(IsButtonVisible());
  SetActiveIMECount(1);
  EXPECT_FALSE(IsButtonVisible());
  SetActiveIMECount(2);
  EXPECT_FALSE(IsButtonVisible());
}

TEST_F(IMEFeaturePodControllerTest, ButtonVisibilityPolicy) {
  SetUpButton();

  Shell::Get()->ime_controller()->SetImesManagedByPolicy(true);

  SetActiveIMECount(0);
  EXPECT_TRUE(IsButtonVisible());
  SetActiveIMECount(1);
  EXPECT_TRUE(IsButtonVisible());
  SetActiveIMECount(2);
  EXPECT_TRUE(IsButtonVisible());
}

// TODO(crbug.com/40893381): Test is failing on "Linux ChromiumOS MSan Tests".
#if defined(MEMORY_SANITIZER)
#define MAYBE_IconUMATracking DISABLED_IconUMATracking
#else
#define MAYBE_IconUMATracking IconUMATracking
#endif
TEST_F(IMEFeaturePodControllerTest, MAYBE_IconUMATracking) {
  SetUpButton();

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/0);

  // Show IME detailed view when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(GetDiveInHistogramName(),
                                      QsFeatureCatalogName::kIME,
                                      /*expected_count=*/1);
}

TEST_F(IMEFeaturePodControllerTest, LabelUMATracking) {
  SetUpButton();

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/0);

  // Show IME detailed view when pressing on the label.
  PressLabel();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(GetDiveInHistogramName(),
                                      QsFeatureCatalogName::kIME,
                                      /*expected_count=*/1);
}

// Tests the tooltip changes after the IME refreshes.
TEST_F(IMEFeaturePodControllerTest, TooltipText) {
  SetUpButton();

  SetActiveIMECount(2);
  current_ime_.id = "0";
  available_imes_[0].name = u"English";
  available_imes_[1].name = u"French";

  RefreshImeController();
  std::u16string tooltip = GetTooltipText();
  EXPECT_EQ(tooltip, u"Show keyboard settings. English is selected.");

  // Switches the current ime to the second one in `available_imes_`.
  current_ime_.id = "1";
  RefreshImeController();
  tooltip = GetTooltipText();
  EXPECT_EQ(tooltip, u"Show keyboard settings. French is selected.");
}

}  // namespace ash
