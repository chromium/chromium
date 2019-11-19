// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_

#include <map>

#include "ash/login_status.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/optional.h"

namespace views {
class ToggleButton;
}  // namespace views

namespace ash {
namespace tray {

class BluetoothDetailedView : public TrayDetailedView {
 public:
  BluetoothDetailedView(DetailedViewDelegate* delegate, LoginStatus login);

  ~BluetoothDetailedView() override;

  // Shows/hides the loading indicator below the header.
  void ShowLoadingIndicator();
  void HideLoadingIndicator();

  // Shows/hides the container of the message "Bluetooth is disabled". It should
  // be shown instead of the device list when Bluetooth is disabled.
  void ShowBluetoothDisabledPanel();
  void HideBluetoothDisabledPanel();

  // Returns true if the device list has any devices, false otherwise.
  bool IsDeviceScrollListEmpty() const;

  // Updates the device list.
  void UpdateDeviceScrollList(
      const BluetoothDeviceList& connected_devices,
      const BluetoothDeviceList& connecting_devices,
      const BluetoothDeviceList& paired_not_connected_devices,
      const BluetoothDeviceList& discovered_not_paired_devices);

  // Sets the state of the toggle in the header.
  void SetToggleIsOn(bool is_on);

  // views::View:
  const char* GetClassName() const override;

 private:
  void CreateItems();

  void AppendSameTypeDevicesToScrollList(const BluetoothDeviceList& list,
                                         bool highlight,
                                         bool checked);

  // Returns true if the device with |device_id| is found in |device_list|.
  bool FoundDevice(const BluetoothAddress& device_id,
                   const BluetoothDeviceList& device_list) const;

  // Updates UI of the clicked bluetooth device to show it is being connected
  // or disconnected if such an operation is going to be performed underway.
  void UpdateClickedDevice(const BluetoothAddress& device_id,
                           views::View* item_container);

  void ShowSettings();

  base::Optional<BluetoothAddress> GetFocusedDeviceAddress() const;
  void FocusDeviceByAddress(const BluetoothAddress& address) const;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void HandleButtonPressed(views::Button* sender,
                           const ui::Event& event) override;
  void CreateExtraTitleRowButtons() override;

  // TODO(jamescook): Don't cache this.
  LoginStatus login_;

  std::map<views::View*, BluetoothAddress> device_map_;

  BluetoothDeviceList connecting_devices_;
  BluetoothDeviceList paired_not_connected_devices_;

  views::ToggleButton* toggle_;
  views::Button* settings_;

  // The container of the message "Bluetooth is disabled" and an icon. It should
  // be shown instead of Bluetooth device list when Bluetooth is disabled.
  views::View* disabled_panel_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_
