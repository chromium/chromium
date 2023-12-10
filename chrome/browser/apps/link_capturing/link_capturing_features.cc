// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"

namespace apps::features {

// TODO(crbug.com/1357905): Remove feature on ChromeOS once all tests pass with
// updated UI.
#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kLinkCapturingUiUpdate,
             "LinkCapturingUiUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kAppToAppLinkCapturing,
             "AppToAppLinkCapturing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppToAppLinkCapturingWorkspaceApps,
             "AppToAppLinkCapturingWorkspaceApps",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

bool ShouldShowLinkCapturingUX() {
#if BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(kLinkCapturingUiUpdate);
#else
  return base::FeatureList::IsEnabled(::features::kDesktopPWAsLinkCapturing);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace apps::features
