// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_unittest_util_win.h"

#include <iterator>

namespace safe_browsing {

const wchar_t* const kTestDllNames[] = {
    L"verifier_test_dll_1.dll",
    L"verifier_test_dll_2.dll",
};

const size_t kTestDllNamesCount = std::size(kTestDllNames);

const char kTestExportName[] = "DummyExport";

const char kTestDllMainExportName[] = "DllMain";

}  // namespace safe_browsing
