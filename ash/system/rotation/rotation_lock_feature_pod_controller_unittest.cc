// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/rotation/rotation_lock_feature_pod_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/shell.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/display/display_switches.h"

namespace ash {

class RotationLockFeaturePodControllerTest : public AshTestBase {
 public:
  RotationLockFeaturePodControllerTest() = default;

  RotationLockFeaturePodControllerTest(
      const RotationLockFeaturePodControllerTest&) = delete;
  RotationLockFeaturePodControllerTest& operator=(
      const RotationLockFeaturePodControllerTest&) = delete;

  ~RotationLockFeaturePodControllerTest() override = default;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  void SetUpController();

  RotationLockFeaturePodController* controller() const {
    return controller_.get();
  }

  FeaturePodButton* button_view() const { return button_view_.get(); }

 private:
  std::unique_ptr<RotationLockFeaturePodController> controller_;
  std::unique_ptr<FeaturePodButton> button_view_;
};

void RotationLockFeaturePodControllerTest::SetUp() {
  // The Display used for testing is not an internal display. This flag
  // allows for DisplayManager to treat it as one.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kUseFirstDisplayAsInternal);
  AshTestBase::SetUp();
}

void RotationLockFeaturePodControllerTest::TearDown() {
  controller_.reset();
  button_view_.reset();
  AshTestBase::TearDown();
}

void RotationLockFeaturePodControllerTest::SetUpController() {
  controller_ = std::make_unique<RotationLockFeaturePodController>();
  button_view_.reset(controller_->CreateButton());
}

// Tests that when the button is initially created, that it is created
// not visible.
TEST_F(RotationLockFeaturePodControllerTest, CreateButton) {
  SetUpController();
  EXPECT_FALSE(button_view()->GetVisible());
}

// Tests that when the button is created, while TabletMode is active,
// that it is visible.
TEST_F(RotationLockFeaturePodControllerTest, CreateButtonDuringTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  SetUpController();
  EXPECT_TRUE(button_view()->GetVisible());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(button_view()->GetVisible());
}

// Tests that the enabling of TabletMode affects a previously created default
// view, changing the visibility.
TEST_F(RotationLockFeaturePodControllerTest,
       ButtonVisibilityChangesDuringTabletMode) {
  SetUpController();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(button_view()->GetVisible());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(button_view()->GetVisible());
}

TEST_F(RotationLockFeaturePodControllerTest, OnIconPressed) {
  SetUpController();
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  ScreenOrientationController* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  ASSERT_FALSE(screen_orientation_controller->rotation_locked());
  tablet_mode_controller->SetEnabledForTest(true);
  ASSERT_TRUE(button_view()->GetVisible());
  EXPECT_FALSE(button_view()->IsToggled());

  controller()->OnIconPressed();
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_TRUE(button_view()->GetVisible());
  EXPECT_TRUE(button_view()->IsToggled());

  controller()->OnIconPressed();
  EXPECT_FALSE(screen_orientation_controller->rotation_locked());
  EXPECT_TRUE(button_view()->GetVisible());
  EXPECT_FALSE(button_view()->IsToggled());

  tablet_mode_controller->SetEnabledForTest(false);
}

TEST_F(RotationLockFeaturePodControllerTest, IconUMATracking) {
  SetUpController();

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

  // Turn on rotation lock when pressing on the icon.
  controller()->OnIconPressed();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      QsFeatureCatalogName::kRotationLock,
      /*expected_count=*/1);

  // Turn off rotation lock when pressing on the icon.
  controller()->OnIconPressed();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      QsFeatureCatalogName::kRotationLock,
      /*expected_count=*/1);
}

TEST_F(RotationLockFeaturePodControllerTest, LabelUMATracking) {
  SetUpController();

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

  // Turn on rotation lock when pressing on the label.
  controller()->OnLabelPressed();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      QsFeatureCatalogName::kRotationLock,
      /*expected_count=*/1);

  // Turn off rotation lock when pressing on the label.
  controller()->OnIconPressed();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      QsFeatureCatalogName::kRotationLock,
      /*expected_count=*/1);
}

}  // namespace ash
