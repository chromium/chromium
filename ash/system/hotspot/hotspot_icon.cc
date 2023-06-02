// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_icon.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/system/hotspot/hotspot_icon_animation.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"

namespace ash {

using hotspot_config::mojom::HotspotState;

namespace {

constexpr int kNumEnablingImages = 3;

}  // namespace

namespace hotspot_icon {

const gfx::VectorIcon& GetIconForHotspot(const HotspotState& hotspot_state) {
  if (hotspot_state == HotspotState::kEnabling) {
    double animation = Shell::Get()->hotspot_icon_animation()->GetAnimation();
    int index =
        animation * nextafter(static_cast<float>(kNumEnablingImages), 0);
    index = std::clamp(index, 0, kNumEnablingImages - 1);
    if (index == 0) {
      return kHotspotDotIcon;
    }
    if (index == 1) {
      return kHotspotOneArcIcon;
    }
    return kHotspotOnIcon;
  }

  if (hotspot_state == HotspotState::kEnabled) {
    return kHotspotOnIcon;
  }
  return kHotspotOffIcon;
}

}  //  namespace hotspot_icon
}  //  namespace ash
