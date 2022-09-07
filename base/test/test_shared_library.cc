// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/native_library_test_utils.h"

extern "C" {

int NATIVE_LIBRARY_TEST_ALWAYS_EXPORT GetExportedValue() {
  return g_native_library_exported_value;
}

void NATIVE_LIBRARY_TEST_ALWAYS_EXPORT SetExportedValue(int value) {
  g_native_library_exported_value = value;
}

// A test function used only to verify basic dynamic symbol resolution.
int NATIVE_LIBRARY_TEST_ALWAYS_EXPORT GetSimpleTestValue() {
  return 5;
}

// When called by |NativeLibraryTest.LoadLibraryPreferOwnSymbols|, this should
// forward to the local definition of NativeLibraryTestIncrement(), even though
// the test module also links in the native_library_test_utils source library
// which exports it.
int NATIVE_LIBRARY_TEST_ALWAYS_EXPORT GetIncrementValue() {
  return NativeLibraryTestIncrement();
}

}  // extern "C"
