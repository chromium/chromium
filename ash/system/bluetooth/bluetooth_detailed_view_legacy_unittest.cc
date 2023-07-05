// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/bluetooth/bluetooth_detailed_view_legacy.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/bluetooth/bluetooth_disabled_detailed_view.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace views {
class View;
}  // namespace views

namespace ash {

namespace {

using bluetooth_config::mojom::BluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

const std::string kDeviceId = "/device/id";

class FakeBluetoothDetailedViewDelegate
    : public BluetoothDetailedView::Delegate {
 public:
  FakeBluetoothDetailedViewDelegate() = default;
  ~FakeBluetoothDetailedViewDelegate() override = default;

  // BluetoothDetailedView::Delegate:
  void OnToggleClicked(bool new_state) override {
    last_bluetooth_toggle_state_ = new_state;
  }

  void OnPairNewDeviceRequested() override {
    on_pair_new_device_requested_call_count_++;
  }

  void OnDeviceListItemSelected(
      const PairedBluetoothDevicePropertiesPtr& device) override {
    last_device_list_item_selected_ = mojo::Clone(device);
  }

  bool last_bluetooth_toggle_state() const {
    return last_bluetooth_toggle_state_;
  }

  size_t on_pair_new_device_requested_call_count() const {
    return on_pair_new_device_requested_call_count_;
  }

  const PairedBluetoothDevicePropertiesPtr& last_device_list_item_selected()
      const {
    return last_device_list_item_selected_;
  }

 private:
  bool last_bluetooth_toggle_state_ = false;
  size_t on_pair_new_device_requested_call_count_ = 0;
  PairedBluetoothDevicePropertiesPtr last_device_list_item_selected_;
};

}  // namespace

class BluetoothDetailedViewLegacyTest : public AshTestBase {
 public:
  void SetUp() override {
    // BluetoothDetailedViewLegacy is only used pre-QsRevamp.
    feature_list_.InitAndDisableFeature(features::kQsRevamp);

    AshTestBase::SetUp();

    std::unique_ptr<BluetoothDetailedView> bluetooth_detailed_view =
        BluetoothDetailedView::Factory::Create(
            &fake_detailed_view_delegate_,
            &fake_bluetooth_detailed_view_delegate_);
    bluetooth_detailed_view_ = bluetooth_detailed_view.get();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(bluetooth_detailed_view.release()->GetAsView());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  ash::IconButton* FindPairNewDeviceClickableView() {
    return FindViewById<ash::IconButton*>(
        BluetoothDetailedViewLegacy::BluetoothDetailedViewChildId::
            kPairNewDeviceClickableView);
  }

  views::ToggleButton* FindBluetoothToggleButton() {
    return FindViewById<views::ToggleButton*>(
        BluetoothDetailedViewLegacy::BluetoothDetailedViewChildId::
            kToggleButton);
  }

  views::Button* FindSettingsButton() {
    return FindViewById<views::Button*>(
        BluetoothDetailedViewLegacy::BluetoothDetailedViewChildId::
            kSettingsButton);
  }

  views::View* FindPairNewDeviceView() {
    return FindViewById<views::View*>(
        BluetoothDetailedViewLegacy::BluetoothDetailedViewChildId::
            kPairNewDeviceView);
  }

  BluetoothDisabledDetailedView* FindBluetoothDisabledView() {
    return FindViewById<BluetoothDisabledDetailedView*>(
        BluetoothDetailedViewLegacy::BluetoothDetailedViewChildId::
            kDisabledView);
  }

  BluetoothDetailedView* bluetooth_detailed_view() {
    return bluetooth_detailed_view_;
  }

  FakeBluetoothDetailedViewDelegate* bluetooth_detailed_view_delegate() {
    return &fake_bluetooth_detailed_view_delegate_;
  }

  FakeDetailedViewDelegate* fake_detailed_view_delegate() {
    return &fake_detailed_view_delegate_;
  }

 private:
  template <class T>
  T FindViewById(BluetoothDetailedViewLegacy::BluetoothDetailedViewChildId id) {
    return static_cast<T>(bluetooth_detailed_view_->GetAsView()->GetViewByID(
        static_cast<int>(id)));
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<BluetoothDetailedView, ExperimentalAsh> bluetooth_detailed_view_;
  FakeBluetoothDetailedViewDelegate fake_bluetooth_detailed_view_delegate_;
  FakeDetailedViewDelegate fake_detailed_view_delegate_;
};

TEST_F(BluetoothDetailedViewLegacyTest, PressingSettingsButtonOpensSettings) {
  views::Button* settings_button = FindSettingsButton();

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  LeftClickOn(settings_button);
  EXPECT_EQ(0, GetSystemTrayClient()->show_bluetooth_settings_count());
  EXPECT_EQ(0u, fake_detailed_view_delegate()->close_bubble_call_count());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  LeftClickOn(settings_button);
  EXPECT_EQ(1, GetSystemTrayClient()->show_bluetooth_settings_count());
  EXPECT_EQ(1u, fake_detailed_view_delegate()->close_bubble_call_count());
}

TEST_F(BluetoothDetailedViewLegacyTest,
       BluetoothEnabledStateChangesUpdateChildrenViewState) {
  views::ToggleButton* toggle_button = FindBluetoothToggleButton();
  views::View* pair_new_device_view = FindPairNewDeviceView();
  BluetoothDisabledDetailedView* disabled_view = FindBluetoothDisabledView();

  EXPECT_FALSE(toggle_button->GetIsOn());
  EXPECT_FALSE(pair_new_device_view->GetVisible());
  EXPECT_TRUE(disabled_view->GetVisible());

  bluetooth_detailed_view()->UpdateBluetoothEnabledState(true);

  EXPECT_TRUE(toggle_button->GetIsOn());
  EXPECT_TRUE(pair_new_device_view->GetVisible());
  EXPECT_FALSE(disabled_view->GetVisible());

  bluetooth_detailed_view()->UpdateBluetoothEnabledState(false);

  EXPECT_FALSE(toggle_button->GetIsOn());
  EXPECT_FALSE(pair_new_device_view->GetVisible());
  EXPECT_TRUE(disabled_view->GetVisible());
}

TEST_F(BluetoothDetailedViewLegacyTest, PressingToggleNotifiesDelegate) {
  views::ToggleButton* toggle_button = FindBluetoothToggleButton();
  EXPECT_FALSE(toggle_button->GetIsOn());
  EXPECT_FALSE(
      bluetooth_detailed_view_delegate()->last_bluetooth_toggle_state());

  LeftClickOn(toggle_button);

  EXPECT_TRUE(toggle_button->GetIsOn());
  EXPECT_TRUE(
      bluetooth_detailed_view_delegate()->last_bluetooth_toggle_state());
}

TEST_F(BluetoothDetailedViewLegacyTest, BluetoothToggleHasCorrectTooltipText) {
  views::ToggleButton* toggle_button = FindBluetoothToggleButton();

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED_TOOLTIP)),
            toggle_button->GetTooltipText());

