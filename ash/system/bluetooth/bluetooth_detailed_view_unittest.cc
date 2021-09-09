// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_detailed_view_impl.h"
#include "ash/system/bluetooth/bluetooth_disabled_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/widget/widget.h"

namespace views {
class View;
}  // namespace views

namespace ash {
namespace tray {
namespace {

using chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

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

class BluetoothDetailedViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kBluetoothRevamp);

    detailed_view_delegate_ =
        std::make_unique<DetailedViewDelegate>(/*tray_controller=*/nullptr);

    std::unique_ptr<BluetoothDetailedView> bluetooth_detailed_view =
        BluetoothDetailedView::Factory::Create(
            detailed_view_delegate_.get(),
            &fake_bluetooth_detailed_view_delegate_);
    bluetooth_detailed_view_ = bluetooth_detailed_view.get();

    widget_ = CreateFramelessTestWidget();
    widget_->SetContentsView(bluetooth_detailed_view.release()->GetAsView());
    widget_->SetFullscreen(true);

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    widget_.reset();
    detailed_view_delegate_.reset();

    AshTestBase::TearDown();
  }

  // Simulate a mouse click on the given view and wait for the event to be
  // processed.
  void ClickOnAndWait(const views::View* view) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();

    base::RunLoop().RunUntilIdle();
  }

  views::ToggleButton* FindBluetoothToggleButton() {
    return FindViewById<views::ToggleButton*>(
        BluetoothDetailedViewImpl::BluetoothDetailedViewChildId::kToggleButton);
  }

  BluetoothDisabledDetailedView* FindBluetoothDisabledView() {
    return FindViewById<BluetoothDisabledDetailedView*>(
        BluetoothDetailedViewImpl::BluetoothDetailedViewChildId::kDisabledView);
  }

  BluetoothDetailedView* bluetooth_detailed_view() {
    return bluetooth_detailed_view_;
  }

  FakeBluetoothDetailedViewDelegate* bluetooth_detailed_view_delegate() {
    return &fake_bluetooth_detailed_view_delegate_;
  }

 private:
  template <class T>
  T FindViewById(BluetoothDetailedViewImpl::BluetoothDetailedViewChildId id) {
    return static_cast<T>(bluetooth_detailed_view_->GetAsView()->GetViewByID(
        static_cast<int>(id)));
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;
  std::unique_ptr<views::Widget> widget_;
  BluetoothDetailedView* bluetooth_detailed_view_;
  FakeBluetoothDetailedViewDelegate fake_bluetooth_detailed_view_delegate_;
};

TEST_F(BluetoothDetailedViewTest,
       BluetoothEnabledStateChangesUpdateChildrenViewState) {
  views::ToggleButton* toggle_button = FindBluetoothToggleButton();
  BluetoothDisabledDetailedView* disabled_view = FindBluetoothDisabledView();

  EXPECT_FALSE(toggle_button->GetIsOn());
  EXPECT_TRUE(disabled_view->GetVisible());

  bluetooth_detailed_view()->UpdateBluetoothEnabledState(true);

  EXPECT_TRUE(toggle_button->GetIsOn());
  EXPECT_FALSE(disabled_view->GetVisible());

  bluetooth_detailed_view()->UpdateBluetoothEnabledState(false);

  EXPECT_FALSE(toggle_button->GetIsOn());
  EXPECT_TRUE(disabled_view->GetVisible());
}

TEST_F(BluetoothDetailedViewTest, PressingToggleNotifiesDelegate) {
  views::ToggleButton* toggle_button = FindBluetoothToggleButton();
  EXPECT_FALSE(toggle_button->GetIsOn());
  EXPECT_FALSE(
      bluetooth_detailed_view_delegate()->last_bluetooth_toggle_state());

  ClickOnAndWait(toggle_button);

  EXPECT_TRUE(toggle_button->GetIsOn());
  EXPECT_TRUE(
      bluetooth_detailed_view_delegate()->last_bluetooth_toggle_state());
}

TEST_F(BluetoothDetailedViewTest, BluetoothToggleHasCorrectTooltipText) {
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

}  // namespace tray
}  // namespace ash
