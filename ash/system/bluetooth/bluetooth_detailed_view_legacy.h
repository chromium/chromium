// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_LEGACY_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_LEGACY_H_

#include <unordered_map>

#include "ash/login_status.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace views {
class ToggleButton;
}  // namespace views

namespace ash {

class TriView;
class TrayInfoLabel;

// TODO(crbug.com/1234138): Remove this class when the
// |ash::features::kBluetoothRevamp| feature flag is fully launched.
class BluetoothDetailedViewLegacy : public TrayDetailedView {
 public:
  // ID for scroll content view. Used in testing.
  static const int kScrollContentID = 1;

  BluetoothDetailedViewLegacy(DetailedViewDelegate* delegate,
                              LoginStatus login);

  BluetoothDetailedViewLegacy(const BluetoothDetailedViewLegacy&) = delete;
  BluetoothDetailedViewLegacy& operator=(const BluetoothDetailedViewLegacy&) =
      delete;

  ~BluetoothDetailedViewLegacy() override;

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

  // Adds subheading with given |text_id| at given |child_index|. If
  // |sub_heading_view| is nullptr, then a new view is created. Returns
  // |sub_heading_view| or the newly created view.
  TriView* AddSubHeading(int text_id,
                         TriView* sub_heading_view,
                         int child_index);

  // Adds devices from |list| into the scroll list at given |child_index|.
  // To avoid disrupting a11y, list items are re-used from |old_device_list| if
  // it exists. Returns index position for next scroll list item.
  int AddSameTypeDevicesToScrollList(
      const BluetoothDeviceList& list,
      const std::unordered_map<HoverHighlightView*, BluetoothAddress>&
          old_device_list,
      int child_index,
      bool highlight,
      bool checked);

  // Returns true if the device with |device_id| is found in |device_list|.
  bool FoundDevice(const BluetoothAddress& device_id,
                   const BluetoothDeviceList& device_list) const;

  // Updates UI of the clicked bluetooth device to show it is being connected
  // or disconnected if such an operation is going to be performed underway.
  void UpdateClickedDevice(const BluetoothAddress& device_id,
                           HoverHighlightView* item_container);

  void ToggleButtonPressed();

  void ShowSettings();

  absl::optional<BluetoothAddress> GetFocusedDeviceAddress() const;
  void FocusDeviceByAddress(const BluetoothAddress& address) const;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void CreateExtraTitleRowButtons() override;

  // TODO(jamescook): Don't cache this.
  LoginStatus login_;

  std::unordered_map<HoverHighlightView*, BluetoothAddress> device_map_;

  BluetoothDeviceList connecting_devices_;
  BluetoothDeviceList paired_not_connected_devices_;

  views::ToggleButton* toggle_ = nullptr;
  views::Button* settings_ = nullptr;

  TriView* paired_devices_heading_ = nullptr;
  TriView* unpaired_devices_heading_ = nullptr;
  TrayInfoLabel* bluetooth_discovering_label_ = nullptr;

  // The container of the message "Bluetooth is disabled" and an icon. It should
  // be shown instead of Bluetooth device list when Bluetooth is disabled.
  views::View* disabled_panel_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_LEGACY_H_
