// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_wifi_header_view_impl.h"

#include <memory>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/fake_network_list_network_header_view_delegate.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

class NetworkListWifiHeaderViewTest : public AshTestBase {
 public:
  NetworkListWifiHeaderViewTest() = default;
  ~NetworkListWifiHeaderViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    std::unique_ptr<NetworkListWifiHeaderViewImpl>
        network_list_wifi_header_view =
            std::make_unique<NetworkListWifiHeaderViewImpl>(
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

  void SetJoinWifiButtonState(bool enabled, bool visible) {
    network_list_wifi_header_view()->SetJoinWifiButtonState(enabled, visible);
  }

  void SetToggleState(bool enabled, bool is_on) {
    network_list_wifi_header_view()->SetToggleState(enabled, is_on,
                                                    /*animate_toggle=*/true);
  }

  NetworkStateTestHelper* network_state_helper() {
    return &network_config_helper_.network_state_helper();
  }

  IconButton* GetJoinWifiButton() {
    return FindViewById<IconButton*>(
        NetworkListWifiHeaderViewImpl::kJoinWifiButtonId);
  }

  TrayToggleButton* GetToggleButton() {
    return FindViewById<TrayToggleButton*>(
        NetworkListNetworkHeaderView::kToggleButtonId);
  }

  views::Label* GetLabelView() {
    return FindViewById<views::Label*>(
        NetworkListHeaderView::kTitleLabelViewId);
  }

  FakeNetworkListNetworkHeaderViewDelegate*
  fake_network_list_network_header_delegate() {
    return &fake_network_list_network_header_delegate_;
  }

  NetworkListWifiHeaderViewImpl* network_list_wifi_header_view() {
    return network_list_wifi_header_view_;
  }

 private:
  template <class T>
  T FindViewById(int id) {
    return static_cast<T>(
        network_list_wifi_header_view_->container()->GetViewByID(id));
  }

  std::unique_ptr<views::Widget> widget_;
  network_config::CrosNetworkConfigTestHelper network_config_helper_;
  FakeNetworkListNetworkHeaderViewDelegate
      fake_network_list_network_header_delegate_;
  NetworkListWifiHeaderViewImpl* network_list_wifi_header_view_;
};

TEST_F(NetworkListWifiHeaderViewTest, HeaderLabel) {
  views::Label* label_view = GetLabelView();
  ASSERT_NE(nullptr, label_view);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_WIFI),
            label_view->GetText());
}

TEST_F(NetworkListWifiHeaderViewTest, JoinWifiButtonStates) {
  IconButton* join_wifi_button = GetJoinWifiButton();
  ASSERT_NE(nullptr, join_wifi_button);
  EXPECT_TRUE(join_wifi_button->GetEnabled());
  EXPECT_TRUE(join_wifi_button->GetVisible());

  EXPECT_EQ(0, GetSystemTrayClient()->show_network_create_count());
  LeftClickOn(join_wifi_button);
  EXPECT_EQ(1, GetSystemTrayClient()->show_network_create_count());
  EXPECT_EQ(::onc::network_type::kWiFi,
            GetSystemTrayClient()->last_network_type());

  SetJoinWifiButtonState(/*enabled=*/false, /*visible=*/false);
  EXPECT_FALSE(join_wifi_button->GetVisible());
  EXPECT_FALSE(join_wifi_button->GetEnabled());
}

TEST_F(NetworkListWifiHeaderViewTest, WifiToggleButton) {
  TrayToggleButton* toggle_button = GetToggleButton();
  ASSERT_NE(nullptr, toggle_button);
  EXPECT_TRUE(toggle_button->GetEnabled());

  IconButton* join_wifi_button = GetJoinWifiButton();
  ASSERT_NE(nullptr, join_wifi_button);
  EXPECT_TRUE(join_wifi_button->GetEnabled());

  SetToggleState(/*enabled=*/false, /*is_on=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(toggle_button->GetEnabled());
  EXPECT_FALSE(toggle_button->GetIsOn());

  // Add WiFi button is disabled each time WiFi is turned off.
  EXPECT_FALSE(join_wifi_button->GetEnabled());

  SetToggleState(/*enabled=*/true, /*is_on=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(toggle_button->GetEnabled());
  EXPECT_TRUE(toggle_button->GetIsOn());

  // Add WiFi button is enabled each time WiFi is turned on.
  EXPECT_TRUE(join_wifi_button->GetEnabled());

  EXPECT_EQ(
      0u,
      fake_network_list_network_header_delegate()->wifi_toggle_clicked_count());
  LeftClickOn(toggle_button);
  EXPECT_EQ(
      1u,
      fake_network_list_network_header_delegate()->wifi_toggle_clicked_count());
  EXPECT_FALSE(toggle_button->GetIsOn());
}

}  // namespace ash
