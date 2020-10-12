// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  EXPECT_TRUE(RunExtensionSubtest("idltest/binary_data", "binary.html"));
  EXPECT_TRUE(RunExtensionSubtest("idltest/nocompile", "nocompile.html"));
}
