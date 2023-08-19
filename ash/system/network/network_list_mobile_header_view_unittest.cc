// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_mobile_header_view_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/fake_network_list_network_header_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
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
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const char kStubCellularDevicePath[] = "/device/stub_cellular_device";
const char kStubCellularDeviceName[] = "stub_cellular_device";

}  // namespace

class NetworkListMobileHeaderViewTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  NetworkListMobileHeaderViewTest() = default;
  ~NetworkListMobileHeaderViewTest() override = default;

  bool IsQsRevampEnabled() { return GetParam(); }

  // AshTestBase:
  void SetUp() override {
    if (IsQsRevampEnabled()) {
      feature_list_.InitAndEnableFeature(features::kQsRevamp);
    } else {
      feature_list_.InitAndDisableFeature(features::kQsRevamp);
    }
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
    std::unique_ptr<NetworkListMobileHeaderViewImpl>
        network_list_mobile_header_view =
            std::make_unique<NetworkListMobileHeaderViewImpl>(
                &fake_network_list_network_header_delegate_);

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    network_list_mobile_header_view_ =
        widget_->SetContentsView(std::move(network_list_mobile_header_view));
  }

  std::unique_ptr<CellularInhibitor::InhibitLock> InhibitCellularScanning(
      CellularInhibitor::InhibitReason inhibit_reason) {
    base::RunLoop inhibit_loop;

    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock;
    network_config_helper_.cellular_inhibitor()->InhibitCellularScanning(
        inhibit_reason,
        base::BindLambdaForTesting(
            [&](std::unique_ptr<CellularInhibitor::InhibitLock> result) {
              inhibit_lock = std::move(result);
              inhibit_loop.Quit();
            }));
    inhibit_loop.Run();
    return inhibit_lock;
  }

  void SetAddESimButtonState(bool enabled, bool visible) {
    network_list_mobile_header_view_->SetAddESimButtonState(enabled, visible);
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

  IconButton* GetAddEsimButton() {
    return FindViewById<IconButton*>(
        NetworkListMobileHeaderViewImpl::kAddESimButtonId);
  }

  views::ToggleButton* GetToggleButton() {
    return FindViewById<views::ToggleButton*>(
        features::IsQsRevampEnabled()
            ? NetworkListNetworkHeaderView::kQsToggleButtonId
            : NetworkListNetworkHeaderView::kToggleButtonId);
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
  template <class T>
  T FindViewById(int id) {
    // For QsRevamp: child views are added into `entry_row()`.
    if (IsQsRevampEnabled()) {
      return static_cast<T>(
          network_list_mobile_header_view_->entry_row()->GetViewByID(id));
    }
    return static_cast<T>(
        network_list_mobile_header_view_->container()->GetViewByID(id));
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  network_config::CrosNetworkConfigTestHelper network_config_helper_;
  FakeNetworkListNetworkHeaderViewDelegate
      fake_network_list_network_header_delegate_;
  raw_ptr<NetworkListMobileHeaderViewImpl, DanglingUntriaged | ExperimentalAsh>
      network_list_mobile_header_view_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         NetworkListMobileHeaderViewTest,
                         testing::Bool() /* IsQsRevampEnabled() */);

TEST_P(NetworkListMobileHeaderViewTest, HeaderLabel) {
  // QsRevamped `NetworkListHeaderView` doesn't have a header label.
  if (IsQsRevampEnabled()) {
    return;
  }
  Init();
  views::Label* labelView = GetLabelView();
  ASSERT_NE(nullptr, labelView);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE),
            labelView->GetText());
}

TEST_P(NetworkListMobileHeaderViewTest, AddEsimButtonStates) {
  // QsRevamped `NetworkListHeaderView` doesn't have a `add_esim_button`.
  if (IsQsRevampEnabled()) {
    return;
  }
  Init();
  IconButton* add_esim_button = GetAddEsimButton();
  ASSERT_NE(nullptr, add_esim_button);

  EXPECT_EQ(0, GetSystemTrayClient()->show_network_create_count());
  LeftClickOn(add_esim_button);
  EXPECT_EQ(1, GetSystemTrayClient()->show_network_create_count());
  EXPECT_EQ(::onc::network_type::kCellular,
            GetSystemTrayClient()->last_network_type());

  EXPECT_TRUE(add_esim_button->GetVisible());
  EXPECT_TRUE(add_esim_button->GetEnabled());
  SetAddESimButtonState(/*enabled=*/false, /*visible*/ false);
  EXPECT_FALSE(add_esim_button->GetVisible());
  EXPECT_FALSE(add_esim_button->GetEnabled());
}

