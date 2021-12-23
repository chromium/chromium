// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

using BrowserTestUtilBrowserTest = ExtensionBrowserTest;

// Tests the ability to run JS in an extension-registered service worker.
IN_PROC_BROWSER_TEST_F(BrowserTestUtilBrowserTest,
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
  ExtensionTestMessageListener listener("ready", /*will_reply=*/false);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  base::RunLoop run_loop;
  base::Value value_out;
  auto callback = [&run_loop, &value_out](base::Value value) {
    value_out = std::move(value);
    run_loop.Quit();
  };

  browsertest_util::ExecuteScriptInServiceWorker(
      profile(), extension->id(), "console.warn('script ran'); myTestFlag;",
      base::BindLambdaForTesting(callback));
  run_loop.Run();
  EXPECT_THAT(value_out, base::test::IsJson(R"("HELLO!")"));
}

}  // namespace extensions
