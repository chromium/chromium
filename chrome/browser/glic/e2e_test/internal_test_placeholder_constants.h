// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_E2E_TEST_INTERNAL_TEST_PLACEHOLDER_CONSTANTS_H_
#define CHROME_BROWSER_GLIC_E2E_TEST_INTERNAL_TEST_PLACEHOLDER_CONSTANTS_H_

// Fake placeholder file in place of the internal test constants file, when
// internal test is not enabled.

#include <string>
#include <vector>

namespace glic::test {

inline constexpr char kAllowedHostAndPathForWpr[] = "";
inline constexpr char kTestAccountLabel[] = "";
inline constexpr char kTestActorAccountLabel[] = "";
auto kWprArguments = std::vector<std::string>{};

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_E2E_TEST_INTERNAL_TEST_PLACEHOLDER_CONSTANTS_H_
