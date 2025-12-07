// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/model/fake_power_status.h"
#include "ash/system/model/scoped_fake_power_status.h"
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
#include "ash/test/pixel/ash_pixel_test_helper.h"
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
  explicit StatusAreaPixelTest(bool enable_system_blur)
      : enable_system_blur_(enable_system_blur) {}
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.system_blur_enabled = enable_system_blur_;
    return init_params;
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    notification_test_api_ = std::make_unique<NotificationCenterTestApi>();
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
  const bool enable_system_blur_;
};

class StatusAreaParameterizedPixelTest
    : public StatusAreaPixelTest,
      public testing::WithParamInterface<
          std::tuple<ShelfAlignment,
                     bool /*IsTabletMode()*/,
                     bool /*IsRTL()*/,
                     bool /*IsActive()*/,
                     bool /*IsSystemBlurEnabled()*/>> {
 public:
  StatusAreaParameterizedPixelTest()
      : StatusAreaPixelTest(IsSystemBlurEnabled()) {}

  ShelfAlignment GetShelfAlignment() const { return std::get<0>(GetParam()); }
  bool IsTabletMode() const { return std::get<1>(GetParam()); }
  bool IsRTL() const { return std::get<2>(GetParam()); }
  bool IsActive() const { return std::get<3>(GetParam()); }
  bool IsSystemBlurEnabled() const { return std::get<4>(GetParam()); }

  std::string GenerateScreenshotName(const std::string& title) override {
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

    return pixel_test_helper()->GenerateScreenshotName(title +
                                                       screenshot_name_prefix);
  }
};

const ShelfAlignment kShelfAlignments[] = {
    ShelfAlignment::kBottom, ShelfAlignment::kLeft, ShelfAlignment::kRight};

INSTANTIATE_TEST_SUITE_P(
    All,
    StatusAreaParameterizedPixelTest,
    testing::Combine(testing::ValuesIn(kShelfAlignments),
                     testing::Bool() /*IsTabletMode()*/,
                     testing::Bool() /*IsRTL()*/,
                     testing::Bool() /*IsActive()*/,
                     testing::Bool() /*IsSystemBlurEnabled()*/));

TEST_P(StatusAreaParameterizedPixelTest, SystemTrayTest) {
  GetPrimaryShelf()->SetAlignment(GetShelfAlignment());
  ShellTestApi().SetTabletModeEnabledForTest(IsTabletMode());
  base::i18n::SetRTLForTesting(IsRTL());

  auto* system_tray = GetSystemTray();
  system_tray->SetIsActive(IsActive());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("system_tray"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 6 : 0,
      system_tray));
}

TEST_P(StatusAreaParameterizedPixelTest, DateTrayTest) {
  GetPrimaryShelf()->SetAlignment(GetShelfAlignment());
  ShellTestApi().SetTabletModeEnabledForTest(IsTabletMode());
  base::i18n::SetRTLForTesting(IsRTL());

  auto* date_tray = GetDateTray();
  date_tray->SetIsActive(IsActive());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("date_tray"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 6 : 0,
      date_tray));
}

// TODO(crbug.com/40934062): Disaled due to flakiness.
TEST_P(StatusAreaParameterizedPixelTest,
       DISABLED_NotificationTrayCounterWithSingleCount) {
  GetPrimaryShelf()->SetAlignment(GetShelfAlignment());
  ShellTestApi().SetTabletModeEnabledForTest(IsTabletMode());
  base::i18n::SetRTLForTesting(IsRTL());

  notification_test_api()->AddNotification();
  auto* notification_tray = notification_test_api()->GetTray();
  notification_tray->SetIsActive(IsActive());
  EXPECT_TRUE(notification_tray->GetVisible());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("notification_tray"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 1 : 0,
      notification_tray));
}