  bluetooth_detailed_view()->UpdateBluetoothEnabledState(true);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_BLUETOOTH_TOGGLE_TOOLTIP,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED_TOOLTIP)),
            toggle_button->GetTooltipText());
}

TEST_F(BluetoothDetailedViewLegacyTest, PressingPairNewDeviceNotifiesDelegate) {
  IconButton* pair_new_device_button = FindPairNewDeviceClickableView();
  views::View* pair_new_device_view = FindPairNewDeviceView();

  EXPECT_FALSE(pair_new_device_view->GetVisible());
  EXPECT_EQ(0u, bluetooth_detailed_view_delegate()
                    ->on_pair_new_device_requested_call_count());

  bluetooth_detailed_view()->UpdateBluetoothEnabledState(true);
  LeftClickOn(pair_new_device_button);
  EXPECT_EQ(1u, bluetooth_detailed_view_delegate()
                    ->on_pair_new_device_requested_call_count());
}

TEST_F(BluetoothDetailedViewLegacyTest, PairNewDeviceButtonIsCentered) {
  IconButton* pair_new_device_button = FindPairNewDeviceClickableView();
  views::View* pair_new_device_view = FindPairNewDeviceView();

  bluetooth_detailed_view()->UpdateBluetoothEnabledState(true);

  EXPECT_EQ(2u, pair_new_device_view->children().size());
  EXPECT_STREQ("Separator",
               pair_new_device_view->children().at(1)->GetClassName());

  views::View* separator = pair_new_device_view->children().at(1);

  const gfx::Point button_center =
      pair_new_device_button->GetBoundsInScreen().CenterPoint();
  const gfx::Rect& view_bounds = pair_new_device_view->GetBoundsInScreen();
  const int separator_height = separator->GetContentsBounds().height();

  // When determining the center of the view we should not consider the content
  // of the following separator, and only its top padding.
  const int view_center =
      view_bounds.y() + (view_bounds.height() - separator_height + 1) / 2;

  EXPECT_EQ(view_center, button_center.y());
}

TEST_F(BluetoothDetailedViewLegacyTest,
       SelectingDeviceListItemNotifiesDelegate) {
  bluetooth_detailed_view()->UpdateBluetoothEnabledState(true);

  PairedBluetoothDevicePropertiesPtr paired_properties =
      PairedBluetoothDeviceProperties::New();
  paired_properties->device_properties = BluetoothDeviceProperties::New();
  paired_properties->device_properties->id = kDeviceId;

  BluetoothDeviceListItemView* device_list_item =
      bluetooth_detailed_view()->AddDeviceListItem();
  device_list_item->UpdateDeviceProperties(
      /*device_index=*/0, /*device_count=*/0, paired_properties);

  bluetooth_detailed_view()->NotifyDeviceListChanged();

  EXPECT_FALSE(
      bluetooth_detailed_view_delegate()->last_device_list_item_selected());
  LeftClickOn(device_list_item);
  EXPECT_TRUE(
      bluetooth_detailed_view_delegate()->last_device_list_item_selected());
  EXPECT_EQ(kDeviceId, bluetooth_detailed_view_delegate()
                           ->last_device_list_item_selected()
                           ->device_properties->id);
}

}  // namespace ash
