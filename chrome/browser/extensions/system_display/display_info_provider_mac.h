// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_MAC_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_MAC_H_

#include "extensions/browser/api/system_display/display_info_provider.h"

namespace extensions {

class DisplayInfoProviderMac : public DisplayInfoProvider {
 public:
  DisplayInfoProviderMac();

  DisplayInfoProviderMac(const DisplayInfoProviderMac&) = delete;
  DisplayInfoProviderMac& operator=(const DisplayInfoProviderMac&) = delete;

  // DisplayInfoProvider implementation.
  void UpdateDisplayUnitInfoForPlatform(
      const display::Display& display,
      api::system_display::DisplayUnitInfo* unit) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_MAC_H_
