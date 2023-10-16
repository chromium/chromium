// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURES_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURES_H_

#include "base/feature_list.h"

namespace apps::features {

BASE_DECLARE_FEATURE(kLinkCapturingUiUpdate);

// Enables user link capturing on desktop platforms, i.e. Windows, Mac
// Linux amd Fuchsia.
BASE_DECLARE_FEATURE(kDesktopPWAsLinkCapturing);

#if BUILDFLAG(IS_CHROMEOS)
// Enables link capturing into apps when links are clicked from another app
// context, regardless of user setting.
BASE_DECLARE_FEATURE(kAppToAppLinkCapturing);

// Enables link capturing into a specific set of Google Workspace apps when
// links are clicked from another app context, regardless of user setting.
BASE_DECLARE_FEATURE(kAppToAppLinkCapturingWorkspaceApps);
#endif

// Returns true if the overall link capturing UI update feature is enabled.
bool LinkCapturingUiUpdateEnabled();

}  // namespace apps::features

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURES_H_
