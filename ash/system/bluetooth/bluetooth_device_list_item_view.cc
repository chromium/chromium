// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_device_list_item_view.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "mojo/public/cpp/bindings/clone_traits.h"

namespace ash {
namespace {
using chromeos::bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr;
}  // namespace

BluetoothDeviceListItemView::BluetoothDeviceListItemView(
    ViewClickListener* listener)
    : HoverHighlightView(listener) {
  DCHECK(ash::features::IsBluetoothRevampEnabled());
}

BluetoothDeviceListItemView::~BluetoothDeviceListItemView() = default;

void BluetoothDeviceListItemView::UpdateDeviceProperties(
    const PairedBluetoothDevicePropertiesPtr& device_properties) {
  device_properties_ = mojo::Clone(device_properties);
}

}  // namespace ash
