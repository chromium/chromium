// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_provider_touch.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ash {
namespace diagnostics {

InputDataProviderTouch::InputDataProviderTouch() {}
InputDataProviderTouch::~InputDataProviderTouch() {}

mojom::TouchDeviceInfoPtr InputDataProviderTouch::ConstructTouchDevice(
    int id,
    const ui::EventDeviceInfo* device_info,
    mojom::ConnectionType connection_type) {
  mojom::TouchDeviceInfoPtr result = mojom::TouchDeviceInfo::New();
  result->id = id;
  result->connection_type = connection_type;
  result->type = device_info->HasTouchpad() ? mojom::TouchDeviceType::kPointer
                                            : mojom::TouchDeviceType::kDirect;
  result->name = device_info->name();
  return result;
}

}  // namespace diagnostics
}  // namespace ash
