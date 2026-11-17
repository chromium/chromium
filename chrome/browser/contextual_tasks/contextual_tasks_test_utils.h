// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TEST_UTILS_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TEST_UTILS_H_

#include "components/contextual_tasks/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

// Skips the current test if the device's form factor doesn't support
// Contextual Tasks UI. Must be used directly in SetUp() or the test body,
// because GTEST_SKIP() returns from the enclosing function.
#define SKIP_IF_CONTEXTUAL_TASKS_UNSUPPORTED()                    \
  if (!contextual_tasks::IsContextualTasksFormFactorSupported()) \
  GTEST_SKIP() << "Contextual Tasks UI isn't supported on this form factor."

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TEST_UTILS_H_