TEST_P(NetworkListMobileHeaderViewTest, CellularInhibitState) {
  // QsRevamped `NetworkListHeaderView` doesn't have a `add_esim_button`.
  if (IsQsRevampEnabled()) {
    return;
  }
  Init();

  IconButton* add_esim_button = GetAddEsimButton();
  ASSERT_NE(nullptr, add_esim_button);

  // Tooltip is not initially set.
  EXPECT_EQ(u"", add_esim_button->GetTooltipText());

  // Tooltip is not updated when eSIM button is not visible, this is
  // because there would not be a valid tooltip when there isnt a valid
  // cellular device.
  SetAddESimButtonState(/*enabled=*/true, /*visible*/ false);
  EXPECT_EQ(u"", add_esim_button->GetTooltipText());

  SetAddESimButtonState(/*enabled=*/true, /*visible*/ true);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ADD_CELLULAR_LABEL),
            add_esim_button->GetTooltipText());

  const struct {
    CellularInhibitor::InhibitReason reason;
    int message_id;
  } kTestCases[]{
      {CellularInhibitor::InhibitReason::kInstallingProfile,
       IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_INSTALLING_PROFILE},
      {CellularInhibitor::InhibitReason::kRenamingProfile,
       IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_RENAMING_PROFILE},
      {CellularInhibitor::InhibitReason::kRemovingProfile,
       IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_REMOVING_PROFILE},
      {CellularInhibitor::InhibitReason::kConnectingToProfile,
       IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_CONNECTING_TO_PROFILE},
      {CellularInhibitor::InhibitReason::kRefreshingProfileList,
       IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_REFRESHING_PROFILE_LIST},
      {CellularInhibitor::InhibitReason::kResettingEuiccMemory,
       IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_RESETTING_ESIM},
      {CellularInhibitor::InhibitReason::kDisablingProfile,
       IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_DISABLING_PROFILE},
      {CellularInhibitor::InhibitReason::kRequestingAvailableProfiles,
       IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_REQUESTING_AVAILABLE_PROFILES},
  };

  for (auto cases : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "Inhibit Reason: " << cases.message_id);
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock =
        InhibitCellularScanning(cases.reason);
    EXPECT_TRUE(inhibit_lock);
    base::RunLoop().RunUntilIdle();
    SetAddESimButtonState(/*enabled=*/true, /*visible=*/true);
    EXPECT_EQ(l10n_util::GetStringUTF16(cases.message_id),
              add_esim_button->GetTooltipText());
    inhibit_lock = nullptr;
  }
}

TEST_P(NetworkListMobileHeaderViewTest, EnabledButtonNotAdded) {
  // QsRevamped `NetworkListHeaderView` doesn't have a `add_esim_button`.
  if (IsQsRevampEnabled()) {
    return;
  }

  // Add eSim button should not be added if the screen is locked.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  Init();

  IconButton* add_esim_button = GetAddEsimButton();
  EXPECT_EQ(nullptr, add_esim_button);
}

TEST_P(NetworkListMobileHeaderViewTest, MobileToggleButtonStates) {
  Init();
  views::ToggleButton* toggle_button = GetToggleButton();
  EXPECT_NE(nullptr, toggle_button);

  EXPECT_EQ(0u, fake_network_list_network_header_delegate()
                    ->mobile_toggle_clicked_count());
  LeftClickOn(toggle_button);
  EXPECT_EQ(1u, fake_network_list_network_header_delegate()
                    ->mobile_toggle_clicked_count());
}

TEST_P(NetworkListMobileHeaderViewTest, SetToggleStateUpdatesTooltips) {
  // Only QsRevamp uses an entry row.
  if (!IsQsRevampEnabled()) {
    return;
  }
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
