// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "ui/gfx/vector_icon_types.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class BluetoothDeviceListItemView;
class DetailedViewDelegate;

// This class defines both the interface used to interact with the detailed
// Bluetooth page within the quick settings, including the view responsible for
// containing the device list. This class includes the declaration for the
// delegate interface it uses to propagate user interactions, and defines the
// factory used to create instances of implementations of this class.
class ASH_EXPORT BluetoothDetailedView {
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
        const bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr&
            device) = 0;
  };

  class Factory {
   public:
    Factory(const Factory&) = delete;
    const Factory& operator=(const Factory&) = delete;
    virtual ~Factory() = default;

    static std::unique_ptr<BluetoothDetailedView> Create(
        DetailedViewDelegate* detailed_view_delegate,
        Delegate* delegate);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    Factory() = default;

    virtual std::unique_ptr<BluetoothDetailedView> CreateForTesting(
        Delegate* delegate) = 0;
  };

  BluetoothDetailedView(const BluetoothDetailedView&) = delete;
  BluetoothDetailedView& operator=(const BluetoothDetailedView&) = delete;
  virtual ~BluetoothDetailedView() = default;

  // Returns the implementation casted to views::View*. This may be |nullptr|
  // when testing, where the implementation might not inherit from views::View.
  virtual views::View* GetAsView() = 0;

  // Updates the detailed view to reflect a Bluetooth state of |system_state|.
  virtual void UpdateBluetoothEnabledState(
      const bluetooth_config::mojom::BluetoothSystemState system_state) = 0;

  // Creates a targetable row for a single device within the device list. The
  // client is expected to configure the returned view themselves, and to use
  // the returned pointer for removing and rearranging the row.
  virtual BluetoothDeviceListItemView* AddDeviceListItem() = 0;

  // Adds a sticky sub-header to the end of the device list containing |icon|
  // and text represented by the |text_id| resource ID. The client is expected
  // to use the returned pointer for removing and rearranging the sub-header.
  virtual views::View* AddDeviceListSubHeader(const gfx::VectorIcon& icon,
                                              int text_id) = 0;

  // Notifies that the device list has changed and the layout is invalid.
  virtual void NotifyDeviceListChanged() = 0;

  // Returns the main content view which contains a list of child views that
  // make up the list of connected and previously connected bluetooth devices,
  // including their headers.
  virtual views::View* device_list() = 0;

 protected:
  explicit BluetoothDetailedView(Delegate* delegate);

  Delegate* delegate() { return delegate_; }

 private:
  raw_ptr<Delegate> delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DETAILED_VIEW_H_
