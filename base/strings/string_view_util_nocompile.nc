// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/strings/string_view_util.h"

#include "base/containers/span.h"

namespace base {

void AsStringViewNotBytes() {
  const int arr[] = {1, 2, 3};
  as_string_view(span(arr));  // expected-error@*:* {{no matching function for call to 'as_string_view'}}
}

}  // namespace base
