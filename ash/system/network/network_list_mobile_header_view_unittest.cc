// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_mobile_header_view.h"

#include <memory>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/fake_network_list_network_header_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const char kStubCellularDevicePath[] = "/device/stub_cellular_device";
const char kStubCellularDeviceName[] = "stub_cellular_device";

}  // namespace

class NetworkListMobileHeaderViewTest : public AshTestBase {
 public:
  NetworkListMobileHeaderViewTest() = default;
  ~NetworkListMobileHeaderViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    network_state_helper()->ClearDevices();

    network_state_helper()->manager_test()->AddTechnology(shill::kTypeCellular,
                                                          /*enabled=*/true);

    network_state_helper()->device_test()->AddDevice(
        kStubCellularDevicePath, shill::kTypeCellular, kStubCellularDeviceName);

    // Wait for network state and device change events to be handled.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  void Init() {
    std::unique_ptr<NetworkListMobileHeaderView>
        network_list_mobile_header_view =
            std::make_unique<NetworkListMobileHeaderView>(
                &fake_network_list_network_header_delegate_);

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    network_list_mobile_header_view_ =
        widget_->SetContentsView(std::move(network_list_mobile_header_view));
  }

  NetworkStateTestHelper* network_state_helper() {
    return &network_config_helper_.network_state_helper();
  }

  void SetToggleState(bool is_on) {
    network_list_mobile_header_view_->SetToggleState(/*enabled=*/true, is_on,
                                                     /*animate_toggle=*/false);
  }

  HoverHighlightView* GetEntryRow() {
    return network_list_mobile_header_view_->entry_row();
  }

  views::ToggleButton* GetToggleButton() {
    return FindViewById<views::ToggleButton*>(
        NetworkListNetworkHeaderView::kToggleButtonId);
  }

  FakeNetworkListNetworkHeaderViewDelegate*
  fake_network_list_network_header_delegate() {
    return &fake_network_list_network_header_delegate_;
  }

 private:
  template <class T>
  T FindViewById(int id) {
    return static_cast<T>(
        network_list_mobile_header_view_->entry_row()->GetViewByID(id));
  }

  std::unique_ptr<views::Widget> widget_;
  network_config::CrosNetworkConfigTestHelper network_config_helper_;
  FakeNetworkListNetworkHeaderViewDelegate
      fake_network_list_network_header_delegate_;
  raw_ptr<NetworkListMobileHeaderView, DanglingUntriaged>
      network_list_mobile_header_view_;
};

TEST_F(NetworkListMobileHeaderViewTest, MobileToggleButtonStates) {
  Init();
  views::ToggleButton* toggle_button = GetToggleButton();
  EXPECT_NE(nullptr, toggle_button);

  EXPECT_EQ(0u, fake_network_list_network_header_delegate()
                    ->mobile_toggle_clicked_count());
  LeftClickOn(toggle_button);
  EXPECT_EQ(1u, fake_network_list_network_header_delegate()
                    ->mobile_toggle_clicked_count());
}

TEST_F(NetworkListMobileHeaderViewTest, SetToggleStateUpdatesTooltips) {
  Init();
  SetToggleState(true);
  EXPECT_EQ(GetEntryRow()->GetTooltipText(),
            u"Toggle mobile data. Mobile data is turned on.");
  EXPECT_EQ(GetToggleButton()->GetTooltipText(),
            u"Toggle mobile data. Mobile data is turned on.");

  SetToggleState(false);
  EXPECT_EQ(GetEntryRow()->GetTooltipText(),
            u"Toggle mobile data. Mobile data is turned off.");
  EXPECT_EQ(GetToggleButton()->GetTooltipText(),
            u"Toggle mobile data. Mobile data is turned off.");
}

}  // namespace ash
