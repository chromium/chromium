// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"

const base::Feature kSearchPrefetchService{"SearchPrefecthService",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

bool SearchPrefetchServiceIsEnabled() {
  return base::FeatureList::IsEnabled(kSearchPrefetchService);
}
