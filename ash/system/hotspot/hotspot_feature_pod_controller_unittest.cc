// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_feature_pod_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/hotspot/hotspot_detailed_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr char kToggledOnHistogram[] = "Ash.QuickSettings.FeaturePod.ToggledOn";
constexpr char kToggledOffHistogram[] =
    "Ash.QuickSettings.FeaturePod.ToggledOff";
constexpr char kDiveInHistogram[] = "Ash.QuickSettings.FeaturePod.DiveIn";

}  // namespace

using hotspot_config::mojom::HotspotAllowStatus;
using hotspot_config::mojom::HotspotInfo;
using hotspot_config::mojom::HotspotState;

class HotspotFeaturePodControllerTest : public AshTestBase {
 public:
  HotspotFeaturePodControllerTest()
      : AshTestBase(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}
  ~HotspotFeaturePodControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Spin the runloop to have HotspotInfoCache finish querying the hotspot
    // info.
    base::RunLoop().RunUntilIdle();

    GetPrimaryUnifiedSystemTray()->ShowBubble();
    CreateHotspotFeatureTile();
  }

  void TearDown() override {
    hotspot_feature_tile_.reset();
    hotspot_feature_pod_controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateHotspotFeatureTile() {
    CHECK(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
    hotspot_feature_pod_controller_ =
        std::make_unique<HotspotFeaturePodController>(
            GetPrimaryUnifiedSystemTray()
                ->bubble()
                ->unified_system_tray_controller());
    hotspot_feature_tile_ = hotspot_feature_pod_controller_->CreateTile();
  }

  void UpdateHotspotInfo(HotspotState state,
                         HotspotAllowStatus allow_status,
                         uint32_t client_count = 0) {
    auto hotspot_info = HotspotInfo::New();
    hotspot_info->state = state;
    hotspot_info->allow_status = allow_status;
    hotspot_info->client_count = client_count;
    ash_test_helper()->cros_hotspot_config_test_helper()->SetFakeHotspotInfo(
        std::move(hotspot_info));
    // Spin the runloop to observe the hotspot info change.
    base::RunLoop().RunUntilIdle();
  }

  void EnableAndDisableHotspotOnce() {
    UpdateHotspotInfo(HotspotState::kEnabled, HotspotAllowStatus::kAllowed);
    UpdateHotspotInfo(HotspotState::kDisabled, HotspotAllowStatus::kAllowed);

    EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  }

  void PressIcon() {
    hotspot_feature_pod_controller_->OnIconPressed();
    base::RunLoop().RunUntilIdle();
  }

  void PressLabel() {
    hotspot_feature_pod_controller_->OnLabelPressed();
    base::RunLoop().RunUntilIdle();
  }

  void LockScreen() {
    GetSessionControllerClient()->LockScreen();
    base::RunLoop().RunUntilIdle();
  }

  const char* GetVectorIconName() {
    return hotspot_feature_tile_->vector_icon_->name;
  }

  void ExpectHotspotDetailedViewShown() {
    TrayDetailedView* detailed_view =
        GetPrimaryUnifiedSystemTray()
            ->bubble()
            ->quick_settings_view()
            ->GetDetailedViewForTest<TrayDetailedView>();
    ASSERT_TRUE(detailed_view);
    EXPECT_TRUE(views::IsViewClass<HotspotDetailedView>(detailed_view));
  }

 protected:
  std::unique_ptr<HotspotFeaturePodController> hotspot_feature_pod_controller_;
  std::unique_ptr<FeatureTile> hotspot_feature_tile_;
};

TEST_F(HotspotFeaturePodControllerTest, HotspotNotUsedBefore) {
  EXPECT_FALSE(hotspot_feature_tile_->GetVisible());
}

TEST_F(HotspotFeaturePodControllerTest, PressLabelWhenHotspotEnabled) {
  EnableAndDisableHotspotOnce();
  UpdateHotspotInfo(HotspotState::kEnabled, HotspotAllowStatus::kAllowed);

  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  EXPECT_TRUE(hotspot_feature_tile_->IsToggled());
  EXPECT_EQ(u"Hotspot", hotspot_feature_tile_->label()->GetText());
  EXPECT_EQ(u"On", hotspot_feature_tile_->sub_label()->GetText());
  EXPECT_EQ(u"Toggle hotspot. Hotspot is on, no device connected.",
            hotspot_feature_tile_->icon_button()->GetTooltipText());
  EXPECT_EQ(u"Show hotspot details. Hotspot is on.",
            hotspot_feature_tile_->GetTooltipText());
  UpdateHotspotInfo(HotspotState::kEnabled, HotspotAllowStatus::kAllowed, 2);
  EXPECT_EQ(u"Toggle hotspot. Hotspot is on, 2 devices connected.",
            hotspot_feature_tile_->icon_button()->GetTooltipText());
  EXPECT_STREQ(kHotspotOnIcon.name, GetVectorIconName());

  // Press on the label should navigate to the detailed page without toggle
  // hotspot.
  PressLabel();
  ExpectHotspotDetailedViewShown();
  EXPECT_TRUE(hotspot_feature_tile_->IsToggled());
}

TEST_F(HotspotFeaturePodControllerTest, PressIconWhenHotspotEnabled) {
  EnableAndDisableHotspotOnce();
  UpdateHotspotInfo(HotspotState::kEnabled, HotspotAllowStatus::kAllowed);

  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  EXPECT_TRUE(hotspot_feature_tile_->IsToggled());
  EXPECT_EQ(u"Hotspot", hotspot_feature_tile_->label()->GetText());
  EXPECT_EQ(u"On", hotspot_feature_tile_->sub_label()->GetText());
  EXPECT_EQ(u"Toggle hotspot. Hotspot is on, no device connected.",
            hotspot_feature_tile_->icon_button()->GetTooltipText());
  EXPECT_EQ(u"Show hotspot details. Hotspot is on.",
            hotspot_feature_tile_->GetTooltipText());
  EXPECT_STREQ(kHotspotOnIcon.name, GetVectorIconName());

  // Press on the icon should toggle hotspot.
  PressIcon();
  EXPECT_FALSE(hotspot_feature_tile_->IsToggled());
  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  EXPECT_EQ(u"Off", hotspot_feature_tile_->sub_label()->GetText());
}

TEST_F(HotspotFeaturePodControllerTest, HotspotEnabling) {
  EnableAndDisableHotspotOnce();
  UpdateHotspotInfo(HotspotState::kEnabling, HotspotAllowStatus::kAllowed);

  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  EXPECT_TRUE(hotspot_feature_tile_->IsToggled());
  EXPECT_EQ(u"Hotspot", hotspot_feature_tile_->label()->GetText());
  EXPECT_EQ(u"Turning on…", hotspot_feature_tile_->sub_label()->GetText());
  EXPECT_EQ(u"Show hotspot details. Hotspot is turning on.",
            hotspot_feature_tile_->icon_button()->GetTooltipText());
  EXPECT_EQ(u"Show hotspot details. Hotspot is turning on.",
            hotspot_feature_tile_->GetTooltipText());
  EXPECT_STREQ(kHotspotDotIcon.name, GetVectorIconName());
  // Verifies the hotspot icon is animating when enabling.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_STREQ(kHotspotOneArcIcon.name, GetVectorIconName());
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_STREQ(kHotspotOnIcon.name, GetVectorIconName());

  // Press on the icon should navigate to the detailed page but not to toggle
  // hotspot.
  PressIcon();
  EXPECT_TRUE(hotspot_feature_tile_->IsToggled());
  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  ExpectHotspotDetailedViewShown();
}

TEST_F(HotspotFeaturePodControllerTest, HotspotDisabling) {
  EnableAndDisableHotspotOnce();
  UpdateHotspotInfo(HotspotState::kDisabling, HotspotAllowStatus::kAllowed);

  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  EXPECT_TRUE(hotspot_feature_tile_->IsToggled());
  EXPECT_EQ(u"Hotspot", hotspot_feature_tile_->label()->GetText());
  EXPECT_EQ(u"Turning off…", hotspot_feature_tile_->sub_label()->GetText());
  EXPECT_EQ(u"Show hotspot details. Hotspot is turning off.",
            hotspot_feature_tile_->icon_button()->GetTooltipText());
  EXPECT_EQ(u"Show hotspot details. Hotspot is turning off.",
            hotspot_feature_tile_->GetTooltipText());
  EXPECT_STREQ(kHotspotOffIcon.name, GetVectorIconName());

  // Press on the icon should navigate to the detailed page but not to toggle
  // hotspot.
  PressIcon();
  EXPECT_TRUE(hotspot_feature_tile_->IsToggled());
  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  ExpectHotspotDetailedViewShown();
}

TEST_F(HotspotFeaturePodControllerTest,
       PressIconWhenHotspotDisabledAndAllowEnable) {
  EnableAndDisableHotspotOnce();
  UpdateHotspotInfo(HotspotState::kDisabled, HotspotAllowStatus::kAllowed);

  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  EXPECT_FALSE(hotspot_feature_tile_->IsToggled());
  EXPECT_EQ(u"Hotspot", hotspot_feature_tile_->label()->GetText());
  EXPECT_EQ(u"Off", hotspot_feature_tile_->sub_label()->GetText());
  EXPECT_EQ(u"Toggle hotspot. Hotspot is off.",
            hotspot_feature_tile_->icon_button()->GetTooltipText());
  EXPECT_EQ(u"Toggle hotspot. Hotspot is off.",
            hotspot_feature_tile_->GetTooltipText());
  EXPECT_STREQ(kHotspotOffIcon.name, GetVectorIconName());

  // Press on the icon should toggle hotspot and navigate to the detailed page.
  PressIcon();
  EXPECT_TRUE(hotspot_feature_tile_->IsToggled());
  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  ExpectHotspotDetailedViewShown();
}

TEST_F(HotspotFeaturePodControllerTest,
       PressLabelWhenHotspotDisabledAndAllowEnable) {
  EnableAndDisableHotspotOnce();
  UpdateHotspotInfo(HotspotState::kDisabled, HotspotAllowStatus::kAllowed);

  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  EXPECT_FALSE(hotspot_feature_tile_->IsToggled());
  EXPECT_EQ(u"Hotspot", hotspot_feature_tile_->label()->GetText());
  EXPECT_EQ(u"Off", hotspot_feature_tile_->sub_label()->GetText());
  EXPECT_EQ(u"Toggle hotspot. Hotspot is off.",
            hotspot_feature_tile_->icon_button()->GetTooltipText());
  EXPECT_EQ(u"Toggle hotspot. Hotspot is off.",
            hotspot_feature_tile_->GetTooltipText());
  EXPECT_STREQ(kHotspotOffIcon.name, GetVectorIconName());

  // Press on the drive in label should navigate to the detailed page without
  // toggling hotspot.
  UpdateHotspotInfo(HotspotState::kDisabled, HotspotAllowStatus::kAllowed);
  PressLabel();
  EXPECT_FALSE(hotspot_feature_tile_->IsToggled());
  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  ExpectHotspotDetailedViewShown();
}

TEST_F(HotspotFeaturePodControllerTest, HotspotDisabledNoMobileNetwork) {
  EnableAndDisableHotspotOnce();
  UpdateHotspotInfo(HotspotState::kDisabled,
                    HotspotAllowStatus::kDisallowedNoMobileData);

  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  EXPECT_FALSE(hotspot_feature_tile_->IsToggled());
  EXPECT_EQ(u"Hotspot", hotspot_feature_tile_->label()->GetText());
  EXPECT_EQ(u"Off", hotspot_feature_tile_->sub_label()->GetText());
  EXPECT_EQ(u"Show hotspot details. Connect to mobile network to use hotspot.",
            hotspot_feature_tile_->icon_button()->GetTooltipText());
  EXPECT_EQ(u"Show hotspot details. Hotspot is off.",
            hotspot_feature_tile_->GetTooltipText());
  EXPECT_STREQ(kHotspotOffIcon.name, GetVectorIconName());

  // Press on the icon should navigate to the detailed page but not to toggle
  // hotspot.
  PressIcon();
  EXPECT_FALSE(hotspot_feature_tile_->IsToggled());
  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  ExpectHotspotDetailedViewShown();
}

TEST_F(HotspotFeaturePodControllerTest,
       HotspotDisabledMobileNetworkNotSupported) {
  EnableAndDisableHotspotOnce();
  UpdateHotspotInfo(HotspotState::kDisabled,
                    HotspotAllowStatus::kDisallowedReadinessCheckFail);

  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  EXPECT_FALSE(hotspot_feature_tile_->IsToggled());
  EXPECT_EQ(u"Hotspot", hotspot_feature_tile_->label()->GetText());
  EXPECT_EQ(u"Off", hotspot_feature_tile_->sub_label()->GetText());
  EXPECT_EQ(
      u"Show hotspot details. Your mobile network doesn't support hotspot.",
      hotspot_feature_tile_->icon_button()->GetTooltipText());
  EXPECT_EQ(u"Show hotspot details. Hotspot is off.",
            hotspot_feature_tile_->GetTooltipText());
  EXPECT_STREQ(kHotspotOffIcon.name, GetVectorIconName());

  // Press on the icon should navigate to the detailed page but not to toggle
  // hotspot.
  PressIcon();
  EXPECT_FALSE(hotspot_feature_tile_->IsToggled());
  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  ExpectHotspotDetailedViewShown();
}

TEST_F(HotspotFeaturePodControllerTest, HotspotDisabledBlockedByPolicy) {
  EnableAndDisableHotspotOnce();
  UpdateHotspotInfo(HotspotState::kDisabled,
                    HotspotAllowStatus::kDisallowedByPolicy);

  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  EXPECT_FALSE(hotspot_feature_tile_->IsToggled());
  EXPECT_EQ(u"Hotspot", hotspot_feature_tile_->label()->GetText());
  EXPECT_EQ(u"Off", hotspot_feature_tile_->sub_label()->GetText());
  EXPECT_EQ(u"Show hotspot details. Hotspot is blocked by your administrator.",
            hotspot_feature_tile_->icon_button()->GetTooltipText());
  EXPECT_EQ(u"Show hotspot details. Hotspot is off.",
            hotspot_feature_tile_->GetTooltipText());
  EXPECT_STREQ(kHotspotOffIcon.name, GetVectorIconName());

  // Press on the icon should navigate to the detailed page but not to toggle
  // hotspot.
  PressIcon();
  EXPECT_FALSE(hotspot_feature_tile_->IsToggled());
  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  ExpectHotspotDetailedViewShown();
}

TEST_F(HotspotFeaturePodControllerTest, LockScreen) {
  EnableAndDisableHotspotOnce();
  LockScreen();

  // Locking the screen closes the system tray bubble thus destroying the
  // hotspot feature tile, so re-show the bubble and recreate the tile.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  CreateHotspotFeatureTile();
  UpdateHotspotInfo(HotspotState::kDisabled, HotspotAllowStatus::kAllowed);

  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  EXPECT_FALSE(hotspot_feature_tile_->IsToggled());
  EXPECT_EQ(u"Hotspot", hotspot_feature_tile_->label()->GetText());
  EXPECT_EQ(u"Off", hotspot_feature_tile_->sub_label()->GetText());
  EXPECT_EQ(u"Toggle hotspot. Hotspot is off.",
            hotspot_feature_tile_->icon_button()->GetTooltipText());
  EXPECT_EQ(u"Toggle hotspot. Hotspot is off.",
            hotspot_feature_tile_->GetTooltipText());
  EXPECT_STREQ(kHotspotOffIcon.name, GetVectorIconName());

  // Press on the icon should toggle hotspot and navigate to the detailed page.
  PressIcon();
  EXPECT_TRUE(hotspot_feature_tile_->IsToggled());
  EXPECT_TRUE(hotspot_feature_tile_->GetVisible());
  EXPECT_TRUE(hotspot_feature_tile_->GetEnabled());
  ExpectHotspotDetailedViewShown();
}

TEST_F(HotspotFeaturePodControllerTest, LabelUMATracking) {
  UpdateHotspotInfo(HotspotState::kDisabled, HotspotAllowStatus::kAllowed);

  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(kToggledOnHistogram,
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(kToggledOffHistogram,
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(kDiveInHistogram,
                                     /*expected_count=*/0);

  // Press on the label to show detailed page.
  PressLabel();
  histogram_tester->ExpectTotalCount(kToggledOnHistogram,
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount(kToggledOnHistogram,
                                      QsFeatureCatalogName::kHotspot,
                                      /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(kToggledOffHistogram,
                                     /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(kDiveInHistogram,
                                     /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(kDiveInHistogram,
                                      QsFeatureCatalogName::kHotspot,
                                      /*expected_count=*/1);

  // Press on the icon to toggle hotspot and show detailed page.
  PressIcon();
  histogram_tester->ExpectTotalCount(kToggledOnHistogram,
                                     /*expected_count=*/1);
  histogram_tester->ExpectBucketCount(kToggledOnHistogram,
                                      QsFeatureCatalogName::kHotspot,
                                      /*expected_count=*/1);
  histogram_tester->ExpectTotalCount(kToggledOffHistogram,
                                     /*expected_count=*/0);
  histogram_tester->ExpectBucketCount(kToggledOffHistogram,
                                      QsFeatureCatalogName::kHotspot,
                                      /*expected_count=*/0);
  histogram_tester->ExpectTotalCount(kDiveInHistogram,
                                     /*expected_count=*/2);
  histogram_tester->ExpectBucketCount(kDiveInHistogram,
                                      QsFeatureCatalogName::kHotspot,
                                      /*expected_count=*/2);
}

}  // namespace ash