class StatusAreaParamerterizedAlignmentPixelTest
    : public StatusAreaPixelTest,
      public testing::WithParamInterface<
          std::tuple<ShelfAlignment, /*enable_system_blur=*/bool>> {
 public:
  StatusAreaParamerterizedAlignmentPixelTest()
      : StatusAreaPixelTest(EnableSystemBlur()) {}

  ShelfAlignment GetShelfAlignment() { return std::get<0>(GetParam()); }
  bool EnableSystemBlur() { return std::get<1>(GetParam()); }

  std::string GenerateScreenshotName(const std::string& title) override {
    return pixel_test_helper()->GenerateScreenshotName(
        title + GetNameForShelfAlignment(GetShelfAlignment()));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         StatusAreaParamerterizedAlignmentPixelTest,
                         testing::Combine(testing::ValuesIn(kShelfAlignments),
                                          testing::Bool()));

// TODO(crbug.com/40934073): Disabled due to excessive flakiness.
TEST_P(StatusAreaParamerterizedAlignmentPixelTest, DISABLED_OverflowTray) {
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
      GenerateScreenshotName("overflow_tray"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 0 : 0,
      overflow_tray));
}

class StatusAreaBatteryPixelTest
    : public StatusAreaPixelTest,
      public testing::WithParamInterface<
          std::tuple</*battery_badge_icon_enabled=*/bool,
                     /*enable_system_blur=*/bool>> {
 public:
  StatusAreaBatteryPixelTest() : StatusAreaPixelTest(EnableSystemBlur()) {}

  bool IsBatteryBadgeIconEnabled() { return std::get<0>(GetParam()); }
  bool EnableSystemBlur() { return std::get<1>(GetParam()); }

  FakePowerStatus* GetFakePowerStatus() {
    return scoped_fake_power_status_->fake_power_status();
  }

  PowerTrayView* power_tray_view() {
    return GetPrimaryUnifiedSystemTray()->power_tray_view_;
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(
        chromeos::features::kBatteryBadgeIcon, IsBatteryBadgeIconEnabled());
    scoped_fake_power_status_ = std::make_unique<ScopedFakePowerStatus>();
  }

  // AshTestBase:
  void TearDown() override {
    scoped_fake_power_status_.reset();
    scoped_feature_list_->Reset();
    AshTestBase::TearDown();
  }

  std::string GenerateScreenshotName(const std::string& title) override {
    return pixel_test_helper()->GenerateScreenshotName(
        title + (IsBatteryBadgeIconEnabled() ? "_new" : "_old"));
  }

 private:
  std::unique_ptr<ScopedFakePowerStatus> scoped_fake_power_status_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    StatusAreaBatteryPixelTest,
    testing::Combine(/*IsBatteryBadgeIconEnabled()=*/testing::Bool(),
                     /*EnableSystemBlur=*/testing::Bool()));

TEST_P(StatusAreaBatteryPixelTest, BoltIcon) {
  auto* fake_power_status = GetFakePowerStatus();
  fake_power_status->SetIsLinePowerConnected(true);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("bolt_icon"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 0 : 0,
      power_tray_view()));
}

TEST_P(StatusAreaBatteryPixelTest, UnreliableIcon) {
  auto* fake_power_status = GetFakePowerStatus();
  fake_power_status->SetIsUsbChargerConnected(true);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("unreliable_icon"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 0 : 0,
      power_tray_view()));
}

TEST_P(StatusAreaBatteryPixelTest, BatterySaverPlusIcon) {
  FakePowerStatus* fake_power_status = GetFakePowerStatus();
  fake_power_status->SetIsBatterySaverActive(true);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("battery_saver_plus_icon"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 0 : 0,
      power_tray_view()));
}

TEST_P(StatusAreaBatteryPixelTest, AlertIcon) {
  auto* fake_power_status = GetFakePowerStatus();
  fake_power_status->SetBatteryPercent(1);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("alert_icon"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 0 : 0,
      power_tray_view()));
}

}  // namespace ash
