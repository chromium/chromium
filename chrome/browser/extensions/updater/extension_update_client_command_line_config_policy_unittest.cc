// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_update_client_command_line_config_policy.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

TEST(ExtensionUpdateClientCommandLineConfigPolicyTest, CommandLine) {
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);

  {
    const ExtensionUpdateClientCommandLineConfigPolicy config_policy(&cmdline);

#if BUILDFLAG(IS_WIN)
    EXPECT_TRUE(config_policy.BackgroundDownloadsEnabled());
#else
    EXPECT_FALSE(config_policy.BackgroundDownloadsEnabled());
#endif
    EXPECT_TRUE(config_policy.DeltaUpdatesEnabled());
    EXPECT_TRUE(config_policy.PingsEnabled());
    EXPECT_FALSE(config_policy.FastUpdate());
    EXPECT_FALSE(config_policy.TestRequest());
    EXPECT_EQ(GURL(), config_policy.UrlSourceOverride());
  }

  cmdline.AppendSwitch("--extension-updater-test-request");
  {
    const ExtensionUpdateClientCommandLineConfigPolicy config_policy(&cmdline);
    EXPECT_TRUE(config_policy.TestRequest());
  }
}

}  // namespace extensions
