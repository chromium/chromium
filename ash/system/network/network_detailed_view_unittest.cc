// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_view.h"

#include <memory>

#include "ash/login_status.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/fake_network_detailed_view_delegate.h"
#include "ash/system/network/network_info_bubble.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/system/tray/tri_view.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/user_action_tester.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace views {
class View;
}  // namespace views

namespace ash {
namespace {

const std::string kNetworkdId = "/network/id";

using chromeos::network_config::mojom::NetworkStatePropertiesPtr;

}  // namespace

class NetworkDetailedViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    list_type_ = NetworkDetailedView::ListType::LIST_TYPE_NETWORK;

    network_detailed_view_ = new NetworkDetailedView(
        &fake_detailed_view_delegate_, &fake_network_detailed_view_delegate_,
        list_type_);

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(network_detailed_view_);

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  views::Button* FindSettingsButton() {
    return FindViewById<views::Button*>(
        NetworkDetailedView::NetworkDetailedViewChildId::kSettingsButton);
  }

  views::Button* FindInfoButton() {
    return FindViewById<views::Button*>(
        NetworkDetailedView::NetworkDetailedViewChildId::kInfoButton);
  }

  NetworkInfoBubble* GetInfoBubble() {
    return network_detailed_view_->info_bubble_;
  }

  FakeNetworkDetailedViewDelegate* network_detailed_view_delegate() {
    return &fake_network_detailed_view_delegate_;
  }

  FakeDetailedViewDelegate* fake_detailed_view_delegate() {
    return &fake_detailed_view_delegate_;
  }

  NetworkDetailedView* network_detailed_view() {
    return network_detailed_view_;
  }

  void CheckHistogramBuckets(int count) {
    EXPECT_EQ(count, user_action_tester_.GetActionCount(
                         "ChromeOS.SystemTray.Network.SettingsButtonPressed"));
    EXPECT_EQ(count, user_action_tester_.GetActionCount(
                         "StatusArea_Network_Settings"));
  }

 private:
  template <class T>
  T FindViewById(NetworkDetailedView::NetworkDetailedViewChildId id) {
    return static_cast<T>(
        network_detailed_view_->GetViewByID(static_cast<int>(id)));
  }

  std::unique_ptr<views::Widget> widget_;
  NetworkDetailedView* network_detailed_view_;
  FakeNetworkDetailedViewDelegate fake_network_detailed_view_delegate_;
  FakeDetailedViewDelegate fake_detailed_view_delegate_;
  NetworkDetailedView::ListType list_type_;
  base::UserActionTester user_action_tester_;
};

TEST_F(NetworkDetailedViewTest, PressingSettingsButtonOpensSettings) {
  views::Button* settings_button = FindSettingsButton();

  CheckHistogramBuckets(/*count=*/0);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  LeftClickOn(settings_button);
  EXPECT_EQ(0, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0u, fake_detailed_view_delegate()->close_bubble_call_count());

  CheckHistogramBuckets(/*count=*/0);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  LeftClickOn(settings_button);
  EXPECT_EQ(1, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(1u, fake_detailed_view_delegate()->close_bubble_call_count());

  CheckHistogramBuckets(/*count=*/1);
}

TEST_F(NetworkDetailedViewTest, PressingInfoButtonOpensInfoBubble) {
  views::Button* info_button = FindInfoButton();
  LeftClickOn(info_button);
  for (int i = 0; i < 3; ++i) {
    LeftClickOn(info_button);
    base::RunLoop().RunUntilIdle();
    if (i % 2 == 0) {
      EXPECT_FALSE(GetInfoBubble());
      EXPECT_TRUE(network_detailed_view()->GetWidget()->IsActive());
    } else {
      EXPECT_TRUE(GetInfoBubble());
      EXPECT_FALSE(network_detailed_view()->GetWidget()->IsActive());
    }
  }
}

}  // namespace ash
