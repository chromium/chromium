// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_WIN_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_WIN_H_

#include "extensions/browser/api/system_display/display_info_provider.h"

namespace extensions {

class DisplayInfoProviderWin : public DisplayInfoProvider {
 public:
  DisplayInfoProviderWin();

  DisplayInfoProviderWin(const DisplayInfoProviderWin&) = delete;
  DisplayInfoProviderWin& operator=(const DisplayInfoProviderWin&) = delete;

  // DisplayInfoProvider implementation.
  void UpdateDisplayUnitInfoForPlatform(
      const std::vector<display::Display>& displays,
      DisplayUnitInfoList& units) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_WIN_H_
