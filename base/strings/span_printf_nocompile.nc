// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/strings/span_printf.h"

#include <utility>

void MismatchSpanprintf() {
  char buf[100];
  std::ignore = base::SpanPrintf(buf, "%s\n", 42);  // expected-error {{format specifies type 'char *' but the argument has type 'int'}}
}
