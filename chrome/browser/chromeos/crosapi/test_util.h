// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_TEST_UTIL_H_

#include <string>

#include "base/containers/flat_set.h"

namespace crosapi {

namespace mojom {
class TestController;
}  // namespace mojom

// Provides access to the test setup's TestController in browsertests only.
// Can be used in both the Lacros and Ash processes.
// Must only be used for browser -> Ash-system communication; test code that
// uses this must also be run in Lacros.
mojom::TestController* GetTestController();

// Abstraction over testing crosapi::browser_util::GetAshCapabilities() that
// works in both the Lacros and Ash processes.
bool AshSupportsCapabilities(const base::flat_set<std::string>& capabilities);

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_TEST_UTIL_H_
