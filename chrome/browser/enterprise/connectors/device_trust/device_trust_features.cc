// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

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

BASE_FEATURE(kDeviceTrustConnectorAndroid, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeviceTrustConnectorDesktopFormFactors,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDeviceTrustConnectorAndroidEnabled() {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/575835298): Remove `kDeviceTrustConnectorDesktopFormFactors` once
  // the Device Trust Connector launches on desktop form factors.
  if (base::android::device_info::is_desktop() &&
      !base::FeatureList::IsEnabled(kDeviceTrustConnectorDesktopFormFactors)) {
    return false;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(kDeviceTrustConnectorAndroid);
}

BASE_FEATURE(kDeviceTrustAndroidAttestationTokens,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsDeviceTrustAndroidAttestationTokensEnabled() {
  return base::FeatureList::IsEnabled(kDeviceTrustAndroidAttestationTokens);
}

}  // namespace enterprise_connectors
