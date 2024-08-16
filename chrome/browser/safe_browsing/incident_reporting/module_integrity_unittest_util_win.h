// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_INTEGRITY_UNITTEST_UTIL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_INTEGRITY_UNITTEST_UTIL_WIN_H_

#include <stddef.h>

#include <array>

namespace safe_browsing {

// The test dlls used by module_integrity_verifier_win_unittest.cc and
// environment_data_collection_win_unittest.cc.  The tests assume there exists
// at least one entry.
constexpr auto kTestDllNames = std::to_array({
    L"verifier_test_dll_1.dll",
    L"verifier_test_dll_2.dll",
});

// A function exported by the test dlls in |kTestDllNames|.
extern const char kTestExportName[];

// The DllMain function exported by the test dlls in |kTestDllNames|.
extern const char kTestDllMainExportName[];

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_INTEGRITY_UNITTEST_UTIL_WIN_H_
