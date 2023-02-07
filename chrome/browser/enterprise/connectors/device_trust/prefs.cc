// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/prefs.h"

namespace enterprise_connectors {

const char kContextAwareAccessSignalsAllowlistPref[] =
    "enterprise_connectors.device_trust.origins";

const char kAutomatedKeyRotationEnabledPref[] =
    "enterprise_connectors.device_trust.automated_key_rotation_enabled";

void RegisterDeviceTrustConnectorProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kContextAwareAccessSignalsAllowlistPref);
  registry->RegisterStringPref(kAutomatedKeyRotationEnabledPref, "");
}

}  // namespace enterprise_connectors
