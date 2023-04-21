// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/test_data_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/test.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace OnMessage = api::test::OnMessage;

namespace {

// Tests running extension APIs on WebUI.
class ExtensionWebUITest : public ExtensionApiTest {
 protected:
  testing::AssertionResult RunTest(const char* name,
                                   const GURL& page_url,
                                   bool expected_result) {
    std::string script;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      // Tests are located in chrome/test/data/extensions/webui/$(name).
      base::FilePath path;
      base::PathService::Get(chrome::DIR_TEST_DATA, &path);
      path =
          path.AppendASCII("extensions").AppendASCII("webui").AppendASCII(name);

      // Read the test.
      if (!base::PathExists(path))
        return testing::AssertionFailure() << "Couldn't find " << path.value();
      base::ReadFileToString(path, &script);
      script = "(function(){'use strict';" + script + "}());";
    }

    // Run the test.
    bool actual_result = false;
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
    content::RenderFrameHost* webui = browser()
                                          ->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetPrimaryMainFrame();
    if (!webui)
      return testing::AssertionFailure() << "Failed to navigate to WebUI";
    CHECK(content::ExecuteScriptAndExtractBool(webui, script, &actual_result));
    return (expected_result == actual_result)
               ? testing::AssertionSuccess()
               : (testing::AssertionFailure() << "Check console output");
  }

  testing::AssertionResult RunTestOnExtensionsPage(const char* name) {
    return RunTest(name, GURL("chrome://extensions"), true);
  }

  testing::AssertionResult RunTestOnAboutPage(const char* name) {
    // chrome://about is an innocuous page that doesn't have any bindings.
    // Tests should fail.
    return RunTest(name, GURL("chrome://about"), false);
  }
};

// Tests running within an <extensionoptions>.
class ExtensionWebUIEmbeddedOptionsTest : public ExtensionWebUITest {
 public:
  void SetUpOnMainThread() override {
    ExtensionWebUITest::SetUpOnMainThread();
    guest_view::GuestViewManager::set_factory_for_testing(
        &test_guest_view_manager_factory_);
    test_guest_view_manager_ = static_cast<guest_view::TestGuestViewManager*>(
        guest_view::GuestViewManager::CreateWithDelegate(
            browser()->profile(),
            ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                browser()->profile())));
  }

 protected:
  // Loads |extension|'s options page in an <extensionoptions> and returns the
  // <extensionoptions>'s main RenderFrameHost.
  content::RenderFrameHost* OpenExtensionOptions(const Extension* extension) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(chrome::kChromeUIExtensionsURL)));
    content::WebContents* webui =
        browser()->tab_strip_model()->GetActiveWebContents();

    EXPECT_EQ(0U, test_guest_view_manager_->num_guests_created());

    EXPECT_TRUE(content::ExecuteScript(
        webui,
        content::JsReplace(
            "let extensionoptions = document.createElement('extensionoptions');"
            "extensionoptions.extension = $1;"
            "document.body.appendChild(extensionoptions);",
            extension->id())));

    guest_view::GuestViewBase* guest_view =
        test_guest_view_manager_->WaitForSingleGuestViewCreated();
    EXPECT_TRUE(guest_view);
    WaitForGuestViewLoadStop(guest_view);

    return guest_view->GetGuestMainFrame();
  }

 private:
  // In preparation for the migration of guest view from inner WebContents to
  // MPArch (crbug/1261928), individual tests should avoid accessing the guest's
  // inner WebContents. The direct access is centralized in this helper function
  // for easier migration.
  //
  // TODO(crbug/1261928): Update this implementation for MPArch, and consider
  // relocate it to `content/public/test/browser_test_utils.h`.
  void WaitForGuestViewLoadStop(guest_view::GuestViewBase* guest_view) {
    auto* guest_contents = guest_view->web_contents();
    ASSERT_TRUE(content::WaitForLoadStop(guest_contents));
  }

  guest_view::TestGuestViewManagerFactory test_guest_view_manager_factory_;
  raw_ptr<guest_view::TestGuestViewManager, DanglingUntriaged>
      test_guest_view_manager_ = nullptr;
};

