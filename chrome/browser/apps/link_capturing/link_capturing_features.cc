// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"

namespace apps::features {

// TODO(crbug.com/40236806): Remove feature on ChromeOS once all tests pass with
// updated UI.
#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kLinkCapturingUiUpdate,
             "LinkCapturingUiUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

bool ShouldShowLinkCapturingUX() {
#if BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(kLinkCapturingUiUpdate);
#else
  return base::FeatureList::IsEnabled(::features::kPwaNavigationCapturing);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool IsNavigationCapturingReimplEnabled() {
  return base::FeatureList::IsEnabled(::features::kPwaNavigationCapturing) &&
         (::features::kNavigationCapturingDefaultState.Get() ==
              ::features::CapturingState::kReimplDefaultOn ||
          ::features::kNavigationCapturingDefaultState.Get() ==
              ::features::CapturingState::kReimplDefaultOff);
}

}  // namespace apps::features
