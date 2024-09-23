// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"

class ExtensionApiTestWithSwitch : public extensions::ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kSilentDebuggerExtensionAPI);
    command_line->AppendSwitch(extensions::switches::kExtensionsOnChromeURLs);
  }

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithSwitch, ExtensionDebugger) {
  ASSERT_TRUE(RunExtensionTest("debugger_extension")) << message_;
}

// TODO(crbug.com/41485082) Flaky on various platforms
IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithSwitch, DISABLED_ExtensionTracing) {
  ASSERT_TRUE(RunExtensionTest("tracing_extension")) << message_;
}
