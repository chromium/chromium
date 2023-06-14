// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_TRUSTED_TYPES_TEST_UTIL_H_
#define ASH_WEBUI_COMMON_TRUSTED_TYPES_TEST_UTIL_H_

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class ToRenderFrameHost;
}

namespace ash::test_util {

// Adds a `testStaticUrlPolicy` TrustedTypes policy that allows Ash WebUI tests
// to create trusted script URLs.
[[nodiscard]] ::testing::AssertionResult AddTestStaticUrlPolicy(
    const content::ToRenderFrameHost& execution_target);

}  // namespace ash::test_util

#endif  //  ASH_WEBUI_COMMON_TRUSTED_TYPES_TEST_UTIL_H_
