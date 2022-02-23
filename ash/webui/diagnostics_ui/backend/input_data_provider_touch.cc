// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_provider_touch.h"
#include "ash/webui/diagnostics_ui/backend/input_data_provider.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ash {
namespace diagnostics {

InputDataProviderTouch::InputDataProviderTouch() {}
InputDataProviderTouch::~InputDataProviderTouch() {}

mojom::TouchDeviceInfoPtr InputDataProviderTouch::ConstructTouchDevice(
    const InputDeviceInformation* device_info) {
  mojom::TouchDeviceInfoPtr result = mojom::TouchDeviceInfo::New();

  result->id = device_info->evdev_id;
  result->connection_type = device_info->connection_type;

  // TODO(crbug.com/1207678): double-check logic
  result->type = device_info->event_device_info.HasTouchpad()
                     ? mojom::TouchDeviceType::kPointer
                     : mojom::TouchDeviceType::kDirect;
  result->name = device_info->event_device_info.name();
  return result;
}

}  // namespace diagnostics
}  // namespace ash
