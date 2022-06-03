// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/telemetry_extension_ui/services/convert_ptr.h"

#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// TODO(https://crbug.com/1164001): Remove if cros_healthd::mojom moved to ash.
namespace cros_healthd {
namespace mojom = ::chromeos::cros_healthd::mojom;
}  // namespace cros_healthd

namespace converters {

// Tests that |ConvertPtr| function returns nullptr if input is nullptr.
// ConvertPtr is a template, so we can test this function with any valid type.
TEST(TelemetryConvertPtr, ConvertPtrTakesNullPtr) {
  EXPECT_TRUE(ConvertPtr(cros_healthd::mojom::ProbeErrorPtr()).is_null());
}

}  // namespace converters
}  // namespace ash
