// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Tests web assembly usage in Manifest V3 extensions and its interaction with
// the default extension CSP.

using WasmMV3BrowserTest = ExtensionApiTest;

// Test web assembly usage in a service worker.
IN_PROC_BROWSER_TEST_F(WasmMV3BrowserTest, ServiceWorker) {
  ResultCatcher catcher;

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("wasm_mv3")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply("go");

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test web assembly usage without explicit CSP allowing it.
IN_PROC_BROWSER_TEST_F(WasmMV3BrowserTest, ExtensionPageNoCSP) {
  ASSERT_TRUE(RunExtensionTest("no_wasm_mv3", {.extension_url = "page.html"}))
      << message_;
}

// Test web assembly usage in an extension page.
IN_PROC_BROWSER_TEST_F(WasmMV3BrowserTest, ExtensionPage) {
  ASSERT_TRUE(RunExtensionTest("wasm_mv3", {.extension_url = "page.html"}))
      << message_;
}

}  // namespace extensions
