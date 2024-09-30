// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_controller_impl.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view.h"

namespace ash {

namespace {

using bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;

// Helper function to remove |*view| from its view hierarchy, delete the view,
// and reset the value of |*view| to be |nullptr|.
template <class T>
void RemoveAndResetViewIfExists(raw_ptr<T>* view) {
  DCHECK(view);

  if (!*view)
    return;

  views::View* parent = (*view)->parent();

  if (parent) {
    parent->RemoveChildViewT(view->ExtractAsDangling());
  }
}

}  // namespace

BluetoothDeviceListControllerImpl::BluetoothDeviceListControllerImpl(
    BluetoothDetailedView* bluetooth_detailed_view)
    : bluetooth_detailed_view_(bluetooth_detailed_view) {}

BluetoothDeviceListControllerImpl::~BluetoothDeviceListControllerImpl() =
    default;

void BluetoothDeviceListControllerImpl::UpdateBluetoothEnabledState(
    bool enabled) {
  if (is_bluetooth_enabled_ && !enabled) {
    device_id_to_view_map_.clear();
    connected_sub_header_ = nullptr;
    no_device_connected_sub_header_ = nullptr;
    previously_connected_sub_header_ = nullptr;
    bluetooth_detailed_view_->device_list()->RemoveAllChildViews();
  }
  is_bluetooth_enabled_ = enabled;
}

void BluetoothDeviceListControllerImpl::UpdateDeviceList(
    const PairedBluetoothDevicePropertiesPtrs& connected,
    const PairedBluetoothDevicePropertiesPtrs& previously_connected) {
  DCHECK(is_bluetooth_enabled_);

  // This function will create views for new devices, re-use views for existing
  // devices, and remove views for devices that no longer exist. To do this, we
  // keep track of all the preexisting views in |previous_views|, removing a
  // view from this map when the corresponding device is found in |connected| or
  // |previously_connected|. Before returning, any view remaining in
  // |previous_views| is no longer needed and is deleted.
  base::flat_map<std::string,
                 raw_ptr<BluetoothDeviceListItemView, CtnExperimental>>
      previous_views = std::move(device_id_to_view_map_);
  device_id_to_view_map_.clear();

  // Since we re-use views when possible, we need to re-order them to match the
  // order of the devices we are provided with. We use |index| to keep track of
  // the next index within the device list where a view should be placed, i.e.
  // all views before |index| are in their final position.
  size_t index = 0;

  // The list of connected devices.
  if (!connected.empty()) {
    connected_sub_header_ = CreateSubHeaderIfMissingAndReorder(
        connected_sub_header_,
        IDS_ASH_STATUS_TRAY_BLUETOOTH_CURRENTLY_CONNECTED_DEVICES, index);

    // Increment |index| since this position was taken by
    // |connected_sub_header_|.
    index++;

    index = CreateViewsIfMissingAndReorder(connected, &previous_views, index);
  } else {
    RemoveAndResetViewIfExists(&connected_sub_header_);
  }

  // The previously connected devices.
  if (!previously_connected.empty()) {
    previously_connected_sub_header_ = CreateSubHeaderIfMissingAndReorder(
        previously_connected_sub_header_,
        IDS_ASH_STATUS_TRAY_BLUETOOTH_PREVIOUSLY_CONNECTED_DEVICES, index);

    // Increment |index| since this position was taken by
    // |previously_connected_sub_header_|.
    index++;

    // Ignore the returned index since we are now done re-ordering the list.
    CreateViewsIfMissingAndReorder(previously_connected, &previous_views,
                                   index);
  } else {
    RemoveAndResetViewIfExists(&previously_connected_sub_header_);
  }

  // The header when there are no connected or previously connected devices.
  if (device_id_to_view_map_.empty()) {
    no_device_connected_sub_header_ = CreateSubHeaderIfMissingAndReorder(
        no_device_connected_sub_header_,
        IDS_ASH_STATUS_TRAY_BLUETOOTH_NO_DEVICE_CONNECTED, index);
  } else {
    RemoveAndResetViewIfExists(&no_device_connected_sub_header_);
  }

  for (const auto& id_and_view : previous_views) {
    bluetooth_detailed_view_->device_list()->RemoveChildViewT(
        id_and_view.second);
  }
  bluetooth_detailed_view_->NotifyDeviceListChanged();
}

views::View*
BluetoothDeviceListControllerImpl::CreateSubHeaderIfMissingAndReorder(
    views::View* sub_header,
    int text_id,
    size_t index) {
  if (!sub_header) {
    sub_header = bluetooth_detailed_view_->AddDeviceListSubHeader(
        gfx::kNoneIcon, text_id);
  }
  bluetooth_detailed_view_->device_list()->ReorderChildView(sub_header, index);
  return sub_header;
}

size_t BluetoothDeviceListControllerImpl::CreateViewsIfMissingAndReorder(
    const PairedBluetoothDevicePropertiesPtrs& device_property_list,
    base::flat_map<std::string,
                   raw_ptr<BluetoothDeviceListItemView, CtnExperimental>>*
        previous_views,
    size_t index) {
  DCHECK(previous_views);

  BluetoothDeviceListItemView* device_view = nullptr;

  const size_t device_count = device_property_list.size();

  for (size_t i = 0; i < device_count; ++i) {
    const PairedBluetoothDevicePropertiesPtr& device_properties =
        device_property_list.at(i);
    const std::string& device_id = device_properties->device_properties->id;
    auto it = previous_views->find(device_id);

    if (it == previous_views->end()) {
      device_view = bluetooth_detailed_view_->AddDeviceListItem();
    } else {
      device_view = it->second;
      previous_views->erase(it);
    }
    device_id_to_view_map_.emplace(device_id, device_view);

    device_view->UpdateDeviceProperties(i, device_count, device_properties);
    bluetooth_detailed_view_->device_list()->ReorderChildView(device_view,
                                                              index);

    // Increment |index| since this position was taken by |device_view|.
    index++;
  }
  return index;
}

}  // namespace ash
