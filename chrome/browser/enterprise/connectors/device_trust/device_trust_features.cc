// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"

namespace enterprise_connectors {

BASE_FEATURE(kDTCKeyRotationEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsKeyRotationEnabled() {
  return base::FeatureList::IsEnabled(kDTCKeyRotationEnabled);
}

BASE_FEATURE(kDTCKeyUploadedBySharedAPIEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsDTCKeyUploadedBySharedAPI() {
  return base::FeatureList::IsEnabled(kDTCKeyUploadedBySharedAPIEnabled);
}

BASE_FEATURE(kDTCKeyRotationUploadedBySharedAPIEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsDTCKeyRotationUploadedBySharedAPI() {
  return base::FeatureList::IsEnabled(
      kDTCKeyRotationUploadedBySharedAPIEnabled);
}

BASE_FEATURE(kDTCAntivirusSignalEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsDTCAntivirusSignalEnabled() {
  return base::FeatureList::IsEnabled(kDTCAntivirusSignalEnabled);
}

}  // namespace enterprise_connectors
