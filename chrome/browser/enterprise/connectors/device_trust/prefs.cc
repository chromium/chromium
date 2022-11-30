// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/prefs.h"

namespace enterprise_connectors {

const char kContextAwareAccessSignalsAllowlistPref[] =
    "enterprise_connectors.device_trust.origins";

#if BUILDFLAG(IS_MAC)
const char kDeviceTrustDisableKeyCreationPref[] =
    "enterprise_connectors.device_trust.disable_key_creation";
#endif

void RegisterDeviceTrustConnectorProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kContextAwareAccessSignalsAllowlistPref);
}

#if BUILDFLAG(IS_MAC)
void RegisterDeviceTrustConnectorLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kDeviceTrustDisableKeyCreationPref, false);
}
#endif

}  // namespace enterprise_connectors
