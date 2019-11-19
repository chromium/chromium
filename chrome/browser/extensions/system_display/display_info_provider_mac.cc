// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_mac.h"

#include "base/logging.h"
#include "chrome/browser/extensions/system_display/display_info_provider.h"

namespace extensions {

DisplayInfoProviderMac::DisplayInfoProviderMac() = default;

void DisplayInfoProviderMac::UpdateDisplayUnitInfoForPlatform(
    const display::Display& display,
    extensions::api::system_display::DisplayUnitInfo* unit) {
  NOTIMPLEMENTED_LOG_ONCE();
}

std::unique_ptr<DisplayInfoProvider> CreateChromeDisplayInfoProvider() {
  return std::make_unique<DisplayInfoProviderMac>();
}

}  // namespace extensions
