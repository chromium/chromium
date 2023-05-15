// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"

namespace enterprise_connectors {

BASE_FEATURE(kDeviceTrustConnectorEnabled,
             "DeviceTrustConnectorEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUserDTCInlineFlowEnabled,
             "UserDTCInlineFlowEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDeviceTrustConnectorFeatureEnabled() {
  return base::FeatureList::IsEnabled(kDeviceTrustConnectorEnabled);
}

bool IsUserInlineFlowFeatureEnabled() {
  return IsDeviceTrustConnectorFeatureEnabled() &&
         base::FeatureList::IsEnabled(kUserDTCInlineFlowEnabled);
}

}  // namespace enterprise_connectors
