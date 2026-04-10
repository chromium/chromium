// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_TEST_RESULT_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_TEST_RESULT_H_

#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"

template <typename T = void>
using TestResult = base::expected<T, std::string>;

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_TEST_RESULT_H_
