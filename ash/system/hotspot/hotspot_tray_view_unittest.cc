// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_tray_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

bool AreImagesEqual(const gfx::ImageSkia& image,
                    const gfx::ImageSkia& reference) {
  return gfx::test::AreBitmapsEqual(*image.bitmap(), *reference.bitmap());
}

}  // namespace

class HotspotTrayViewTest : public NoSessionAshTestBase,
                            public testing::WithParamInterface<bool> {
 public:
  HotspotTrayViewTest() = default;
  ~HotspotTrayViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    if (IsJellyEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kHotspot, chromeos::features::kJelly}, {});
    } else {
      scoped_feature_list_.InitAndEnableFeature(features::kHotspot);
    }
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();
    network_handler_test_helper_->ResetDevicesAndServices();
    cros_hotspot_config_test_helper_ =
        std::make_unique<hotspot_config::CrosHotspotConfigTestHelper>();
    std::unique_ptr<HotspotTrayView> hotspot_tray_view =
        std::make_unique<HotspotTrayView>(GetPrimaryShelf());
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    hotspot_tray_view_ = widget_->SetContentsView(std::move(hotspot_tray_view));
    LogIn();

    // Spin the runloop to sync up the latest hotspot info.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    widget_.reset();
    cros_hotspot_config_test_helper_.reset();
    network_handler_test_helper_.reset();
    AshTestBase::TearDown();
  }

  void LogIn() { SimulateUserLogin("user1@test.com"); }

  void SetHotspotStateAndClientCountInShill(const std::string& state,
                                            size_t client_count) {
    base::Value::Dict status_dict;
    status_dict.Set(shill::kTetheringStatusStateProperty, state);
    base::Value::List active_clients_list;
    for (size_t i = 0; i < client_count; i++) {
      base::Value::Dict client;
      client.Set(shill::kTetheringStatusClientIPv4Property, "IPV4");
      client.Set(shill::kTetheringStatusClientHostnameProperty, "hostname");
      client.Set(shill::kTetheringStatusClientMACProperty, "persist");
      active_clients_list.Append(std::move(client));
    }
    if (state == shill::kTetheringStateActive) {
      status_dict.Set(shill::kTetheringStatusClientsProperty,
                      std::move(active_clients_list));
    }
    network_handler_test_helper_->manager_test()->SetManagerProperty(
        shill::kTetheringStatusProperty, base::Value(std::move(status_dict)));
    base::RunLoop().RunUntilIdle();
  }

  std::u16string GetTooltip() {
    return hotspot_tray_view_->GetTooltipText(gfx::Point());
  }

  std::u16string GetAccessibleNameString() {
    return hotspot_tray_view_->GetAccessibleNameString();
  }

  bool IsIconVisible() { return hotspot_tray_view_->GetVisible(); }

  bool IsJellyEnabled() const { return GetParam(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<hotspot_config::CrosHotspotConfigTestHelper>
      cros_hotspot_config_test_helper_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<HotspotTrayView, ExperimentalAsh> hotspot_tray_view_;
};

INSTANTIATE_TEST_SUITE_P(Jelly, HotspotTrayViewTest, testing::Bool());

TEST_P(HotspotTrayViewTest, HotspotIconImage) {
  SetHotspotStateAndClientCountInShill(shill::kTetheringStateActive, 0);
  if (IsJellyEnabled()) {
    EXPECT_TRUE(AreImagesEqual(
        hotspot_tray_view_->image_view()->GetImage(),
        gfx::CreateVectorIcon(kHotspotOnIcon, kUnifiedTrayIconSize,
                              widget_->GetColorProvider()->GetColor(
                                  cros_tokens::kCrosSysPrimary))));
  } else {
    EXPECT_TRUE(AreImagesEqual(
        hotspot_tray_view_->image_view()->GetImage(),
        gfx::CreateVectorIcon(
            kHotspotOnIcon, kUnifiedTrayIconSize,
            AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorPrimary))));
  }
}

TEST_P(HotspotTrayViewTest, HotspotIconVisibility) {
  EXPECT_FALSE(IsIconVisible());

  SetHotspotStateAndClientCountInShill(shill::kTetheringStateActive, 0);
  EXPECT_TRUE(IsIconVisible());

  SetHotspotStateAndClientCountInShill(shill::kTetheringStateIdle, 0);
  EXPECT_FALSE(IsIconVisible());
}

TEST_P(HotspotTrayViewTest, HotspotIconTooltip) {
  SetHotspotStateAndClientCountInShill(shill::kTetheringStateActive, 0);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_HOTSPOT_ON_NO_CONNECTED_DEVICES,
                ui::GetChromeOSDeviceName()),
            GetTooltip());
  EXPECT_EQ(GetTooltip(), GetAccessibleNameString());

  SetHotspotStateAndClientCountInShill(shill::kTetheringStateActive, 1);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_HOTSPOT_ON_ONE_CONNECTED_DEVICE,
                ui::GetChromeOSDeviceName()),
            GetTooltip());
  EXPECT_EQ(GetTooltip(), GetAccessibleNameString());

  SetHotspotStateAndClientCountInShill(shill::kTetheringStateActive, 3);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_HOTSPOT_ON_MULTIPLE_CONNECTED_DEVICES,
                base::NumberToString16(3), ui::GetChromeOSDeviceName()),
            GetTooltip());
  EXPECT_EQ(GetTooltip(), GetAccessibleNameString());
}

}  // namespace ash
