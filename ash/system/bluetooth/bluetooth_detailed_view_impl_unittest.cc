// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view_impl.h"

#include <memory>

#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using bluetooth_config::mojom::BluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDeviceProperties;
using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

const char kDeviceId[] = "/device/id";

class FakeBluetoothDetailedViewDelegate
    : public BluetoothDetailedView::Delegate {
 public:
  FakeBluetoothDetailedViewDelegate() = default;
  ~FakeBluetoothDetailedViewDelegate() override = default;

  // BluetoothDetailedView::Delegate:
  void OnToggleClicked(bool new_state) override {}

  void OnPairNewDeviceRequested() override {}

  void OnDeviceListItemSelected(
      const PairedBluetoothDevicePropertiesPtr& device) override {
    last_device_list_item_selected_ = mojo::Clone(device);
  }

  PairedBluetoothDevicePropertiesPtr last_device_list_item_selected_;
};

// This class exists to stub out the CloseBubble() call. This allows tests to
// directly construct the detailed view, without depending on the entire quick
// settings bubble and view hierarchy.
class FakeDetailedViewDelegate : public DetailedViewDelegate {
 public:
  FakeDetailedViewDelegate()
      : DetailedViewDelegate(/*tray_controller=*/nullptr) {}
  ~FakeDetailedViewDelegate() override = default;

  // DetailedViewDelegate:
  void CloseBubble() override {}
};

}  // namespace

class BluetoothDetailedViewImplTest : public AshTestBase {
 public:
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

  std::unique_ptr<views::Widget> widget_;
  FakeBluetoothDetailedViewDelegate bluetooth_detailed_view_delegate_;
  FakeDetailedViewDelegate detailed_view_delegate_;
  BluetoothDetailedView* bluetooth_detailed_view_ = nullptr;
};

TEST_F(BluetoothDetailedViewImplTest, SelectingDeviceListItemNotifiesDelegate) {
  bluetooth_detailed_view_->UpdateBluetoothEnabledState(true);

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
