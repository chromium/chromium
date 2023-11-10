// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/debug/crash_logging.h"

namespace base {

void NonBoolArg() {
  SCOPED_CRASH_KEY_BOOL("category", "name", 1);  // expected-error {{SCOPED_CRASH_KEY_BOOL must be passed a boolean value.}}
}

}  // namespace base
