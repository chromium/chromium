// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/switches.h"

class ExtensionIdltestApiTest : public extensions::ExtensionApiTest {
 public:
  // Set the channel to "trunk" since idltest is restricted to trunk.
  ExtensionIdltestApiTest() : trunk_(version_info::Channel::UNKNOWN) {}
  ~ExtensionIdltestApiTest() override {}

 private:
  extensions::ScopedCurrentChannel trunk_;
};

IN_PROC_BROWSER_TEST_F(ExtensionIdltestApiTest, IdlCompiler) {
  EXPECT_TRUE(RunExtensionTest("idltest/binary_data",
                               {.extension_url = "binary.html"}));
  EXPECT_TRUE(RunExtensionTest("idltest/nocompile",
                               {.extension_url = "nocompile.html"}));
}
