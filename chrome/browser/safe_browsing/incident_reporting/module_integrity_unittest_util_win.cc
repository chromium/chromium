// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_unittest_util_win.h"

#include "base/cxx17_backports.h"

namespace safe_browsing {

const wchar_t* const kTestDllNames[] = {
    L"verifier_test_dll_1.dll",
    L"verifier_test_dll_2.dll",
};

const size_t kTestDllNamesCount = base::size(kTestDllNames);

const char kTestExportName[] = "DummyExport";

const char kTestDllMainExportName[] = "DllMain";

}  // namespace safe_browsing
