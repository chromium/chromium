// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_features.h"

#include "base/feature_list.h"

namespace app_list_features {

BASE_FEATURE(kEnableAppListLaunchRecording, base::FEATURE_DISABLED_BY_DEFAULT);
// DO NOT REMOVE: Tast integration tests use this feature. (See crbug/1340267)
BASE_FEATURE(kForceShowContinueSection, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAppListLaunchRecordingEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppListLaunchRecording);
}

bool IsForceShowContinueSectionEnabled() {
  return base::FeatureList::IsEnabled(kForceShowContinueSection);
}

}  // namespace app_list_features
