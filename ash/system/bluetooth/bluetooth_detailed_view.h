// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_

#include "ash/login_status.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/tray/tray_detailed_view.h"

namespace views {
class ToggleButton;
}  // namespace views

namespace ash {
namespace tray {

class BluetoothDetailedView : public TrayDetailedView {
 public:
  BluetoothDetailedView(DetailedViewDelegate* delegate, LoginStatus login);

  ~BluetoothDetailedView() override;

  void Update();

 private:
  void CreateItems();

  void BluetoothStartDiscovering();

  void BluetoothStopDiscovering();

  void UpdateBluetoothDeviceList();
  void UpdateHeaderEntry();
  void UpdateDeviceScrollList();
  void AppendSameTypeDevicesToScrollList(const BluetoothDeviceList& list,
                                         bool highlight,
                                         bool checked);

  // Returns true if the device with |device_id| is found in |device_list|.
  bool FoundDevice(const std::string& device_id,
                   const BluetoothDeviceList& device_list) const;

  // Updates UI of the clicked bluetooth device to show it is being connected
  // or disconnected if such an operation is going to be performed underway.
  void UpdateClickedDevice(const std::string& device_id,
                           views::View* item_container);

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void HandleButtonPressed(views::Button* sender,
                           const ui::Event& event) override;
  void CreateExtraTitleRowButtons() override;

  void ShowSettings();
  void ShowLoadingIndicator();
  void HideLoadingIndicator();
  void ShowDisabledPanel();
  void HideDisabledPanel();

  std::string GetFocusedDeviceAddress() const;
  void FocusDeviceByAddress(const std::string& address) const;
  void DoUpdate();

  // TODO(jamescook): Don't cache this.
  LoginStatus login_;

  std::map<views::View*, std::string> device_map_;

  BluetoothDeviceList connected_devices_;
  BluetoothDeviceList connecting_devices_;
  BluetoothDeviceList paired_not_connected_devices_;
  BluetoothDeviceList discovered_not_paired_devices_;

  views::ToggleButton* toggle_;
  views::Button* settings_;

  // The container of the message "Bluetooth is disabled" and an icon. It should
  // be shown instead of Bluetooth device list when Bluetooth is disabled.
  views::View* disabled_panel_;

  // Timer used to limit the update frequency.
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_
