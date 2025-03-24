// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_is_test.h"

#include "base/base_export.h"

namespace {
bool g_this_is_a_test = false;
}

namespace base::internal {
bool get_is_test_impl() {
  return g_this_is_a_test;
}
}  // namespace base::internal

namespace base::test {
// base/test/allow_check_is_test_for_testing.h declares
// `AllowCheckIsTestForTesting`, but is only allowed to be included in test
// code. We therefore have to also mark the symbol as exported here.
BASE_EXPORT void AllowCheckIsTestForTesting() {
  g_this_is_a_test = true;
}
}  // namespace base::test
