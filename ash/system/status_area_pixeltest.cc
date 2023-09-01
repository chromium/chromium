// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/status_area_overflow_button_tray.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/i18n/rtl.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

const std::string GetNameForShelfAlignment(ShelfAlignment alignment) {
  switch (alignment) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return "_bottom_shelf";
    case ash::ShelfAlignment::kLeft:
      return "_left_shelf";
    case ash::ShelfAlignment::kRight:
      return "_right_shelf";
  }
}

}  // namespace

// Pixel tests for Chrome OS Status Area. This relates to all tray buttons in
// the bottom right corner.
class StatusAreaPixelTest : public AshTestBase {
 public:
  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kQsRevamp, chromeos::features::kJelly}, {});

    AshTestBase::SetUp();

    // The `NotificationCenterTray` does not exist until the `QsRevamp` feature
    // is enabled.
    notification_test_api_ = std::make_unique<NotificationCenterTestApi>(
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()
            ->notification_center_tray());
  }

  TrayBackgroundView* GetSystemTray() {
    return GetPrimaryShelf()->GetStatusAreaWidget()->unified_system_tray();
  }

  TrayBackgroundView* GetDateTray() {
    return GetPrimaryShelf()->GetStatusAreaWidget()->date_tray();
  }

  NotificationCenterTestApi* notification_test_api() {
    return notification_test_api_.get();
  }

 private:
  std::unique_ptr<NotificationCenterTestApi> notification_test_api_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class StatusAreaParameterizedPixelTest
    : public StatusAreaPixelTest,
      public testing::WithParamInterface<std::tuple<ShelfAlignment,
                                                    bool /*IsTabletMode()*/,
                                                    bool /*IsRTL()*/,
                                                    bool /*IsActive()*/>> {
 public:
  ShelfAlignment GetShelfAlignment() { return std::get<0>(GetParam()); }
  bool IsTabletMode() { return std::get<1>(GetParam()); }
  bool IsRTL() { return std::get<2>(GetParam()); }
  bool IsActive() { return std::get<3>(GetParam()); }

  const std::string GetScreenshotNameSuffix() {
    std::string screenshot_name_prefix =
        GetNameForShelfAlignment(GetShelfAlignment());

    if (IsTabletMode()) {
      screenshot_name_prefix += "_tablet_mode";
    }

    if (IsRTL()) {
      screenshot_name_prefix += "_rtl";
    }

    if (IsActive()) {
      screenshot_name_prefix += "_active";
    }

    return screenshot_name_prefix;
  }
};

const ShelfAlignment kShelfAlignments[] = {
    ShelfAlignment::kBottom, ShelfAlignment::kLeft, ShelfAlignment::kRight};

INSTANTIATE_TEST_SUITE_P(All,
                         StatusAreaParameterizedPixelTest,
                         testing::Combine(testing::ValuesIn(kShelfAlignments),
                                          testing::Bool() /*IsTabletMode()*/,
                                          testing::Bool() /*IsRTL()*/,
                                          testing::Bool() /*IsActive()*/));

TEST_P(StatusAreaParameterizedPixelTest, SystemTrayTest) {
  GetPrimaryShelf()->SetAlignment(GetShelfAlignment());
  ShellTestApi().SetTabletModeEnabledForTest(IsTabletMode());
  base::i18n::SetRTLForTesting(IsRTL());

  auto* system_tray = GetSystemTray();
  system_tray->SetIsActive(IsActive());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "system_tray" + GetScreenshotNameSuffix(), /*revision_number=*/0,
      system_tray));
}

TEST_P(StatusAreaParameterizedPixelTest, DateTrayTest) {
  GetPrimaryShelf()->SetAlignment(GetShelfAlignment());
  ShellTestApi().SetTabletModeEnabledForTest(IsTabletMode());
  base::i18n::SetRTLForTesting(IsRTL());

  auto* date_tray = GetDateTray();
  date_tray->SetIsActive(IsActive());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "date_tray" + GetScreenshotNameSuffix(), /*revision_number=*/0,
      date_tray));
}

TEST_P(StatusAreaParameterizedPixelTest,
       NotificationTrayCounterWithSingleCount) {
  GetPrimaryShelf()->SetAlignment(GetShelfAlignment());
  ShellTestApi().SetTabletModeEnabledForTest(IsTabletMode());
  base::i18n::SetRTLForTesting(IsRTL());

  notification_test_api()->AddNotification();
  auto* notification_tray = notification_test_api()->GetTray();
  notification_tray->SetIsActive(IsActive());
  EXPECT_TRUE(notification_tray->GetVisible());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "notification_tray" + GetScreenshotNameSuffix(), /*revision_number=*/0,
      notification_tray));
}

class StatusAreaParamerterizedAlignmentPixelTest
    : public StatusAreaPixelTest,
      public testing::WithParamInterface<ShelfAlignment> {
 public:
  ShelfAlignment GetShelfAlignment() { return GetParam(); }

  const std::string GetScreenshotNameSuffix() {
    return GetNameForShelfAlignment(GetShelfAlignment());
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         StatusAreaParamerterizedAlignmentPixelTest,
                         testing::ValuesIn(kShelfAlignments));

TEST_P(StatusAreaParamerterizedAlignmentPixelTest, OverflowTray) {
  UpdateDisplay("500x600");
  GetPrimaryShelf()->SetAlignment(GetShelfAlignment());
  ShellTestApi().SetTabletModeEnabledForTest(true);
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  notification_test_api()->AddNotification();
  auto* status_area_widget = GetPrimaryShelf()->GetStatusAreaWidget();
  status_area_widget->media_tray()->SetVisiblePreferred(true);
  status_area_widget->eche_tray()->SetVisiblePreferred(true);

  auto* overflow_tray = status_area_widget->overflow_button_tray();

  ASSERT_TRUE(overflow_tray->GetVisible());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "overflow_tray" + GetScreenshotNameSuffix(),
      /*revision_number=*/0, overflow_tray));
}

}  // namespace ash
