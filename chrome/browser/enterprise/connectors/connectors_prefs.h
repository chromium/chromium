// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_PREFS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_PREFS_H_

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

// Pref that maps to the "OnSecurityEventEnterpriseConnector" policy.
extern const char kOnSecurityEventPref[];

// Pref that maps to the "ContextAwareAccessSignalsAllowlistPref" policy.
extern const char kContextAwareAccessSignalsAllowlistPref[];

// Prefs that map to the scope of each policy using a
// EnterpriseConnectorsPolicyHandler.
extern const char kOnFileAttachedScopePref[];
extern const char kOnFileDownloadedScopePref[];
extern const char kOnBulkDataEntryScopePref[];
extern const char kOnSecurityEventScopePref[];

// The pref name where this class stores the encrypted private key.
// If the machine supports storage in TPM, the private key will be
// stored there; otherwise, it will be stored in the local state.
extern const char kDeviceTrustPrivateKeyPref[];
// The pref name where this class stores the public key;
// If the machine supports storage in TPM, the public key will be
// stored there; owtherwise, it will be stored in the local state.
extern const char kDeviceTrustPublicKeyPref[];

// Template to store the service provider's "folder_id" for caching purposes.
extern const char kFileSystemUploadFolderIdPref[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

void RegisterLocalPrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_PREFS_H_
