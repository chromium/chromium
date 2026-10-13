// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace enterprise_data_protection {

BASE_FEATURE(kEnableForceDownloadToCloud, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableTabSharingProtection, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnterpriseTabTitleReporting,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kEnableForceSaveToCloudDeferredReporting,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace enterprise_data_protection
