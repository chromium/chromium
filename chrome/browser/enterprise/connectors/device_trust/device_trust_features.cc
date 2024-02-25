// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"

namespace enterprise_connectors {

BASE_FEATURE(kDTCKeyRotationEnabled,
             "DTCKeyRotationEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsKeyRotationEnabled() {
  return base::FeatureList::IsEnabled(kDTCKeyRotationEnabled);
}

}  // namespace enterprise_connectors
