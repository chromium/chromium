// Copyright 2021 The Chromium Authors
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

// Returns true if the sharing hub page action icon should exist in the location
// bar for some combination of |context|, |is_popup_mode|, and platform (from
// build flag). Note: having the page action does not necessarily mean that the
// sharing hub is enabled, e.g. ChromeOS uses the sharing hub page action icon
// to open the native sharesheet.
bool HasPageAction(content::BrowserContext* context, bool is_popup_mode);

// Returns true if the omnibox sharing hub is enabled for |context|. Only for
// Windows/Mac/Linux. ChromeOS opens the native sharesheet.
bool SharingHubOmniboxEnabled(content::BrowserContext* context);

// Returns true if the desktop screenshots feature is enabled.
// This feature is only accessed through the sharing hub. It allows the user to
// select and capture a region of a page, and optionally to edit it with an
// image editor before sharing.
bool DesktopScreenshotsFeatureEnabled(content::BrowserContext* context);

// Returns whether sharing is disabled altogether, in which case entry points
// for sharing should be hidden.
bool SharingIsDisabledByPolicy(content::BrowserContext* context);

// Feature flag to enable the screenshots feature, currently accessed only
// through the sharing hub.
BASE_DECLARE_FEATURE(kDesktopScreenshots);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
void RegisterProfilePrefs(PrefRegistrySimple* registry);
#endif

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_SHARING_HUB_SHARING_HUB_FEATURES_H_
