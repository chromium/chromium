// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_network_header_view.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/fake_network_list_network_header_view_delegate.h"
#include "ash/system/network/network_list_header_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tri_view.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/view.h"

namespace ash {

class NetworkListNetworkHeaderViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    network_list_network_header_view_ =
        std::make_unique<NetworkListNetworkHeaderView>(
            &fake_network_list_network_header_delegate_,
            IDS_ASH_STATUS_TRAY_NETWORK_MOBILE, kPhoneHubPhoneIcon);
  }

  void TearDown() override {
    network_list_network_header_view_.reset();

    AshTestBase::TearDown();
  }

  FakeNetworkListNetworkHeaderViewDelegate*
  fake_network_list_network_header_delegate() {
    return &fake_network_list_network_header_delegate_;
  }

  NetworkListNetworkHeaderView* network_list_network_header_view() {
    return network_list_network_header_view_.get();
  }

  views::ToggleButton* GetToggleButton() {
    return FindViewById<views::ToggleButton*>(
        NetworkListNetworkHeaderView::kToggleButtonId);
  }

  void SetToggleVisibility(bool visible) {
    network_list_network_header_view()->SetToggleVisibility(visible);
  }

  template <class T>
  T FindViewById(int id) {
    return static_cast<T>(
        network_list_network_header_view_->entry_row()->GetViewByID(id));
  }

 private:
  FakeNetworkListNetworkHeaderViewDelegate
      fake_network_list_network_header_delegate_;
  std::unique_ptr<NetworkListNetworkHeaderView>
      network_list_network_header_view_;
};

TEST_F(NetworkListNetworkHeaderViewTest, ToggleStates) {
  views::ToggleButton* toggle_button = GetToggleButton();
  EXPECT_NE(nullptr, toggle_button);
  EXPECT_EQ(views::Button::ButtonState::STATE_NORMAL,
            toggle_button->GetState());
  EXPECT_TRUE(toggle_button->GetVisible());

  EXPECT_TRUE(toggle_button->GetAcceptsEvents());
  EXPECT_FALSE(toggle_button->GetIsOn());

  network_list_network_header_view()->SetToggleState(/*enabled=*/false,
                                                     /*is_on=*/true,
                                                     /*animate_toggle=*/false);
  EXPECT_FALSE(toggle_button->GetAcceptsEvents());
  EXPECT_TRUE(toggle_button->GetIsOn());

  SetToggleVisibility(/*visible=*/false);
  EXPECT_FALSE(toggle_button->GetVisible());
}

}  // namespace ash
