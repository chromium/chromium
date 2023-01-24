// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_mobile_header_view_impl.h"

#include <memory>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/network/fake_network_list_network_header_view_delegate.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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

  IconButton* GetAddEsimButton() {
    return FindViewById<IconButton*>(
        NetworkListMobileHeaderViewImpl::kAddESimButtonId);
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

 private:
  template <class T>
  T FindViewById(int id) {
    return static_cast<T>(
        network_list_mobile_header_view_->container()->GetViewByID(id));
  }

  std::unique_ptr<views::Widget> widget_;
  network_config::CrosNetworkConfigTestHelper network_config_helper_;
  FakeNetworkListNetworkHeaderViewDelegate
      fake_network_list_network_header_delegate_;
  NetworkListMobileHeaderViewImpl* network_list_mobile_header_view_;
};

TEST_F(NetworkListMobileHeaderViewTest, HeaderLabel) {
  Init();
  views::Label* labelView = GetLabelView();
  ASSERT_NE(nullptr, labelView);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE),
            labelView->GetText());
}

TEST_F(NetworkListMobileHeaderViewTest, AddEsimButtonStates) {
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

TEST_F(NetworkListMobileHeaderViewTest, CellularInhibitState) {
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

TEST_F(NetworkListMobileHeaderViewTest, EnabledButtonNotAdded) {
  // Add eSim button should not be added if the screen is locked.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  Init();

  IconButton* add_esim_button = GetAddEsimButton();
  EXPECT_EQ(nullptr, add_esim_button);
}

TEST_F(NetworkListMobileHeaderViewTest, MobileToggleButtonStates) {
  Init();
  TrayToggleButton* toggle_button = GetToggleButton();
  EXPECT_NE(nullptr, toggle_button);

  EXPECT_EQ(0u, fake_network_list_network_header_delegate()
                    ->mobile_toggle_clicked_count());
  LeftClickOn(toggle_button);
  EXPECT_EQ(1u, fake_network_list_network_header_delegate()
                    ->mobile_toggle_clicked_count());
}

}  // namespace ash
