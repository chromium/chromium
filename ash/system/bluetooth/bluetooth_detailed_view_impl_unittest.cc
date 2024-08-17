// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_impl.h"

#include <memory>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using bluetooth_config::mojom::BluetoothDeviceProperties;
using bluetooth_config::mojom::BluetoothSystemState;
using bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

const char kDeviceId[] = "/device/id";

class FakeBluetoothDetailedViewDelegate
    : public BluetoothDetailedView::Delegate {
 public:
  FakeBluetoothDetailedViewDelegate() = default;
  ~FakeBluetoothDetailedViewDelegate() override = default;

  // BluetoothDetailedView::Delegate:
  void OnToggleClicked(bool new_state) override {
    last_toggle_state_ = new_state;
  }

  void OnPairNewDeviceRequested() override {
    ++pair_new_device_requested_count_;
  }

  void OnDeviceListItemSelected(
      const PairedBluetoothDevicePropertiesPtr& device) override {
    last_device_list_item_selected_ = mojo::Clone(device);
  }

  bool last_toggle_state_ = false;
  int pair_new_device_requested_count_ = 0;
  PairedBluetoothDevicePropertiesPtr last_device_list_item_selected_;
};

}  // namespace

class BluetoothDetailedViewImplTest : public AshTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  BluetoothDetailedViewImplTest() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(
        chromeos::features::kBluetoothWifiQSPodRefresh,
        IsBluetoothWifiQSPodRefreshEnabled());
  }

  void SetUp() override {
    AshTestBase::SetUp();

    auto bluetooth_detailed_view = std::make_unique<BluetoothDetailedViewImpl>(
        &detailed_view_delegate_, &bluetooth_detailed_view_delegate_);
    bluetooth_detailed_view_ = bluetooth_detailed_view.get();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(bluetooth_detailed_view.release()->GetAsView());
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  views::Button* GetSettingsButton() {
    return bluetooth_detailed_view_->settings_button_;
  }

  bool IsBluetoothWifiQSPodRefreshEnabled() { return GetParam(); }

  HoverHighlightView* GetToggleRow() {
    return bluetooth_detailed_view_->toggle_row_;
  }

  Switch* GetToggleButton() { return bluetooth_detailed_view_->toggle_button_; }

  RoundedContainer* GetMainContainer() {
    return bluetooth_detailed_view_->main_container_;
  }

  views::Button* GetPairNewDeviceView() {
    return bluetooth_detailed_view_->pair_new_device_view_;
  }

  std::unique_ptr<views::Widget> widget_;
  FakeBluetoothDetailedViewDelegate bluetooth_detailed_view_delegate_;
  FakeDetailedViewDelegate detailed_view_delegate_;
  raw_ptr<BluetoothDetailedViewImpl, DanglingUntriaged>
      bluetooth_detailed_view_ = nullptr;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BluetoothDetailedViewImplTest,
    /*IsBluetoothWifiQSPodRefreshEnabled()=*/testing::Bool());

TEST_P(BluetoothDetailedViewImplTest, PressingSettingsButtonOpensSettings) {
  views::Button* settings_button = GetSettingsButton();

  // Clicking the button at the lock screen does nothing.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  LeftClickOn(settings_button);
  EXPECT_EQ(0, GetSystemTrayClient()->show_bluetooth_settings_count());
  EXPECT_EQ(0u, detailed_view_delegate_.close_bubble_call_count());

  // Clicking the button in an active user session opens OS settings.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  LeftClickOn(settings_button);
  EXPECT_EQ(1, GetSystemTrayClient()->show_bluetooth_settings_count());
  EXPECT_EQ(1u, detailed_view_delegate_.close_bubble_call_count());
}

