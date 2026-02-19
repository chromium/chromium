// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_ANDROID_H_

#include "extensions/browser/display_info_provider_base.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class DisplayInfoProviderAndroid : public DisplayInfoProviderBase {
 public:
  DisplayInfoProviderAndroid();
  DisplayInfoProviderAndroid(const DisplayInfoProviderAndroid&) = delete;
  DisplayInfoProviderAndroid& operator=(const DisplayInfoProviderAndroid&) =
      delete;
  ~DisplayInfoProviderAndroid() override;

 private:
  void UpdateDisplayUnitInfoForPlatform(
      const std::vector<display::Display>& displays,
      DisplayUnitInfoList& units) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_ANDROID_H_
