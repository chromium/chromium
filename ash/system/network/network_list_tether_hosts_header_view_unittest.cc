// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_tether_hosts_header_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/fake_network_list_network_header_view_delegate.h"
#include "ash/system/network/network_list_header_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tri_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class NetworkListTetherHostsHeaderViewTest : public AshTestBase {
 public:
  NetworkListTetherHostsHeaderViewTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kQsRevamp,
                              features::kInstantHotspotRebrand},
        /*disabled_features=*/{});
  }
  ~NetworkListTetherHostsHeaderViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    network_state_helper()->ClearDevices();

    network_state_helper()->manager_test()->AddTechnology(shill::kTypeCellular,
                                                          /*enabled=*/true);

    std::unique_ptr<NetworkListTetherHostsHeaderView>
        network_list_tether_hosts_header_view =
            std::make_unique<NetworkListTetherHostsHeaderView>(
                &fake_network_list_network_header_delegate_);

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    network_list_tether_hosts_header_view_ = widget_->SetContentsView(
        std::move(network_list_tether_hosts_header_view));

    // Wait for network state and device change events to be handled.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  NetworkStateTestHelper* network_state_helper() {
    return &network_config_helper_.network_state_helper();
  }

  HoverHighlightView* GetEntryRow() {
    return network_list_tether_hosts_header_view_->entry_row();
  }

  views::Label* GetLabelView() {
    return FindViewById<views::Label*>(
        NetworkListHeaderView::kTitleLabelViewId);
  }

  FakeNetworkListNetworkHeaderViewDelegate*
  fake_network_list_network_header_delegate() {
    return &fake_network_list_network_header_delegate_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  template <class T>
  T FindViewById(int id) {
    // For QsRevamp: child views are added into `entry_row()`.
    if (features::IsQsRevampEnabled()) {
      return static_cast<T>(
          network_list_tether_hosts_header_view_->entry_row()->GetViewByID(id));
    }
    return static_cast<T>(
        network_list_tether_hosts_header_view_->container()->GetViewByID(id));
  }

  std::unique_ptr<views::Widget> widget_;
  network_config::CrosNetworkConfigTestHelper network_config_helper_;
  FakeNetworkListNetworkHeaderViewDelegate
      fake_network_list_network_header_delegate_;
  raw_ptr<NetworkListTetherHostsHeaderView, DanglingUntriaged | ExperimentalAsh>
      network_list_tether_hosts_header_view_;
};

TEST_F(NetworkListTetherHostsHeaderViewTest, CanConstruct) {
  EXPECT_TRUE(true);
}

// TODO(b/298254852): check for correct label ID and icon.
// TODO(b/300155715): parameterize test suite on whether kQsRevamp is
// enabled/disabled.

}  // namespace ash
