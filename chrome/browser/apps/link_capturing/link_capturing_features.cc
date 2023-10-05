// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace apps::features {

// TODO(crbug.com/1357905): Remove feature on ChromeOS once all tests pass with
// updated UI.
BASE_FEATURE(kLinkCapturingUiUpdate,
             "LinkCapturingUiUpdate",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kLinkCapturingInfoBar,
             "LinkCapturingInfoBar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDesktopPWAsLinkCapturing,
             "DesktopPWAsLinkCapturing",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool LinkCapturingUiUpdateEnabled() {
  return base::FeatureList::IsEnabled(kLinkCapturingUiUpdate);
}

bool LinkCapturingInfoBarEnabled() {
  return LinkCapturingUiUpdateEnabled() &&
         base::FeatureList::IsEnabled(kLinkCapturingInfoBar);
}

}  // namespace apps::features
