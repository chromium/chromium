// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"

namespace extensions {

class WorkerTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // To enable module dedicated workers.
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalWebPlatformFeatures);
  }
};

IN_PROC_BROWSER_TEST_F(WorkerTest, WorkerInBackgroundPage) {
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("worker")) << message_;
}

}  // namespace extensions
