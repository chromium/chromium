// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace {
// This should be consistent with
// chrome/test/data/extensions/api_test/command_line/basics/test.js.
const char kTestCommandLineSwitch[] = "command-line-private-api-test-foo";
}  // namespace

class CommandLinePrivateApiTest : public extensions::ExtensionApiTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(kTestCommandLineSwitch);
  }
};

IN_PROC_BROWSER_TEST_F(CommandLinePrivateApiTest, Basics) {
  EXPECT_TRUE(
      RunExtensionTest("command_line/basics", {}, {.load_as_component = true}))
      << message_;
}
