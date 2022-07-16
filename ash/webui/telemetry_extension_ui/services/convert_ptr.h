// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_CONVERT_PTR_H_
#define ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_CONVERT_PTR_H_

#include <utility>

#include "ash/webui/telemetry_extension_ui/services/diagnostics_service_converters.h"
#include "ash/webui/telemetry_extension_ui/services/probe_service_converters.h"

// To use ConvertPtr with other functions, headers to function definitions
// must be included in this file.

namespace ash {
namespace converters {

template <class InputT>
auto ConvertPtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : nullptr;
}

}  // namespace converters
}  // namespace ash

#endif  // ASH_WEBUI_TELEMETRY_EXTENSION_UI_SERVICES_CONVERT_PTR_H_
