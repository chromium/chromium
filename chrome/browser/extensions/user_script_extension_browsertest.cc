// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/convert_user_script.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/user_script.h"
#include "extensions/test/test_content_script_load_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

class UserScriptExtensionBrowserTest : public ExtensionBrowserTest {
 public:
  UserScriptExtensionBrowserTest() = default;
  ~UserScriptExtensionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// A simple test for injection of a script from an extension converted from a
// user script.
IN_PROC_BROWSER_TEST_F(UserScriptExtensionBrowserTest, TestBasicInjection) {
  // Use a TestExtensionDir as an easy way to write files and get a temp
  // directory.
  base::ScopedAllowBlockingForTesting allow_blocking;
  static constexpr char kScriptFile[] =
      R"(// ==UserScript==
         // @name My user script
         // @version 1.0
         // @namespace http://www.google.com
         // @description Does stuff
         // @match http://example.com/*
         // ==/UserScript==
         document.title = 'user script injected';)";
  TestExtensionDir test_dir;
  test_dir.WriteFile(FILE_PATH_LITERAL("script.user.js"), kScriptFile);

  // Convert the user script to an extension.
  std::u16string error;
  scoped_refptr<const Extension> extension(ConvertUserScriptToExtension(
      test_dir.UnpackedPath().AppendASCII("script.user.js"),
      GURL("http://www.google.com/"), test_dir.UnpackedPath(), &error));
  ASSERT_TRUE(extension);
  EXPECT_EQ(u"", error);

  // Install the extension and grant permissions.
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());
  updater.GrantActivePermissions(extension.get());

  extension_service()->AddExtension(extension.get());

  // Wait for the scripts to load, if they haven't already.
  UserScriptManager* user_script_manager =
      ExtensionSystem::Get(profile())->user_script_manager();
  ExtensionUserScriptLoader* user_script_loader =
      user_script_manager->GetUserScriptLoaderForExtension(extension->id());
  if (!user_script_loader->HasLoadedScripts()) {
    ContentScriptLoadWaiter waiter(user_script_loader);
    waiter.Wait();
  }

  // Navigate to a page and ensure the script injected.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(
      "user script injected",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"));
}

}  // namespace extensions
