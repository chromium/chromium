// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURES_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURES_H_

#include "base/feature_list.h"

namespace apps::features {

BASE_DECLARE_FEATURE(kLinkCapturingUiUpdate);
BASE_DECLARE_FEATURE(kLinkCapturingInfoBar);

// Enables user link capturing on desktop platforms, i.e. Windows, Mac
// Linux amd Fuchsia.
BASE_DECLARE_FEATURE(kDesktopPWAsLinkCapturing);

// Returns true if the overall link capturing UI update feature is enabled.
bool LinkCapturingUiUpdateEnabled();

// Returns true if the Link Capturing Info Bar should be shown when launching an
// app through the Intent Picker. Only returns true if
// LinkCapturingUiUpdateEnabled() returns true.
bool LinkCapturingInfoBarEnabled();

}  // namespace apps::features

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURES_H_
