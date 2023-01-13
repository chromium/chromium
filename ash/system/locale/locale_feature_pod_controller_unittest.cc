// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/locale_feature_pod_controller.h"

#include <memory>
#include <vector>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/locale_update_controller.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Tests are parameterized by feature QsRevamp.
class LocaleFeaturePodControllerTest
    : public NoSessionAshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  LocaleFeaturePodControllerTest() {
    if (IsQsRevampEnabled()) {
      feature_list_.InitAndEnableFeature(features::kQsRevamp);
    } else {
      feature_list_.InitAndDisableFeature(features::kQsRevamp);
    }
  }

  LocaleFeaturePodControllerTest(const LocaleFeaturePodControllerTest&) =
      delete;
  LocaleFeaturePodControllerTest& operator=(
      const LocaleFeaturePodControllerTest&) = delete;

  ~LocaleFeaturePodControllerTest() override = default;

  bool IsQsRevampEnabled() const { return GetParam(); }

  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  void TearDown() override {
    tile_.reset();
    button_.reset();
    controller_.reset();
    NoSessionAshTestBase::TearDown();
  }

  void SetUpButton() {
    controller_ =
        std::make_unique<LocaleFeaturePodController>(tray_controller());
    if (IsQsRevampEnabled()) {
      tile_ = controller_->CreateTile();
    } else {
      button_ = base::WrapUnique(controller_->CreateButton());
    }
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  bool IsButtonVisible() {
    return IsQsRevampEnabled() ? tile_->GetVisible() : button_->GetVisible();
  }

  const char* GetToggledOnHistogramName() {
    return IsQsRevampEnabled() ? "Ash.QuickSettings.FeaturePod.ToggledOn"
                               : "Ash.UnifiedSystemView.FeaturePod.ToggledOn";
  }

  const char* GetToggledOffHistogramName() {
    return IsQsRevampEnabled() ? "Ash.QuickSettings.FeaturePod.ToggledOff"
                               : "Ash.UnifiedSystemView.FeaturePod.ToggledOff";
  }

  const char* GetDiveInHistogramName() {
    return IsQsRevampEnabled() ? "Ash.QuickSettings.FeaturePod.DiveIn"
                               : "Ash.UnifiedSystemView.FeaturePod.DiveIn";
  }

  void PressIcon() { controller_->OnIconPressed(); }

  void PressLabel() { controller_->OnLabelPressed(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<LocaleFeaturePodController> controller_;
  std::unique_ptr<FeaturePodButton> button_;
  std::unique_ptr<FeatureTile> tile_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         LocaleFeaturePodControllerTest,
                         testing::Bool());

TEST_P(LocaleFeaturePodControllerTest, ButtonVisibility) {
  constexpr char kDefaultLocaleIsoCode[] = "en-US";
  // The button is invisible if the locale list is unset.
  SetUpButton();
  EXPECT_FALSE(IsButtonVisible());

  // The button is invisible if the locale list is empty.
  Shell::Get()->system_tray_model()->SetLocaleList({}, kDefaultLocaleIsoCode);
  SetUpButton();
  EXPECT_FALSE(IsButtonVisible());

  // The button is visible if the locale list is non-empty.
  std::vector<LocaleInfo> locale_list;
  locale_list.emplace_back(kDefaultLocaleIsoCode, u"English (United States)");
  Shell::Get()->system_tray_model()->SetLocaleList(std::move(locale_list),
                                                   kDefaultLocaleIsoCode);
  SetUpButton();
  EXPECT_TRUE(IsButtonVisible());
}

TEST_P(LocaleFeaturePodControllerTest, IconUMATracking) {
  std::vector<LocaleInfo> locale_list;
  constexpr char kDefaultLocaleIsoCode[] = "en-US";
  locale_list.emplace_back(kDefaultLocaleIsoCode, u"English (United States)");
  Shell::Get()->system_tray_model()->SetLocaleList(std::move(locale_list),
                                                   kDefaultLocaleIsoCode);
  SetUpButton();

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/0);

  // Show Locale detailed view when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(GetDiveInHistogramName(),
                                      QsFeatureCatalogName::kLocale,
                                      /*expected_count=*/1);
}

TEST_P(LocaleFeaturePodControllerTest, LabelUMATracking) {
  SetUpButton();

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/0);

  // Show Locale detailed view when pressing on the label.
  PressLabel();
  histogram_tester->ExpectTotalCount(GetToggledOnHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetToggledOffHistogramName(),
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(GetDiveInHistogramName(),
                                     /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(GetDiveInHistogramName(),
                                      QsFeatureCatalogName::kLocale,
                                      /*expected_count=*/1);
}

}  // namespace
}  // namespace ash
