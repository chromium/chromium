// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a no-compile test suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/types/optional_util.h"

#include <optional>

namespace base {

void OptionalToPtrLifetime() {
  [[maybe_unused]] const int* ptr = OptionalToPtr(std::optional<int>(1));  // expected-error {{temporary whose address is used as value of local variable 'ptr' will be destroyed at the end of the full-expression}}
}

}  // namespace base
