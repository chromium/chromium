// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "build/branding_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shell_integration {

TEST(ShellIntegrationTest, GetDirectLaunchUrlSchemeUnbranded) {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ("chromium", GetDirectLaunchUrlScheme());
#endif
}

TEST(ShellIntegrationTest, GetDirectLaunchUrlSchemeBranded) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ("google-chrome", GetDirectLaunchUrlScheme());
#endif
}

}  // namespace shell_integration
