// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/memory/ptr_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/test/base/scoped_channel_override.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace shell_integration {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST(ShellIntegrationMacTest, GetDirectLaunchUrlScheme) {
  // Test each channel on Mac.
  {
    chrome::ScopedChannelOverride stable(
        chrome::ScopedChannelOverride::Channel::kStable);
    EXPECT_EQ("google-chrome", GetDirectLaunchUrlScheme());
  }
  {
    chrome::ScopedChannelOverride beta(
        chrome::ScopedChannelOverride::Channel::kBeta);
    EXPECT_EQ("google-chrome-beta", GetDirectLaunchUrlScheme());
  }
  {
    chrome::ScopedChannelOverride dev(
        chrome::ScopedChannelOverride::Channel::kDev);
    EXPECT_EQ("google-chrome-dev", GetDirectLaunchUrlScheme());
  }
  {
    chrome::ScopedChannelOverride canary(
        chrome::ScopedChannelOverride::Channel::kCanary);
    EXPECT_EQ("google-chrome-canary", GetDirectLaunchUrlScheme());
  }
}
#else  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST(ShellIntegrationMacTest, GetDirectLaunchUrlSchemeUnbranded) {
  EXPECT_EQ("chromium", GetDirectLaunchUrlScheme());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace shell_integration
