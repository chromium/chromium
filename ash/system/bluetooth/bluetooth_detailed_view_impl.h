// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_controller.h"
#include "ash/system/tray/tray_detailed_view.h"

namespace views {
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
  // BluetoothDetailedView:
  views::View* GetAsView() override;
  void SetBluetoothToggleState(bool enabled) override;
  BluetoothDeviceListItemView* AddDeviceListItem() override;
  ash::TriView* AddDeviceListSubHeader(const gfx::VectorIcon& icon,
                                       int text_id) override;
  void NotifyDeviceListChanged() override;
  views::View* device_list() override;

  // views::View:
  const char* GetClassName() const override;

  bool should_toggle_be_enabled_ = false;
  BluetoothDisabledDetailedView* disabled_view_ = nullptr;
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_IMPL_H_
