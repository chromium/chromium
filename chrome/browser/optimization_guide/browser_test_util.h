// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_BROWSER_TEST_UTIL_H_

#include <string>

namespace base {
class HistogramTester;
}  // namespace base

namespace optimization_guide {

// Retries fetching |histogram_name| until it contains at least |count| samples.
// Returns the count of samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count);

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_BROWSER_TEST_UTIL_H_
