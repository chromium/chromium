// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/debug/crash_logging.h"

#if defined(NCTEST_SCOPED_CRASH_KEY_BOOL_ON_NON_BOOL_ARG)  // [r"static assertion failed due to requirement 'std::is_same_v<int, bool>': SCOPED_CRASH_KEY_BOOL must be passed a boolean value\."]

void WontCompile() {
  SCOPED_CRASH_KEY_BOOL(category, name, 1);
}

#endif
