// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_helper.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

namespace ash {

namespace {

const std::u16string kPhoneName = u"Fake Phone Name";
const ShelfAlignment kShelfAlignments[] = {
    ShelfAlignment::kLeft, ShelfAlignment::kBottom, ShelfAlignment::kRight};
const phonehub::FeatureStatus kFeatureStatuses[] = {
    phonehub::FeatureStatus::kEnabledAndConnecting,
    phonehub::FeatureStatus::kEnabledAndConnected,
    phonehub::FeatureStatus::kEnabledButDisconnected,
    phonehub::FeatureStatus::kUnavailableBluetoothOff};

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

const std::string GetNameForFeatureStatus(phonehub::FeatureStatus status) {
  switch (status) {
    case phonehub::FeatureStatus::kEnabledAndConnecting:
      return "_enabled_and_connecting";
    case phonehub::FeatureStatus::kEnabledAndConnected:
      return "_enabled_and_connected";
    case phonehub::FeatureStatus::kEnabledButDisconnected:
      return "_enabled_and_disconnected";
    case phonehub::FeatureStatus::kUnavailableBluetoothOff:
      return "_bluetooth_off";
    default:
      return "_other";
  }
}

}  // namespace

// Pixel tests for ChromeOS Phone Hub. This relates to the Phone Hub tray icon
// on the shelf and the Phone Hub bubble opened upon clicking that icon.
class PhoneHubPixelTest : public AshTestBase {
 public:
  explicit PhoneHubPixelTest(bool enable_system_blur)
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

    // TODO(crbug.com/381281980): Eche is disabled due to an animation crashing
    // despite the permission dialog not being completed. Update these tests to
    // support Eche and different permission states.
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatures(
        /* enabled_features= */ {},
        /* disabled_features= */ {features::kEcheSWA});

    phone_hub_tray_ =
        GetPrimaryShelf()->GetStatusAreaWidget()->phone_hub_tray();
    phone_hub_tray_->SetPhoneHubManager(&phone_hub_manager_);

    // Disable animations to prevent screenshot mismatch.
    gfx::ScopedAnimationDurationScaleMode duration_mode(
        gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  // AshTestBase:
  void TearDown() override {
    phone_hub_tray_ = nullptr;
    scoped_feature_list_->Reset();
    AshTestBase::TearDown();
  }

  PhoneHubTray* GetPhoneHubTray() { return phone_hub_tray_; }

  phonehub::FakeFeatureStatusProvider* GetFeatureStatusProvider() {
    return phone_hub_manager_.fake_feature_status_provider();
  }

  void SetFakePhoneStatusModel() {
    phone_hub_manager_.mutable_phone_model()->SetPhoneStatusModel(
        phonehub::CreateFakePhoneStatusModel());
    phone_hub_manager_.mutable_phone_model()->SetPhoneName(kPhoneName);
  }

 protected:
  const bool enable_system_blur_;
  raw_ptr<PhoneHubTray> phone_hub_tray_ = nullptr;
  phonehub::FakePhoneHubManager phone_hub_manager_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

// Parameterized test class to set up Pixel Tests when the shelf alignment is
// changed and different Feature States for Phone Hub.
class PhoneHubParameterizedPixelTest
    : public PhoneHubPixelTest,
      public testing::WithParamInterface<
          std::tuple<ShelfAlignment,
                     phonehub::FeatureStatus,
                     /*enable_system_blur=*/bool>> {
 public:
  PhoneHubParameterizedPixelTest() : PhoneHubPixelTest(IsSystemBlurEnabled()) {}

  std::string GenerateScreenshotName(const std::string& title) override {
    return pixel_test_helper()->GenerateScreenshotName(
        title + GetNameForShelfAlignment(GetShelfAlignment()) +
        GetNameForFeatureStatus(GetFeatureStatus()));
  }

  ShelfAlignment GetShelfAlignment() const { return std::get<0>(GetParam()); }
  phonehub::FeatureStatus GetFeatureStatus() const {
    return std::get<1>(GetParam());
  }
  bool IsSystemBlurEnabled() const { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PhoneHubParameterizedPixelTest,
                         testing::Combine(testing::ValuesIn(kShelfAlignments),
                                          testing::ValuesIn(kFeatureStatuses),
                                          testing::Bool()));

TEST_P(PhoneHubParameterizedPixelTest, PhoneHubTrayOnShelf) {
  GetPrimaryShelf()->SetAlignment(GetShelfAlignment());
  GetFeatureStatusProvider()->SetStatus(GetFeatureStatus());

  GetPhoneHubTray()->SetIsActive(true);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("phone_hub_tray"),
      /* revision_number= */ pixel_test_helper()->IsSystemBlurEnabled() ? 0 : 0,
      GetPhoneHubTray()));
}

// TODO(b:383384907) disable animations before enabling this test.
TEST_P(PhoneHubParameterizedPixelTest,
       DISABLED_PhoneHubBubbleOpenedNoPermissions) {
  GetPrimaryShelf()->SetAlignment(GetShelfAlignment());
  if (GetFeatureStatus() == phonehub::FeatureStatus::kEnabledAndConnected) {
    SetFakePhoneStatusModel();
  }
  GetFeatureStatusProvider()->SetStatus(GetFeatureStatus());

  auto* phone_hub_tray = GetPhoneHubTray();
  phone_hub_tray->SetIsActive(true);
  LeftClickOn(phone_hub_tray);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("phone_hub_bubble"),
      /* revision_number= */ pixel_test_helper()->IsSystemBlurEnabled() ? 1 : 0,
      phone_hub_tray, phone_hub_tray->GetBubbleView()));
}

}  // namespace ash
