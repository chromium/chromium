// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/script_executor.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

using BackgroundScriptExecutorBrowserTest = ExtensionBrowserTest;

// Tests the ability to run JS in an extension-registered service worker.
IN_PROC_BROWSER_TEST_F(BackgroundScriptExecutorBrowserTest,
                       ExecuteScriptInServiceWorker) {
  constexpr char kManifest[] =
      R"({
          "name": "Test",
          "manifest_version": 3,
          "background": {"service_worker": "background.js"},
          "version": "0.1"
        })";
  constexpr char kBackgroundScript[] =
      R"(self.myTestFlag = 'HELLO!';
         chrome.test.sendMessage('ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundScript);
  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  {
    // Synchronous result.
    base::Value value = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), "chrome.test.sendScriptResult(myTestFlag);",
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    EXPECT_THAT(value, base::test::IsJson(R"("HELLO!")"));
  }

  {
    // Asynchronous result.
    static constexpr char kScript[] =
        R"(setTimeout(() => {
             chrome.test.sendScriptResult(myTestFlag);
           });)";
    base::Value value = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    EXPECT_THAT(value, base::test::IsJson(R"("HELLO!")"));
  }
}

// Tests the ability to run JS in an extension background page.
IN_PROC_BROWSER_TEST_F(BackgroundScriptExecutorBrowserTest,
                       ExecuteScriptInBackgroundPage) {
  constexpr char kManifest[] =
      R"({
          "name": "Test",
          "manifest_version": 2,
          "background": {"scripts": ["background.js"]},
          "version": "0.1"
        })";

  constexpr char kBackgroundScript[] = R"(
    function createResult() {
      return {
        testFlag: 'flag',
        userGesture: chrome.test.isProcessingUserGesture(),
      };
    })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundScript);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  {
    // Synchronous result with no user gesture.
    // NOTE: This test has to come first. User gestures are timed, so once a
    // script executes with a user gesture, it affects subsequent injections
    // because the gesture is still considered active.
    // (This is okay because we really only need to check once each for user
    // gesture; if we wanted to do more involved testing, we'll need to pull
    // these two tests apart or otherwise flush the gesture state.)
    base::Value value = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(),
        "chrome.test.sendScriptResult(createResult());",
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult,
        browsertest_util::ScriptUserActivation::kDontActivate);
    EXPECT_THAT(value, base::test::IsJson(
                           R"({"testFlag":"flag","userGesture":false})"));
  }

  {
    // Synchronous result.
    base::Value value = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(),
        "chrome.test.sendScriptResult(createResult());",
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult,
        browsertest_util::ScriptUserActivation::kActivate);
    EXPECT_THAT(
        value, base::test::IsJson(R"({"testFlag":"flag","userGesture":true})"));
  }

  {
    // Asynchronous result with sendScriptResult().
    static constexpr char kScript[] =
        R"(setTimeout(() => {
             chrome.test.sendScriptResult(createResult());
           }, 0);)";
    base::Value value = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult,
        browsertest_util::ScriptUserActivation::kActivate);
    EXPECT_THAT(
        value, base::test::IsJson(R"({"testFlag":"flag","userGesture":true})"));
  }

  {
    // Asynchronous result with domAutomationController.send().
    static constexpr char kScript[] =
        R"(setTimeout(() => {
             window.domAutomationController.send(createResult());
           }, 0);)";
    base::Value value = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kWindowDomAutomationController,
        browsertest_util::ScriptUserActivation::kActivate);
    EXPECT_THAT(
        value, base::test::IsJson(R"({"testFlag":"flag","userGesture":true})"));
  }
}

}  // namespace extensions
