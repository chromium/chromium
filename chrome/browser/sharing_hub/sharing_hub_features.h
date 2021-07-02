// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_HUB_SHARING_HUB_FEATURES_H_
#define CHROME_BROWSER_SHARING_HUB_SHARING_HUB_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

class PrefRegistrySimple;

namespace content {
class BrowserContext;
}

namespace sharing_hub {

// Returns true if the app menu sharing hub is enabled for |context|. Only for
// Windows/Mac/Linux.
bool SharingHubAppMenuEnabled(content::BrowserContext* context);

// Returns true if the omnibox sharing hub is enabled for |context|. Only for
// Windows/Mac/Linux.
bool SharingHubOmniboxEnabled(content::BrowserContext* context);

// Feature flag to enable the 3-dot menu entry point for the desktop sharing
// hub.
extern const base::Feature kSharingHubDesktopAppMenu;

// Feature flag to enable the omnibox entry point for the desktop sharing hub.
extern const base::Feature kSharingHubDesktopOmnibox;

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
void RegisterProfilePrefs(PrefRegistrySimple* registry);
#endif

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_SHARING_HUB_SHARING_HUB_FEATURES_H_
