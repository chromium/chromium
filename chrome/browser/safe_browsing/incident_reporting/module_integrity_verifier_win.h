// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_INTEGRITY_VERIFIER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_INTEGRITY_VERIFIER_WIN_H_

#include <stdint.h>

#include "base/containers/span.h"

namespace base {
namespace win {
class PEImage;
class PEImageAsData;
}  // namespace win
}  // namespace base

namespace safe_browsing {

class ClientIncidentReport_EnvironmentData_Process_ModuleState;

// Helper to grab the addresses and size of the code section of a PEImage.
// Returns two spans: one containing the code section of the dll loaded as a
// library, the other for the dll loaded as data.
bool GetCodeSpans(const base::win::PEImage& mem_peimage,
                  base::span<const uint8_t> disk_peimage,
                  base::span<const uint8_t>& mem_code_data,
                  base::span<const uint8_t>& disk_code_data);

// Examines the code section of the given module in memory and on disk, looking
// for unexpected differences and populating |module_state| in the process.
// Returns true if the entire image was scanned. |num_bytes_different| is
// populated with the number of differing bytes found, even if the scan failed
// to complete.
bool VerifyModule(
    const wchar_t* module_name,
    ClientIncidentReport_EnvironmentData_Process_ModuleState* module_state,
    int* num_bytes_different);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_INTEGRITY_VERIFIER_WIN_H_
