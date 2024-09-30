// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/bluetooth/bluetooth_device_list_controller.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class BluetoothDetailedView;
class BluetoothDeviceListItemView;

// BluetoothDeviceListController implementation.
class ASH_EXPORT BluetoothDeviceListControllerImpl
    : public BluetoothDeviceListController {
 public:
  explicit BluetoothDeviceListControllerImpl(
      BluetoothDetailedView* bluetooth_detailed_view);
  BluetoothDeviceListControllerImpl(const BluetoothDeviceListControllerImpl&) =
      delete;
  BluetoothDeviceListControllerImpl& operator=(
      const BluetoothDeviceListControllerImpl&) = delete;
  ~BluetoothDeviceListControllerImpl() override;

 private:
  friend class BluetoothDeviceListControllerTest;

  // BluetoothDeviceListController:
  void UpdateBluetoothEnabledState(bool enabled) override;
  void UpdateDeviceList(
      const PairedBluetoothDevicePropertiesPtrs& connected,
      const PairedBluetoothDevicePropertiesPtrs& previously_connected) override;

  // Creates a sub-header with text represented by the |text_id| message ID when
  // |sub_header| is |nullptr|, otherwise uses the provided |sub_header|. The
  // used sub-header is then moved to index |index| within the device list and
  // returned.
  views::View* CreateSubHeaderIfMissingAndReorder(views::View* sub_header,
                                                  int text_id,
                                                  size_t index);

  // Creates and initializes a view for each of the device properties within
  // |device_property_list| if a view does not already exist, otherwise re-using
  // the existing view to avoid disrupting a11y. Each view will be reordered to
  // start at |index| and will be removed from |previous_views|. The index of
  // the position after the final view that was added is returned.
  size_t CreateViewsIfMissingAndReorder(
      const PairedBluetoothDevicePropertiesPtrs& device_property_list,
      base::flat_map<std::string,
                     raw_ptr<BluetoothDeviceListItemView, CtnExperimental>>*
          previous_views,
      size_t index);

  const raw_ptr<BluetoothDetailedView> bluetooth_detailed_view_;

  bool is_bluetooth_enabled_ = false;
  base::flat_map<std::string,
                 raw_ptr<BluetoothDeviceListItemView, CtnExperimental>>
      device_id_to_view_map_;
  raw_ptr<views::View> connected_sub_header_ = nullptr;
  raw_ptr<views::View> no_device_connected_sub_header_ = nullptr;
  raw_ptr<views::View> previously_connected_sub_header_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_CONTROLLER_IMPL_H_
