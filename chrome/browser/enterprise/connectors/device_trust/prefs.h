// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_PREFS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_PREFS_H_

#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"

namespace enterprise_connectors {

// Pref that maps to the "ContextAwareAccessSignalsAllowlistPref" policy.
extern const char kContextAwareAccessSignalsAllowlistPref[];

// Registers the device trust connectors profile preferences.
void RegisterDeviceTrustConnectorProfilePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_PREFS_H_