#if !BUILDFLAG(IS_WIN)  // flaky http://crbug.com/530722

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, SanityCheckAvailableAPIs) {
  ASSERT_TRUE(RunTestOnExtensionsPage("sanity_check_available_apis.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, SanityCheckUnavailableAPIs) {
  ASSERT_TRUE(RunTestOnAboutPage("sanity_check_available_apis.js"));
}

// Tests chrome.test.sendMessage, which exercises WebUI making a
// function call and receiving a response.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, SendMessage) {
  ExtensionTestMessageListener ping_listener("ping", ReplyBehavior::kWillReply);

  ASSERT_TRUE(RunTestOnExtensionsPage("send_message.js"));
  ASSERT_TRUE(ping_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener result_listener;
  ping_listener.Reply("pong");

  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("true", result_listener.message());
}

// Tests chrome.runtime.onMessage, which exercises WebUI registering and
// receiving an event.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, OnMessage) {
  ASSERT_TRUE(RunTestOnExtensionsPage("on_message.js"));

  ExtensionTestMessageListener result_listener;

  OnMessage::Info info;
  info.data = "hi";
  info.last_message = true;
  EventRouter::Get(profile())->BroadcastEvent(base::WrapUnique(
      new Event(events::RUNTIME_ON_MESSAGE, OnMessage::kEventName,
                OnMessage::Create(info))));

  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("true", result_listener.message());
}

// Tests chrome.runtime.lastError, which exercises WebUI accessing a property
// on an API which it doesn't actually have access to. A bindings test really.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, RuntimeLastError) {
  ExtensionTestMessageListener ping_listener("ping", ReplyBehavior::kWillReply);

  ASSERT_TRUE(RunTestOnExtensionsPage("runtime_last_error.js"));
  ASSERT_TRUE(ping_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener result_listener;
  ping_listener.ReplyWithError("unknown host");
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("true", result_listener.message());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, CanEmbedExtensionOptions) {
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("extension_options")
                        .AppendASCII("extension_with_options_page"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(RunTestOnExtensionsPage("can_embed_extension_options.js"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener load_listener("load");
  ready_listener.Reply(extension->id());
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());
}

// Tests that an <extensionoptions> guest view can access appropriate APIs,
// including chrome.storage (semi-privileged; exposed to trusted contexts and
// contexts like content scripts and embedded resources in platform apps) and
// chrome.tabs (privileged; only exposed to trusted contexts).
IN_PROC_BROWSER_TEST_F(ExtensionWebUIEmbeddedOptionsTest,
                       ExtensionOptionsCanAccessAppropriateAPIs) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("extension_options")
                        .AppendASCII("extension_with_options_page"));
  ASSERT_TRUE(extension);

  auto* guest_rfh = OpenExtensionOptions(extension);

  // Check access to the storage API, both for getting/setting values and being
  // notified of changes.
  const std::string storage_key = "test";
  const int storage_value = 42;

  EXPECT_TRUE(content::ExecuteScript(
      guest_rfh,
      content::JsReplace("var onChangedPromise = new Promise((resolve) => {"
                         "  chrome.storage.onChanged.addListener((change) => {"
                         "    resolve(change[$1].newValue);"
                         "  });"
                         "});",
                         storage_key)));

  ASSERT_EQ(
      "success",
      content::EvalJs(
          guest_rfh,
          content::JsReplace(
              "try {"
              "  new Promise(resolve => {"
              "    chrome.storage.local.set({$1: $2}, () => {"
              "      resolve("
              "          chrome.runtime.lastError ?"
              "              chrome.runtime.lastError.message : 'success');"
              "    });"
              "  });"
              "} catch (e) {"
              "  e.name + ': ' + e.message;"
              "}",
              storage_key, storage_value)));

  EXPECT_EQ(
      storage_value,
      content::EvalJs(guest_rfh, content::JsReplace(
                                     "new Promise(resolve =>"
                                     "  chrome.storage.local.get((storage) => "
                                     "    resolve(storage[$1])));",
                                     storage_key)));

  EXPECT_EQ(storage_value, content::EvalJs(guest_rfh, "onChangedPromise;"));

  // Now check access to the tabs API, which is restricted to
  // Feature::BLESSED_EXTENSION_CONTEXTs (which this should be).
  static constexpr char kTabsExecution[] =
      R"(new Promise(r => {
           chrome.tabs.create({}, (tab) => {
             let message;
             // Sanity check that it looks and smells like a tab.
             if (tab && tab.index) {
               message = 'success';
             } else {
               message = chrome.runtime.lastError ?
                             chrome.runtime.lastError.message :
                             'Unknown error';
             }
             r(message);
           });
         });)";
  EXPECT_EQ("success", content::EvalJs(guest_rfh, kTabsExecution));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUIEmbeddedOptionsTest,
                       ExtensionOptionsExternalLinksOpenInNewTab) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("extension_options")
                        .AppendASCII("extension_with_options_page"));
  ASSERT_TRUE(extension);

  auto* guest_rfh = OpenExtensionOptions(extension);

  content::WebContentsAddedObserver new_contents_observer;
  EXPECT_TRUE(content::ExecuteScript(
      guest_rfh, "document.getElementById('link').click();"));
  content::WebContents* new_contents = new_contents_observer.GetWebContents();
  EXPECT_NE(TabStripModel::kNoTab,
            browser()->tab_strip_model()->GetIndexOfWebContents(new_contents));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, ReceivesExtensionOptionsOnClose) {
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);

  const Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("extension_options")
          .AppendASCII("close_self"), 1);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(
      RunTestOnExtensionsPage("receives_extension_options_on_close.js"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener onclose_listener("onclose received");
  ready_listener.Reply(extension->id());
  ASSERT_TRUE(onclose_listener.WaitUntilSatisfied());
}

// Regression test for crbug.com/414526.
//
// Same setup as CanEmbedExtensionOptions but disable the extension before
// embedding.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, EmbedDisabledExtension) {
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);

  std::string extension_id;
  {
    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("extension_options")
                          .AppendASCII("extension_with_options_page"));
    ASSERT_TRUE(extension);
    extension_id = extension->id();
    DisableExtension(extension_id);
  }

  ASSERT_TRUE(RunTestOnExtensionsPage("can_embed_extension_options.js"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener create_failed_listener("createfailed");
  ready_listener.Reply(extension_id);
  ASSERT_TRUE(create_failed_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, EmbedInvalidExtension) {
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);

  const std::string extension_id = "thisisprobablynotrealextensionid";

  ASSERT_TRUE(RunTestOnExtensionsPage("can_embed_extension_options.js"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener create_failed_listener("createfailed");
  ready_listener.Reply(extension_id);
  ASSERT_TRUE(create_failed_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, EmbedExtensionWithoutOptionsPage) {
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("extension_options")
                        .AppendASCII("extension_without_options_page"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(RunTestOnExtensionsPage("can_embed_extension_options.js"));
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener create_failed_listener("createfailed");
  ready_listener.Reply(extension->id());
  ASSERT_TRUE(create_failed_listener.WaitUntilSatisfied());
}

// Tests crbug.com/1253745 where adding and removing listeners in a WebUI frame
// causes all listeners to be removed.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, MultipleURLListeners) {
  content::URLDataSource::Add(profile(),
                              std::make_unique<TestDataSource>("extensions"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("chrome://test/body1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  EventRouter* event_router = EventRouter::Get(profile());
  EXPECT_FALSE(event_router->HasEventListener("test.onMessage"));
  // Register a listener and create a child frame at a different URL.
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(content::ExecuteScript(main_frame, R"(
      const listener = e => {};
      chrome.test.onMessage.addListener(listener);
      const iframe = document.createElement('iframe');
      iframe.src = 'chrome://test/body2.html';
      document.body.appendChild(iframe);
  )"));
  EXPECT_TRUE(event_router->HasEventListener("test.onMessage"));
  observer.Wait();

  // Add and remove the listener in the child frame.
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);
  EXPECT_EQ(GURL("chrome://test/body2.html"),
            child_frame->GetLastCommittedURL());
  EXPECT_TRUE(content::ExecuteScript(child_frame, R"(
      const listener = e => {};
      chrome.test.onMessage.addListener(listener);
      chrome.test.onMessage.removeListener(listener);
  )"));
  EXPECT_TRUE(event_router->HasEventListener("test.onMessage"));

  // Now remove last listener from main frame.
  EXPECT_TRUE(content::ExecuteScript(main_frame, R"(
      chrome.test.onMessage.removeListener(listener);
  )"));
  EXPECT_FALSE(event_router->HasEventListener("test.onMessage"));
}

#endif

}  // namespace

}  // namespace extensions
