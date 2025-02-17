// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_MAC_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_MAC_H_

#include "extensions/browser/display_info_provider_base.h"

namespace extensions {

class DisplayInfoProviderMac : public DisplayInfoProviderBase {
 public:
  DisplayInfoProviderMac();

  DisplayInfoProviderMac(const DisplayInfoProviderMac&) = delete;
  DisplayInfoProviderMac& operator=(const DisplayInfoProviderMac&) = delete;

  // DisplayInfoProvider implementation.
  void UpdateDisplayUnitInfoForPlatform(
      const std::vector<display::Display>& display,
      DisplayUnitInfoList& unit) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_MAC_H_
