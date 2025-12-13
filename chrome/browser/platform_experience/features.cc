// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/features.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

namespace platform_experience::features {

BASE_FEATURE(kLoadLowEngagementPEHFeaturesToPrefs,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisablePEHNotifications, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShouldUseSpecificPEHNotificationText,
             base::FEATURE_DISABLED_BY_DEFAULT);

void ActivateFieldTrials() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return;
  }

  const base::FilePath sentinel_path =
      user_data_dir.Append(FILE_PATH_LITERAL("PlatformExperienceHelper"))
          .Append(FILE_PATH_LITERAL("LoadFeatures"));

  if (base::PathExists(sentinel_path)) {
    // Querying the features will activate them.
    base::FeatureList::IsEnabled(kShouldUseSpecificPEHNotificationText);
    base::FeatureList::IsEnabled(kDisablePEHNotifications);
  }
}

}  // namespace platform_experience::features
