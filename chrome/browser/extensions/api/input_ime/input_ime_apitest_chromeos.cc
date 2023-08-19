// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace extensions {

namespace {

class InputImeApiTest : public ExtensionApiTest {
 public:
  InputImeApiTest() = default;
  InputImeApiTest(const InputImeApiTest&) = delete;
  InputImeApiTest& operator=(const InputImeApiTest&) = delete;
  ~InputImeApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    // The test extension needs chrome.inputMethodPrivate to set up
    // the test.
    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    "ilanclmaeigfpnmdlgelmhkpkegdioip");
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(InputImeApiTest, Basic) {
  // Enable the test IME from the test extension.
  std::vector<std::string> extension_ime_ids{
      "_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest"};
  ash::input_method::InputMethodManager::Get()
      ->GetActiveIMEState()
      ->SetEnabledExtensionImes(extension_ime_ids);
  ASSERT_TRUE(RunExtensionTest("input_ime")) << message_;
}

}  // namespace extensions
