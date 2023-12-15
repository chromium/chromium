// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metadata.h"

#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/containers/fixed_flat_map.h"

namespace ash {

namespace {
static constexpr auto kMouseMetadata =
    base::MakeFixedFlatMap<VendorProductId, MouseMetadata>({
        {{0xffff, 0xffff},
         {mojom::CustomizationRestriction::kAllowCustomizations}},  // Fake data
                                                                    // for
                                                                    // testing.
    });
}

bool MouseMetadata::operator==(const MouseMetadata& other) const {
  return customization_restriction == other.customization_restriction;
}

const MouseMetadata* GetMouseMetadata(const ui::InputDevice& device) {
  const auto* iter = kMouseMetadata.find({device.vendor_id, device.product_id});
  if (iter != kMouseMetadata.end()) {
    return &(iter->second);
  }
  return nullptr;
}

}  // namespace ash
