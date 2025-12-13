// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_features.h"

namespace default_browser {

bool IsDefaultBrowserFrameworkEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserFramework);
}

BASE_FEATURE(kDefaultBrowserFramework, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPerformDefaultBrowserCheckValidations,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace default_browser
