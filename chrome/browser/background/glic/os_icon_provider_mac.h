// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_GLIC_OS_ICON_PROVIDER_MAC_H_
#define CHROME_BROWSER_BACKGROUND_GLIC_OS_ICON_PROVIDER_MAC_H_

#include "ui/gfx/image/image_skia.h"

class PrefService;

namespace glic {

class GlicStatusIcon;

// Class for selecting an icon for the glic status tray icon on Mac.
class OSIconProviderMac {
 public:
  explicit OSIconProviderMac(PrefService& prefs,
                             GlicStatusIcon& glic_status_icon);
  ~OSIconProviderMac();
  gfx::ImageSkia GetIcon() const;
};

}  // namespace glic

#endif  // CHROME_BROWSER_BACKGROUND_GLIC_OS_ICON_PROVIDER_MAC_H_
