// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/native_library_test_utils.h"

namespace {

int g_static_value = 0;

}  // namespace

extern "C" {

int g_native_library_exported_value = 0;

int NativeLibraryTestIncrement() { return ++g_static_value; }

}  // extern "C"