TEST_P(BluetoothDetailedViewImplTest,
       UpdateBluetoothEnabledStateChangesUIState) {
  HoverHighlightView* toggle_row = GetToggleRow();
  Switch* toggle_button = GetToggleButton();
  RoundedContainer* main_container = GetMainContainer();
  views::Button* pair_new_device_view = GetPairNewDeviceView();

  bluetooth_detailed_view_->UpdateBluetoothEnabledState(
      BluetoothSystemState::kEnabled);

  EXPECT_EQ(u"On", toggle_row->text_label()->GetText());
  EXPECT_EQ(u"Toggle Bluetooth. Bluetooth is on.",
            toggle_row->GetTooltipText());
  EXPECT_TRUE(toggle_button->GetIsOn());
  EXPECT_EQ(u"Toggle Bluetooth. Bluetooth is on.",
            toggle_button->GetTooltipText());
  EXPECT_TRUE(main_container->GetVisible());
  EXPECT_TRUE(pair_new_device_view->GetVisible());
  if (IsBluetoothWifiQSPodRefreshEnabled()) {
    EXPECT_FALSE(
        bluetooth_detailed_view_->zero_state_view_for_testing()->GetVisible());
  }
  EXPECT_TRUE(
      bluetooth_detailed_view_->scroll_view_for_testing()->GetVisible());

  bluetooth_detailed_view_->UpdateBluetoothEnabledState(
      BluetoothSystemState::kDisabled);

  EXPECT_EQ(u"Off", toggle_row->text_label()->GetText());
  EXPECT_EQ(u"Toggle Bluetooth. Bluetooth is off.",
            toggle_row->GetTooltipText());
  EXPECT_FALSE(toggle_button->GetIsOn());
  EXPECT_EQ(u"Toggle Bluetooth. Bluetooth is off.",
            toggle_button->GetTooltipText());
  EXPECT_FALSE(main_container->GetVisible());
  if (IsBluetoothWifiQSPodRefreshEnabled()) {
    EXPECT_FALSE(
        bluetooth_detailed_view_->zero_state_view_for_testing()->GetVisible());
  }
  EXPECT_TRUE(
      bluetooth_detailed_view_->scroll_view_for_testing()->GetVisible());
  bluetooth_detailed_view_->UpdateBluetoothEnabledState(
      BluetoothSystemState::kEnabling);
  EXPECT_EQ(u"On", toggle_row->text_label()->GetText());
  EXPECT_EQ(u"Toggle Bluetooth. Bluetooth is on.",
            toggle_row->GetTooltipText());
  EXPECT_TRUE(toggle_button->GetIsOn());
  EXPECT_EQ(u"Toggle Bluetooth. Bluetooth is on.",
            toggle_button->GetTooltipText());
  EXPECT_TRUE(main_container->GetVisible());
  EXPECT_FALSE(pair_new_device_view->GetVisible());
  if (IsBluetoothWifiQSPodRefreshEnabled()) {
    EXPECT_FALSE(
        bluetooth_detailed_view_->zero_state_view_for_testing()->GetVisible());
  }
  EXPECT_TRUE(
      bluetooth_detailed_view_->scroll_view_for_testing()->GetVisible());

  bluetooth_detailed_view_->UpdateBluetoothEnabledState(
      BluetoothSystemState::kUnavailable);
  if (IsBluetoothWifiQSPodRefreshEnabled()) {
    EXPECT_TRUE(
        bluetooth_detailed_view_->zero_state_view_for_testing()->GetVisible());
    EXPECT_FALSE(
        bluetooth_detailed_view_->scroll_view_for_testing()->GetVisible());
  }
}

TEST_P(BluetoothDetailedViewImplTest, PressingToggleRowNotifiesDelegate) {
  HoverHighlightView* toggle_row = GetToggleRow();
  EXPECT_FALSE(bluetooth_detailed_view_delegate_.last_toggle_state_);

  LeftClickOn(toggle_row);

  EXPECT_TRUE(bluetooth_detailed_view_delegate_.last_toggle_state_);
}

TEST_P(BluetoothDetailedViewImplTest, PressingToggleButtonNotifiesDelegate) {
  Switch* toggle_button = GetToggleButton();
  views::Button* pair_new_device_view = GetPairNewDeviceView();

  EXPECT_FALSE(toggle_button->GetIsOn());
  EXPECT_FALSE(bluetooth_detailed_view_delegate_.last_toggle_state_);
  EXPECT_FALSE(pair_new_device_view->GetVisible());

  LeftClickOn(toggle_button);

  EXPECT_TRUE(toggle_button->GetIsOn());
  EXPECT_TRUE(bluetooth_detailed_view_delegate_.last_toggle_state_);
  EXPECT_FALSE(pair_new_device_view->GetVisible());
}

TEST_P(BluetoothDetailedViewImplTest, PressingPairNewDeviceNotifiesDelegate) {
  bluetooth_detailed_view_->UpdateBluetoothEnabledState(
      BluetoothSystemState::kEnabled);
  views::test::RunScheduledLayout(bluetooth_detailed_view_);

  // Clicking the "pair new device" row notifies the delegate.
  views::Button* pair_new_device_view = GetPairNewDeviceView();
  LeftClickOn(pair_new_device_view);
  EXPECT_EQ(1,
            bluetooth_detailed_view_delegate_.pair_new_device_requested_count_);
}

TEST_P(BluetoothDetailedViewImplTest, SelectingDeviceListItemNotifiesDelegate) {
  bluetooth_detailed_view_->UpdateBluetoothEnabledState(
      BluetoothSystemState::kEnabled);

  // Create a simulated device and add it to the list.
  PairedBluetoothDevicePropertiesPtr paired_properties =
      PairedBluetoothDeviceProperties::New();
  paired_properties->device_properties = BluetoothDeviceProperties::New();
  paired_properties->device_properties->id = kDeviceId;

  BluetoothDeviceListItemView* device_list_item =
      bluetooth_detailed_view_->AddDeviceListItem();
  device_list_item->UpdateDeviceProperties(
      /*device_index=*/0, /*total_device_count=*/0, paired_properties);

  bluetooth_detailed_view_->NotifyDeviceListChanged();

  // Clicking on the item notifies the delegate that the device was selected.
  LeftClickOn(device_list_item);
  EXPECT_TRUE(
      bluetooth_detailed_view_delegate_.last_device_list_item_selected_);
  EXPECT_EQ(kDeviceId,
            bluetooth_detailed_view_delegate_.last_device_list_item_selected_
                ->device_properties->id);
}

}  // namespace ash
