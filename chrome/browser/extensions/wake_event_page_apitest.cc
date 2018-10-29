// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/background_page_watcher.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {
namespace {

// manifest.json:
//
// This uses single quotes for brevity, which will be replaced by double quotes
// when installing the extension.
//
// Expects a single string replacement of the "background" property, including
// trailing comma, or nothing if there is no background page.
const char* kManifestJson =
    "{\n"
    "  %s\n"
    "  'content_scripts': [{\n"
    "    'js': ['content_script.js'],\n"
    "    'matches': ['<all_urls>'],\n"
    "    'run_at': 'document_start'\n"
    "  }],\n"
    "  'manifest_version': 2,\n"
    "  'name': 'wake_event_page_apitest',\n"
    "  'version': '1'\n"
    "}\n";

// content_script.js:
//
// This content script just wakes the event page whenever it runs, then sends a
// chrome.test message with the result.
//
// Note: The wake-event-page function is exposed to content scripts via the
// chrome.test API for testing purposes only. In production its intended use
// case is from workers.
const char* kContentScriptJs =
    "chrome.test.getWakeEventPage()(function(success) {\n"
    "  chrome.test.sendMessage(success ? 'success' : 'failure');\n"
    "});\n";

class WakeEventPageTest : public ExtensionBrowserTest {
 public:
  WakeEventPageTest() {}

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  enum BackgroundPageConfiguration { EVENT, PERSISTENT, NONE };

  void RunTest(bool expect_success,
               BackgroundPageConfiguration bg_config,
               bool should_close,
               bool will_be_open) {
    ASSERT_TRUE(embedded_test_server()->Start());

    GURL web_url = embedded_test_server()->GetURL("example.com", "/empty.html");

    TestExtensionDir extension_dir;
    {
      std::string manifest_json;
      switch (bg_config) {
        case EVENT:
          manifest_json =
              base::StringPrintf(kManifestJson,
                                 "  'background': {\n"
                                 "    'persistent': false,\n"
                                 "    'scripts': ['background.js']\n"
                                 "  },");
          break;
        case PERSISTENT:
          manifest_json =
              base::StringPrintf(kManifestJson,
                                 "  'background': {\n"
                                 "    'persistent': true,\n"
                                 "    'scripts': ['background.js']\n"
                                 "  },");
          break;
        case NONE:
          manifest_json = base::StringPrintf(kManifestJson, "");
          break;
      }
      base::ReplaceChars(manifest_json, "'", "\"", &manifest_json);
      extension_dir.WriteManifest(manifest_json);
      // Empty background page. Closing/opening it is driven by this test.
      extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
      extension_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"),
                              kContentScriptJs);
    }

    // Install the extension, then close its background page if desired.
    // TODO(https://crbug.com/898682): Waiting for content scripts to load
    // should be done as part of the extension loading process.
    content::WindowedNotificationObserver scripts_updated_observer(
        extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
        content::NotificationService::AllSources());
    const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
    scripts_updated_observer.Wait();
    CHECK(extension);

    // Regardless of |will_be_open|, we haven't closed the background page yet,
    // so it should always open if it exists.
    if (bg_config != NONE)
      BackgroundPageWatcher(process_manager(), extension).WaitForOpen();

    if (should_close) {
      GetBackgroundPage(extension->id())->Close();
      BackgroundPageWatcher(process_manager(), extension).WaitForClose();
      EXPECT_FALSE(GetBackgroundPage(extension->id()));
    }

    // Start a content script to wake up the background page, if it's closed.
    {
      ExtensionTestMessageListener listener(false /* will_reply */);
      ui_test_utils::NavigateToURL(browser(), web_url);
      ASSERT_TRUE(listener.WaitUntilSatisfied());
      EXPECT_EQ(expect_success ? "success" : "failure", listener.message());
    }

    EXPECT_EQ(will_be_open, GetBackgroundPage(extension->id()) != nullptr);

    // Run the content script again. The background page will be awaken iff
    // |will_be_open| is true, but if not, this is a harmless no-op.
    {
      ExtensionTestMessageListener listener(false /* will_reply */);
      ui_test_utils::NavigateToURL(browser(), web_url);
      ASSERT_TRUE(listener.WaitUntilSatisfied());
      EXPECT_EQ(expect_success ? "success" : "failure", listener.message());
    }

    EXPECT_EQ(will_be_open, GetBackgroundPage(extension->id()) != nullptr);
  }

 private:
  ExtensionHost* GetBackgroundPage(const std::string& extension_id) {
    return process_manager()->GetBackgroundHostForExtension(extension_id);
  }

  ProcessManager* process_manager() { return ProcessManager::Get(profile()); }

  DISALLOW_COPY_AND_ASSIGN(WakeEventPageTest);
};

IN_PROC_BROWSER_TEST_F(WakeEventPageTest, ClosedEventPage) {
  RunTest(true /* expect_success */, EVENT, true /* should_close */,
          true /* will_be_open */);
}

IN_PROC_BROWSER_TEST_F(WakeEventPageTest, OpenEventPage) {
  RunTest(true /* expect_success */, EVENT, false /* should_close */,
          true /* will_be_open */);
}

IN_PROC_BROWSER_TEST_F(WakeEventPageTest, ClosedPersistentBackgroundPage) {
  // Note: this is an odd test, because persistent background pages aren't
  // supposed to close. Extensions can close them with window.close() but why
  // would they do that? Test it anyway.
  RunTest(false /* expect_success */, PERSISTENT, true /* should_close */,
          false /* will_be_open */);
}

IN_PROC_BROWSER_TEST_F(WakeEventPageTest, OpenPersistentBackgroundPage) {
  RunTest(true /* expect_success */, PERSISTENT, false /* should_close */,
          true /* will_be_open */);
}

IN_PROC_BROWSER_TEST_F(WakeEventPageTest, NoBackgroundPage) {
  RunTest(false /* expect_success */, NONE, false /* should_close */,
          false /* will_be_open */);
}

}  // namespace
}  // namespace extensions
