// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_HUB_SHARING_HUB_FEATURES_H_
#define CHROME_BROWSER_SHARING_HUB_SHARING_HUB_FEATURES_H_

#include "base/feature_list.h"

namespace sharing_hub {

// Feature flag to enable the 3-dot menu entry point for the desktop sharing
// hub.
extern const base::Feature kSharingHubDesktopAppMenu;

// Feature flag to enable the omnibox entry point for the desktop sharing hub.
extern const base::Feature kSharingHubDesktopOmnibox;

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_SHARING_HUB_SHARING_HUB_FEATURES_H_
