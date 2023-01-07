// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/ime_feature_pod_controller.h"

#include <vector>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"

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
    button_.reset();
    controller_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  void SetUpButton() {
    controller_ = std::make_unique<IMEFeaturePodController>(tray_controller());
    button_.reset(controller_->CreateButton());
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  FeaturePodButton* button() { return button_.get(); }

  // Creates |count| simulated active IMEs.
  void SetActiveIMECount(int count) {
    available_imes_.resize(count);
    for (int i = 0; i < count; ++i)
      available_imes_[i].id = base::NumberToString(i);
    RefreshImeController();
  }

  void RefreshImeController() {
    std::vector<ImeInfo> available_imes;
    for (const auto& ime : available_imes_)
      available_imes.push_back(ime);

    std::vector<ImeMenuItem> menu_items;
    for (const auto& item : menu_items_)
      menu_items_.push_back(item);

    Shell::Get()->ime_controller()->RefreshIme(
        current_ime_.id, std::move(available_imes), std::move(menu_items));
  }

  void PressIcon() { controller_->OnIconPressed(); }

  void PressLabel() { controller_->OnLabelPressed(); }

 private:
  std::unique_ptr<IMEFeaturePodController> controller_;
  std::unique_ptr<FeaturePodButton> button_;

  // IMEs
  ImeInfo current_ime_;
  std::vector<ImeInfo> available_imes_;
  std::vector<ImeMenuItem> menu_items_;
};

// Tests that if the pod button is hidden if less than 2 IMEs are present.
TEST_F(IMEFeaturePodControllerTest, ButtonVisibilityIMECount) {
  SetUpButton();

  SetActiveIMECount(0);
  EXPECT_FALSE(button()->GetVisible());
  SetActiveIMECount(1);
  EXPECT_FALSE(button()->GetVisible());
  SetActiveIMECount(2);
  EXPECT_TRUE(button()->GetVisible());
}

TEST_F(IMEFeaturePodControllerTest, ButtonVisibilityImeMenuActive) {
  SetUpButton();
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);

  SetActiveIMECount(0);
  EXPECT_FALSE(button()->GetVisible());
  SetActiveIMECount(1);
  EXPECT_FALSE(button()->GetVisible());
  SetActiveIMECount(2);
  EXPECT_FALSE(button()->GetVisible());
}

TEST_F(IMEFeaturePodControllerTest, ButtonVisibilityPolicy) {
  SetUpButton();

  Shell::Get()->ime_controller()->SetImesManagedByPolicy(true);

  SetActiveIMECount(0);
  EXPECT_TRUE(button()->GetVisible());
  SetActiveIMECount(1);
  EXPECT_TRUE(button()->GetVisible());
  SetActiveIMECount(2);
  EXPECT_TRUE(button()->GetVisible());
}

TEST_F(IMEFeaturePodControllerTest, IconUMATracking) {
  SetUpButton();

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);

  // Show IME detailed view when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                      QsFeatureCatalogName::kIME,
                                      /*expected_count=*/1);
}

TEST_F(IMEFeaturePodControllerTest, LabelUMATracking) {
  SetUpButton();

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);

  // Show IME detailed view when pressing on the label.
  PressLabel();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                      QsFeatureCatalogName::kIME,
                                      /*expected_count=*/1);
}

}  // namespace ash
