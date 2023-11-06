// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_connectors {

// Controls whether the key rotation flow, triggered by a remote command, is
// enabled or not.
BASE_DECLARE_FEATURE(kDTCKeyRotationEnabled);

// Return true if the key rotation flow is enabled.
bool IsKeyRotationEnabled();

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_FEATURES_H_
