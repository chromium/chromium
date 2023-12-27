// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_wifi_header_view.h"

#include <memory>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/fake_network_list_network_header_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

class NetworkListWifiHeaderViewTest : public AshTestBase {
 public:
  NetworkListWifiHeaderViewTest() = default;
  ~NetworkListWifiHeaderViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    std::unique_ptr<NetworkListWifiHeaderView> network_list_wifi_header_view =
        std::make_unique<NetworkListWifiHeaderView>(
            &fake_network_list_network_header_delegate_);

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    network_list_wifi_header_view_ =
        widget_->SetContentsView(std::move(network_list_wifi_header_view));
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  void SetToggleState(bool enabled, bool is_on) {
    network_list_wifi_header_view_->SetToggleState(enabled, is_on,
                                                   /*animate_toggle=*/true);
  }

  HoverHighlightView* GetEntryRow() {
    return network_list_wifi_header_view_->entry_row();
  }

  views::ToggleButton* GetToggleButton() {
    return FindViewById<views::ToggleButton*>(
        NetworkListNetworkHeaderView::kToggleButtonId);
  }

 private:
  template <class T>
  T FindViewById(int id) {
    return static_cast<T>(
        network_list_wifi_header_view_->entry_row()->GetViewByID(id));
  }

  std::unique_ptr<views::Widget> widget_;
  network_config::CrosNetworkConfigTestHelper network_config_helper_;
  FakeNetworkListNetworkHeaderViewDelegate
      fake_network_list_network_header_delegate_;
  raw_ptr<NetworkListWifiHeaderView, DanglingUntriaged>
      network_list_wifi_header_view_;
};

TEST_F(NetworkListWifiHeaderViewTest, SetToggleStateUpdatesTooltips) {
  SetToggleState(/*enabled=*/true, /*is_on=*/true);
  EXPECT_EQ(GetEntryRow()->GetTooltipText(),
            u"Toggle Wi-Fi. Wi-Fi is turned on.");
  EXPECT_EQ(GetToggleButton()->GetTooltipText(),
            u"Toggle Wi-Fi. Wi-Fi is turned on.");

  SetToggleState(/*enabled=*/true, /*is_on=*/false);
  EXPECT_EQ(GetEntryRow()->GetTooltipText(),
            u"Toggle Wi-Fi. Wi-Fi is turned off.");
  EXPECT_EQ(GetToggleButton()->GetTooltipText(),
            u"Toggle Wi-Fi. Wi-Fi is turned off.");
}

}  // namespace ash
