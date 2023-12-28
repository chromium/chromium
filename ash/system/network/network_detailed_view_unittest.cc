// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/login_status.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/fake_network_detailed_view_delegate.h"
#include "ash/system/network/network_feature_tile.h"
#include "ash/system/network/network_info_bubble.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class NetworkDetailedViewTest : public AshTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    if (IsInstantHotspotRebrandEnabled()) {
      feature_list_.InitAndEnableFeature(features::kInstantHotspotRebrand);
    } else {
      feature_list_.InitAndDisableFeature(features::kInstantHotspotRebrand);
    }
  }

  bool IsInstantHotspotRebrandEnabled() { return GetParam(); }

  void OpenNetworkDetailedView() {
    GetPrimaryUnifiedSystemTray()->ShowBubble();

    auto* quick_settings_view =
        GetPrimaryUnifiedSystemTray()->bubble()->quick_settings_view();
    ASSERT_TRUE(quick_settings_view);

    const auto* tile = static_cast<const NetworkFeatureTile*>(
        quick_settings_view->GetViewByID(VIEW_ID_FEATURE_TILE_NETWORK));
    ASSERT_TRUE(tile);
    LeftClickAndWait(tile);

    ASSERT_TRUE(quick_settings_view->detailed_view_container());
    views::View::Views children =
        quick_settings_view->detailed_view_container()->children();
    ASSERT_EQ(1u, children.size());

    network_detailed_view_ =
        views::AsViewClass<NetworkDetailedView>(children.front());
    ASSERT_TRUE(network_detailed_view_);
  }

  void LeftClickAndWait(const views::View* view) {
    ASSERT_TRUE(view);
    LeftClickOn(view);
    // Run until idle to ensure that any actions or navigations as a result of
    // clicking |view| are completed before returning.
    base::RunLoop().RunUntilIdle();
  }

  views::Button* FindSettingsButton() {
    return FindViewById<views::Button*>(
        NetworkDetailedView::NetworkDetailedViewChildId::kSettingsButton);
  }

  views::Button* FindInfoButton() {
    return FindViewById<views::Button*>(
        NetworkDetailedView::NetworkDetailedViewChildId::kInfoButton);
  }

  views::View* GetInfoBubble() {
    return network_detailed_view_->info_bubble_tracker_.view();
  }

  int GetTitleRowStringId() {
    return network_detailed_view_->title_row_string_id_for_testing();
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

  base::test::ScopedFeatureList feature_list_;
  raw_ptr<NetworkDetailedView, DanglingUntriaged> network_detailed_view_;
  base::UserActionTester user_action_tester_;
};

INSTANTIATE_TEST_SUITE_P(All, NetworkDetailedViewTest, testing::Bool());

TEST_P(NetworkDetailedViewTest, PressingSettingsButtonOpensSettings) {
  CheckHistogramBuckets(/*count=*/0);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  base::RunLoop().RunUntilIdle();

  GetPrimaryUnifiedSystemTray()->ShowBubble();

  auto* quick_settings_view =
      GetPrimaryUnifiedSystemTray()->bubble()->quick_settings_view();
  ASSERT_TRUE(quick_settings_view);

  const auto* tile = static_cast<const NetworkFeatureTile*>(
      quick_settings_view->GetViewByID(VIEW_ID_FEATURE_TILE_NETWORK));
  ASSERT_TRUE(tile);
  ASSERT_FALSE(tile->GetEnabled());

  CheckHistogramBuckets(/*count=*/0);

  GetPrimaryUnifiedSystemTray()->CloseBubble();

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  base::RunLoop().RunUntilIdle();

  OpenNetworkDetailedView();

  views::Button* settings_button = FindSettingsButton();
  ASSERT_TRUE(settings_button);

  LeftClickAndWait(settings_button);
  EXPECT_EQ(1, GetSystemTrayClient()->show_network_settings_count());

  CheckHistogramBuckets(/*count=*/1);
}

TEST_P(NetworkDetailedViewTest, PressingInfoButtonOpensInfoBubble) {
  OpenNetworkDetailedView();

  views::Button* info_button = FindInfoButton();

  ASSERT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  EXPECT_FALSE(GetInfoBubble());

  LeftClickAndWait(info_button);

  ASSERT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  EXPECT_TRUE(GetInfoBubble());

  LeftClickAndWait(info_button);

  ASSERT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  EXPECT_FALSE(GetInfoBubble());
}

TEST_P(NetworkDetailedViewTest, InfoBubbleClosedWhenDetailedViewClosed) {
  OpenNetworkDetailedView();

  views::Button* info_button = FindInfoButton();
  LeftClickAndWait(info_button);
  views::ViewTracker bubble_tracker_;
  bubble_tracker_.SetView(GetInfoBubble());
  EXPECT_TRUE(bubble_tracker_.view());

  // The info bubble should not exist after the detailed view has been closed.
  GetPrimaryUnifiedSystemTray()->CloseBubble();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(bubble_tracker_.view());
}

TEST_P(NetworkDetailedViewTest, TitleRowString) {
  OpenNetworkDetailedView();

  if (IsInstantHotspotRebrandEnabled()) {
    EXPECT_EQ(GetTitleRowStringId(), IDS_ASH_STATUS_TRAY_INTERNET);
  } else {
    EXPECT_EQ(GetTitleRowStringId(), IDS_ASH_STATUS_TRAY_NETWORK);
  }
}

}  // namespace ash
