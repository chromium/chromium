// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_detailed_network_view_impl.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/run_loop.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/cpp/fake_cros_network_config.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

using ::chromeos::network_config::FakeCrosNetworkConfig;
using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::DeviceStateType;
using ::chromeos::network_config::mojom::NetworkType;
using network_config::CrosNetworkConfigTestHelper;

// Helper that waits for a child view to be added to a parent view.
class ChildAddedWaiter : public views::ViewObserver {
 public:
  explicit ChildAddedWaiter(views::View* parent) : parent_(parent) {
    CHECK(parent_);
  }

  void Wait() {
    parent_->AddObserver(this);
    run_loop_.Run();
    parent_->RemoveObserver(this);
  }

  // views::ViewObserver:
  void OnChildViewAdded(views::View* observed_view,
                        views::View* child) override {
    CHECK_EQ(observed_view, parent_);
    run_loop_.Quit();
  }

 private:
  const raw_ptr<views::View> parent_;
  base::RunLoop run_loop_;
};

// Pixel tests for the quick settings network detailed view.
class NetworkDetailedNetworkViewPixelTest : public AshTestBase {
 public:
  NetworkDetailedNetworkViewPixelTest() = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    cros_network_ = std::make_unique<FakeCrosNetworkConfig>();
    Shell::Get()
        ->system_tray_model()
        ->network_state_model()
        ->ConfigureRemoteForTesting(cros_network_->GetPendingRemote());
    base::RunLoop().RunUntilIdle();
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  FakeCrosNetworkConfig* cros_network() { return cros_network_.get(); }

 private:
  std::unique_ptr<FakeCrosNetworkConfig> cros_network_;
};

TEST_F(NetworkDetailedNetworkViewPixelTest, Basics) {
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          "My Ethernet", NetworkType::kEthernet,
          ConnectionStateType::kConnected));

  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          "My Cellular", NetworkType::kCellular,
          ConnectionStateType::kConnected));

  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          "My Wi-Fi", NetworkType::kWiFi, ConnectionStateType::kConnected));

  // Show the system tray bubble.
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());

  // Show the detailed view.
  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowNetworkDetailedView();
  TrayDetailedView* detailed_view =
      system_tray->bubble()
          ->quick_settings_view()
          ->GetDetailedViewForTest<TrayDetailedView>();
  ASSERT_TRUE(detailed_view);
  ASSERT_TRUE(
      views::IsViewClass<NetworkDetailedNetworkViewImpl>(detailed_view));
  auto* network_detailed_view =
      static_cast<NetworkDetailedNetworkViewImpl*>(detailed_view);

  // Wait for UI to be populated.
  auto* scroll_view = network_detailed_view->scroll_view_for_testing();
  auto* scroll_contents = scroll_view->contents();
  ASSERT_TRUE(scroll_contents);
  ASSERT_TRUE(scroll_contents->children().empty());
  ChildAddedWaiter(scroll_contents).Wait();
  ASSERT_FALSE(scroll_contents->children().empty());

  // Compare pixels.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/11, detailed_view));
}

}  // namespace
}  // namespace ash
