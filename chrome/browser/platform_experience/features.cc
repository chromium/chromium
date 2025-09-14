// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/features.h"

#include "base/feature_list.h"

namespace platform_experience::features {

BASE_FEATURE(kLoadLowEngagementPEHFeaturesToPrefs,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisablePEHNotifications, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShouldUseSpecificPEHNotificationText,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace platform_experience::features
