// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_aura.h"

#include "chrome/browser/extensions/system_display/display_info_provider.h"

namespace extensions {

DisplayInfoProviderAura::DisplayInfoProviderAura() = default;

std::unique_ptr<DisplayInfoProvider> CreateChromeDisplayInfoProvider() {
  return std::make_unique<DisplayInfoProviderAura>();
}

}  // namespace extensions
