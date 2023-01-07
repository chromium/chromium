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

#if BUILDFLAG(IS_MAC)
// The pref on whether the device trust key creation is disabled for the
// current user. The device trust key creation is disabled when a key for
// the device is already present on the Server but a key upload is
// requested with a another key not signed by the previous one. The key
// creation is enabled by default.
extern const char kDeviceTrustDisableKeyCreationPref[];
#endif

// Registers the device trust connectors profile preferences.
void RegisterDeviceTrustConnectorProfilePrefs(PrefRegistrySimple* registry);

// Registers the device trust connectors local preferences.
#if BUILDFLAG(IS_MAC)
void RegisterDeviceTrustConnectorLocalPrefs(PrefRegistrySimple* registry);
#endif

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_PREFS_H_
