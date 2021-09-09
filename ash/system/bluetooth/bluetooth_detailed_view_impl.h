// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/weak_ptr.h"

namespace views {
class ToggleButton;
class View;
}  // namespace views

namespace ash {

class BluetoothDeviceListItemView;
class BluetoothDisabledDetailedView;
class DetailedViewDelegate;
class TriView;

namespace tray {

// BluetoothDetailedView implementation.
class ASH_EXPORT BluetoothDetailedViewImpl : public BluetoothDetailedView,
                                             public TrayDetailedView {
 public:
  BluetoothDetailedViewImpl(DetailedViewDelegate* detailed_view_delegate,
                            BluetoothDetailedView::Delegate* delegate);
  BluetoothDetailedViewImpl(const BluetoothDetailedViewImpl&) = delete;
  BluetoothDetailedViewImpl& operator=(const BluetoothDetailedViewImpl&) =
      delete;
  ~BluetoothDetailedViewImpl() override;

 private:
  friend class BluetoothDetailedViewTest;

  // Used for testing. Starts at 1 because view IDs should not be 0.
  enum class BluetoothDetailedViewChildId {
    kToggleButton = 1,
    kDisabledView = 2,
  };

  // BluetoothDetailedView:
  views::View* GetAsView() override;
  void UpdateBluetoothEnabledState(bool enabled) override;
  BluetoothDeviceListItemView* AddDeviceListItem() override;
  ash::TriView* AddDeviceListSubHeader(const gfx::VectorIcon& icon,
                                       int text_id) override;
  void NotifyDeviceListChanged() override;
  views::View* device_list() override;

  // views::View:
  const char* GetClassName() const override;

  // Creates and configures the Bluetooth toggle button and the settings button.
  void CreateTitleRowButtons();

  // Creates and configures the Bluetooth disabled view.
  void CreateDisabledView();

  // Propagates user interaction with the Bluetooth toggle button.
  void OnToggleClicked();

  views::ToggleButton* toggle_ = nullptr;
  BluetoothDisabledDetailedView* disabled_view_ = nullptr;

  base::WeakPtrFactory<BluetoothDetailedViewImpl> weak_factory_{this};
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_
