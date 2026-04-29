// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browsertest_util.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace browsertest_util {

namespace {

// NOTE: The methods under test are called indirectly through methods of the
// same name on ExtensionBrowserTest, which supplies the Profile.
class ExtensionBrowsertestUtilTest : public ExtensionBrowserTest {
 public:
  ExtensionBrowsertestUtilTest() = default;

  ExtensionBrowsertestUtilTest(const ExtensionBrowsertestUtilTest&) = delete;
  ExtensionBrowsertestUtilTest& operator=(const ExtensionBrowsertestUtilTest&) =
      delete;

  ~ExtensionBrowsertestUtilTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    extension_ = LoadExtension(test_data_dir_.AppendASCII("extension"));
    ASSERT_TRUE(extension_.get());
  }

 protected:
  const Extension* extension() const { return extension_.get(); }

 private:
  scoped_refptr<const Extension> extension_;
};

IN_PROC_BROWSER_TEST_F(ExtensionBrowsertestUtilTest,
                       ExecuteScriptInBackgroundPage) {
  EXPECT_EQ(extension()->id(),
            ExecuteScriptInBackgroundPage(
                extension()->id(),
                "chrome.test.sendScriptResult(chrome.runtime.id);"));

  // Tests a successful test injection, including running nested tasks in the
  // browser process (via an asynchronous extension API).
  EXPECT_EQ("success",
            ExecuteScriptInBackgroundPage(
                extension()->id(),
                R"(chrome.runtime.setUninstallURL('http://example.com',
                                                  function() {
                     chrome.test.sendScriptResult('success');
                   });)"));

  // Return a non-string argument.
  EXPECT_EQ(3, ExecuteScriptInBackgroundPage(extension()->id(),
                                             "chrome.test.sendScriptResult(3);")
                   .GetInt());
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsertestUtilTest,
                       ExecuteScriptInBackgroundPageDeprecated) {
  EXPECT_EQ(extension()->id(),
            ExecuteScriptInBackgroundPageDeprecated(
                extension()->id(),
                "window.domAutomationController.send(chrome.runtime.id);"));

  // Tests a successful test injection, including running nested tasks in the
  // browser process (via an asynchronous extension API).
  EXPECT_EQ(std::string("/") + extensions::kGeneratedBackgroundPageFilename,
            ExecuteScriptInBackgroundPageDeprecated(
                extension()->id(),
                R"(chrome.runtime.getBackgroundPage(function(result) {
                     let url = new URL(result.location.href);
                     window.domAutomationController.send(url.pathname);
                   });)"));

  // An argument that isn't a string should cause a failure, not a hang.
  EXPECT_NONFATAL_FAILURE(
      ExecuteScriptInBackgroundPageDeprecated(
          extension()->id(), "window.domAutomationController.send(3);"),
      "send(3)");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsertestUtilTest,
                       ExecuteScriptInBackgroundPageNoWait) {
  // Run an arbitrary script to check that we don't wait for a response.
  ASSERT_TRUE(
      ExecuteScriptInBackgroundPageNoWait(extension()->id(), "let foo = 0;"));

  // Run a script asynchronously that passes the test.
  ResultCatcher catcher;
  ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(extension()->id(),
                                                  "chrome.test.notifyPass();"));
  ASSERT_TRUE(catcher.GetNextResult());

  // Specifying a non-existent extension should add a non-fatal failure.
  EXPECT_NONFATAL_FAILURE(
      EXPECT_FALSE(ExecuteScriptInBackgroundPageNoWait(
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "")),
      "No enabled extension with id: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
}

}  // namespace

}  // namespace browsertest_util
}  // namespace extensions
