// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/memory/ptr_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shell_integration {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// A test fixture that allows for overriding the channel on macOS.
class ShellIntegrationMacTest : public testing::Test {
 public:
  ShellIntegrationMacTest() = default;

 protected:
  class ScopedChannelOverrider {
   public:
    explicit ScopedChannelOverrider(version_info::Channel channel)
        : original_channel_(chrome::GetChannel()) {
      chrome::SetChannelForTesting(channel);
    }
    ~ScopedChannelOverrider() {
      chrome::SetChannelForTesting(original_channel_);
    }

   private:
    version_info::Channel original_channel_;
  };
};

TEST_F(ShellIntegrationMacTest, GetDirectLaunchUrlScheme) {
  // Test each channel on Mac.
  {
    ScopedChannelOverrider stable(version_info::Channel::STABLE);
    EXPECT_EQ("google-chrome", GetDirectLaunchUrlScheme());
  }
  {
    ScopedChannelOverrider beta(version_info::Channel::BETA);
    EXPECT_EQ("google-chrome-beta", GetDirectLaunchUrlScheme());
  }
  {
    ScopedChannelOverrider dev(version_info::Channel::DEV);
    EXPECT_EQ("google-chrome-dev", GetDirectLaunchUrlScheme());
  }
  {
    ScopedChannelOverrider canary(version_info::Channel::CANARY);
    EXPECT_EQ("google-chrome-canary", GetDirectLaunchUrlScheme());
  }
}
#else  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST(ShellIntegrationMacTest, GetDirectLaunchUrlSchemeUnbranded) {
  EXPECT_EQ("chromium", GetDirectLaunchUrlScheme());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace shell_integration
