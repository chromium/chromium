// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_features.h"

#include "base/feature_list.h"

namespace enterprise_data_protection {

BASE_FEATURE(kEnableSinglePageAppDataProtection,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableForceDownloadToCloud, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableVerdictCache, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kVerdictCacheMaxSize,
                   &kEnableVerdictCache,
                   "verdict_cache_max_size",
                   /*default_value=*/200);

}  // namespace enterprise_data_protection
