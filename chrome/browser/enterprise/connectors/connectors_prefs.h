// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_PREFS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_PREFS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

class PrefRegistrySimple;

namespace enterprise_connectors {

// Pref that maps to the "SendDownloadToCloudEnterpriseConnector" policy.
extern const char kSendDownloadToCloudPref[];

// Pref that maps to the "OnFileAttachedEnterpriseConnector" policy.
extern const char kOnFileAttachedPref[];

// Pref that maps to the "OnFileDownloadedEnterpriseConnector" policy.
extern const char kOnFileDownloadedPref[];

// Pref that maps to the "OnBulkDataEntryEnterpriseConnector" policy.
extern const char kOnBulkDataEntryPref[];

// Pref that maps to the "OnPrintEnterpriseConnector" policy.
extern const char kOnPrintPref[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Pref that maps to the "OnFileTransferEnterpriseConnector" policy.
extern const char kOnFileTransferPref[];
#endif

// Pref that maps to the "OnSecurityEventEnterpriseConnector" policy.
extern const char kOnSecurityEventPref[];

// Pref that maps to the "ContextAwareAccessSignalsAllowlistPref" policy.
extern const char kContextAwareAccessSignalsAllowlistPref[];

// Prefs that map to the scope of each policy using a
// EnterpriseConnectorsPolicyHandler.
extern const char kOnFileAttachedScopePref[];
extern const char kOnFileDownloadedScopePref[];
extern const char kOnBulkDataEntryScopePref[];
extern const char kOnPrintScopePref[];
#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kOnFileTransferScopePref[];
#endif
extern const char kOnSecurityEventScopePref[];

#if BUILDFLAG(IS_MAC)
// The pref on whether the device trust key creation is disabled for the
// current user. The device trust key creation is disabled when a key for
// the device is already present on the Server but a key upload is
// requested with a another key not signed by the previous one. The key
// creation is enabled by default.
extern const char kDeviceTrustDisableKeyCreationPref[];
#endif

void RegisterProfilePrefs(PrefRegistrySimple* registry);

#if BUILDFLAG(IS_MAC)
void RegisterLocalPrefs(PrefRegistrySimple* registry);
#endif

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_PREFS_H_
