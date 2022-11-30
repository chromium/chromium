// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MIXIN_BASED_EXTENSION_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_MIXIN_BASED_EXTENSION_APITEST_H_

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace base {
class CommandLine;
}  // namespace base

namespace content {
class BrowserMainParts;
}  // namespace content

namespace extensions {

// Base class for extension API test fixtures that are using mixins.
class MixinBasedExtensionApiTest : public ExtensionApiTest {
 public:
  MixinBasedExtensionApiTest();
  ~MixinBasedExtensionApiTest() override;

  // ExtensionApiTest:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override;
  bool SetUpUserDataDirectory() override;
  void SetUpInProcessBrowserTestFixture() override;
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void TearDownInProcessBrowserTestFixture() override;
  void TearDown() override;

 protected:
  InProcessBrowserTestMixinHost mixin_host_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MIXIN_BASED_EXTENSION_APITEST_H_
