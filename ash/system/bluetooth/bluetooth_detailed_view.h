// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_

#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "ui/gfx/vector_icon_types.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class BluetoothDeviceListItemView;
class TriView;

namespace tray {

// This class defines both the interface used to interact with the detailed
// Bluetooth page within the quick settings, including the view responsible for
// containing the device list, and the delegate interface this class uses to
// propagate user interactions with this class.
class BluetoothDetailedView {
 public:
  // This class defines the interface that BluetoothDetailedView will use to
  // propagate user interactions.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual void OnToggleClicked(bool new_state) = 0;
    virtual void OnPairNewDeviceRequested() = 0;
    virtual void OnDeviceListItemSelected(
        const chromeos::bluetooth_config::mojom::
            PairedBluetoothDevicePropertiesPtr& device) = 0;
  };

  BluetoothDetailedView(const BluetoothDetailedView&) = delete;
  BluetoothDetailedView& operator=(const BluetoothDetailedView&) = delete;
  virtual ~BluetoothDetailedView() = default;

  // Updates the detailed view to reflect a Bluetooth state of |enabled|.
  virtual void SetBluetoothToggleState(bool enabled) = 0;

  // Creates a targetable row for a single device within the device list. The
  // client is expected to configure the returned view themselves, and to use
  // the returned pointer for removing and rearranging the row.
  virtual BluetoothDeviceListItemView* AddDeviceListItem() = 0;

  // Adds a sticky sub-header to the end of the device list containing |icon|
  // and text represented by the |text_id| resource ID. The client is expected
  // to use the returned pointer for removing and rearranging the sub-header.
  virtual ash::TriView* AddDeviceListSubHeader(const gfx::VectorIcon& icon,
                                               int text_id) = 0;

  // Notifies that the device list has changed and the layout is invalid.
  virtual void NotifyDeviceListChanged() = 0;

  // Returns the device list.
  virtual views::View* device_list() = 0;

 protected:
  explicit BluetoothDetailedView(Delegate* delegate);

  Delegate* delegate() { return delegate_; }

 private:
  Delegate* delegate_;
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_
