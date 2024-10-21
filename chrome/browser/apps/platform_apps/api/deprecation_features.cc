// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/deprecation_features.h"

namespace chrome_apps::features {

// Deprecates the Media Galleries Chrome App APIs.
BASE_FEATURE(kDeprecateMediaGalleriesApis,
             "DeprecateMediaGalleriesApis",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Deprecates the syncFileSystem Chrome App APIs.
BASE_FEATURE(kDeprecateSyncFileSystemApis,
             "kDeprecateSyncFileSystemApis",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace chrome_apps::features
