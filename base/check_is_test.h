// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CHECK_IS_TEST_H_
#define BASE_CHECK_IS_TEST_H_

#include "base/base_export.h"

// Code paths taken in tests are sometimes different from those taken in
// production. This might be because the respective tests do not initialize some
// objects that would be required for the "normal" code path.
//
// Ideally, such code constructs should be avoided, so that tests really test
// the production code and not something different.
//
// However, there already are hundreds of test-only paths in production code
// Cleaning up all these cases retroactively and completely avoiding such cases
// in the future seems unrealistic.
//
// Thus, it is useful to prevent the test code-only paths to be taken in
// production scenarios.
//
// `CHECK_IS_TEST` can be used to assert that a test-only path is actually taken
// only in tests. For instance:
//
//   // This only happens in unit tests:
//   if (!url_loader_factory)
//   {
//     // Assert that this code path is really only taken in tests.
//     CHECK_IS_TEST();
//     return;
//   }
//
// `CHECK_IS_TEST` is thread safe.

#define CHECK_IS_TEST() base::internal::check_is_test_impl()

namespace base::internal {
BASE_EXPORT void check_is_test_impl();
}  // namespace base::internal

#endif  // BASE_CHECK_IS_TEST_H_
