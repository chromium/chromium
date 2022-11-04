// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input/input_data_provider_touch.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_provider.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ash {
namespace diagnostics {

InputDataProviderTouch::InputDataProviderTouch() {}
InputDataProviderTouch::~InputDataProviderTouch() {}

mojom::TouchDeviceInfoPtr InputDataProviderTouch::ConstructTouchDevice(
    const InputDeviceInformation* device_info,
    bool is_internal_display_on) {
  mojom::TouchDeviceInfoPtr result = mojom::TouchDeviceInfo::New();

  result->id = device_info->evdev_id;
  result->connection_type = device_info->connection_type;

  // TODO(crbug.com/1207678): double-check logic
  result->type = device_info->event_device_info.HasTouchpad()
                     ? mojom::TouchDeviceType::kPointer
                     : mojom::TouchDeviceType::kDirect;
  result->name = device_info->event_device_info.name();
  result->testable = true;

  // If the device is internal touchscreen, we check its initial testability
  // by looking at the internal display current power state.
  if (result->type == mojom::TouchDeviceType::kDirect &&
      result->connection_type == mojom::ConnectionType::kInternal) {
    result->testable = is_internal_display_on;
  }

  return result;
}

}  // namespace diagnostics
}  // namespace ash
