// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_connectors {

// Controls whether the Device Trust connector client code is enabled or not.
BASE_DECLARE_FEATURE(kDeviceTrustConnectorEnabled);

// Return true if the device trust connector Finch feature is enabled.
bool IsDeviceTrustConnectorFeatureEnabled();

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_FEATURES_H_
