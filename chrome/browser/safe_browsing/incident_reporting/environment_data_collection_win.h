// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_ENVIRONMENT_DATA_COLLECTION_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_ENVIRONMENT_DATA_COLLECTION_WIN_H_

#include <windows.h>

#include <stddef.h>

#include "base/containers/span.h"

namespace google {
namespace protobuf {
template <typename T>
class RepeatedPtrField;
}
}

namespace safe_browsing {

class ClientIncidentReport_EnvironmentData_OS;
class ClientIncidentReport_EnvironmentData_OS_RegistryKey;
class ClientIncidentReport_EnvironmentData_Process;

// Datatype for storing information about the registry keys from which to
// collect data.
struct RegistryKeyInfo {
  HKEY rootkey;
  const wchar_t* subkey;
};

// Collects then populates |process| with the sanitized paths of all DLLs
// loaded in the current process. Return false if an error occurred while
// querying for the loaded dlls.
bool CollectDlls(ClientIncidentReport_EnvironmentData_Process* process);

// For each of the dlls in this already populated incident report,
// check one of them is a registered LSP.
void RecordLspFeature(ClientIncidentReport_EnvironmentData_Process* process);

// Checks each module in the provided list for modifications and records these,
// along with any modified exports, in |process|.
void CollectModuleVerificationData(
    base::span<const wchar_t* const> modules_to_verify,
    ClientIncidentReport_EnvironmentData_Process* process);

// Populates |process| with the dll names that have been added to the chrome elf
// blocklist through the Windows registry.
void CollectDllBlocklistData(
    ClientIncidentReport_EnvironmentData_Process* process);

// Populates |key_data| with the data in the registry keys specified. In case of
// error, this data may be incomplete.
void CollectRegistryData(
    base::span<const RegistryKeyInfo> keys_to_collect,
    google::protobuf::RepeatedPtrField<
        ClientIncidentReport_EnvironmentData_OS_RegistryKey>* key_data);

// Populates |os_data| with information about the machine's domain enrollment
// status.
void CollectDomainEnrollmentData(
    ClientIncidentReport_EnvironmentData_OS* os_data);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_ENVIRONMENT_DATA_COLLECTION_WIN_H_
