// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/runtime.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace extensions {
namespace {

class MessageSender : public content::NotificationObserver {
 public:
  MessageSender() {
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
                   content::NotificationService::AllSources());
  }

 private:
  static std::unique_ptr<base::ListValue> BuildEventArguments(
      const bool last_message,
      const std::string& data) {
    std::unique_ptr<base::DictionaryValue> event(new base::DictionaryValue());
    event->SetBoolean("lastMessage", last_message);
    event->SetString("data", data);
    std::unique_ptr<base::ListValue> arguments(new base::ListValue());
    arguments->Append(std::move(event));
    return arguments;
  }

  static std::unique_ptr<Event> BuildEvent(
      std::unique_ptr<base::ListValue> event_args,
      Profile* profile,
      GURL event_url) {
    auto event =
        std::make_unique<Event>(events::TEST_ON_MESSAGE, "test.onMessage",
                                std::move(event_args), profile);
    event->event_url = event_url;
    return event;
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
              type);
    EventRouter* event_router =
        EventRouter::Get(content::Source<Profile>(source).ptr());

    // Sends four messages to the extension. All but the third message sent
    // from the origin http://b.com/ are supposed to arrive.
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(false, "no restriction"),
        content::Source<Profile>(source).ptr(),
        GURL()));
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(false, "http://a.com/"),
        content::Source<Profile>(source).ptr(),
        GURL("http://a.com/")));
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(false, "http://b.com/"),
        content::Source<Profile>(source).ptr(),
        GURL("http://b.com/")));
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(true, "last message"),
        content::Source<Profile>(source).ptr(),
        GURL()));
  }

  content::NotificationRegistrar registrar_;
};

class MessagingApiTest : public ExtensionApiTest {
 public:
  MessagingApiTest() {}
  ~MessagingApiTest() override {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MessagingApiTest);
};

IN_PROC_BROWSER_TEST_F(MessagingApiTest, Messaging) {
  ASSERT_TRUE(RunExtensionTest("messaging/connect")) << message_;
}

IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingCrash) {
  ExtensionTestMessageListener ready_to_crash("ready_to_crash", false);
  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("messaging/connect_crash")));
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(ready_to_crash.WaitUntilSatisfied());

  ResultCatcher catcher;
  CrashTab(tab);
  EXPECT_TRUE(catcher.GetNextResult());
}

// Tests that message passing from one extension to another works.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingExternal) {
  ASSERT_TRUE(LoadExtension(
      shared_test_data_dir().AppendASCII("messaging").AppendASCII("receiver")));

  ASSERT_TRUE(RunExtensionTestWithFlags("messaging/connect_external",
                                        kFlagUseRootExtensionsDir))
      << message_;
}

// Tests that a content script can exchange messages with a tab even if there is
// no background page.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingNoBackground) {
  ASSERT_TRUE(RunExtensionSubtest("messaging/connect_nobackground",
                                  "page_in_main_frame.html")) << message_;
}

// Tests that messages with event_urls are only passed to extensions with
// appropriate permissions.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingEventURL) {
  MessageSender sender;
  ASSERT_TRUE(RunExtensionTest("messaging/event_url")) << message_;
}

// Tests that messages cannot be received from the same frame.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingBackgroundOnly) {
  ASSERT_TRUE(RunExtensionTest("messaging/background_only")) << message_;
}

// Tests externally_connectable between a web page and an extension.
//
// TODO(kalman): Test between extensions. This is already tested in this file,
// but not with externally_connectable set in the manifest.
//
// TODO(kalman): Test with host permissions.
class ExternallyConnectableMessagingTest : public MessagingApiTest {
 protected:
  // Result codes from the test. These must match up with |results| in
  // c/t/d/extensions/api_test/externally_connectable/assertions.json.
  enum Result {
    OK = 0,
    NAMESPACE_NOT_DEFINED = 1,
    FUNCTION_NOT_DEFINED = 2,
    COULD_NOT_ESTABLISH_CONNECTION_ERROR = 3,
    OTHER_ERROR = 4,
    INCORRECT_RESPONSE_SENDER = 5,
    INCORRECT_RESPONSE_MESSAGE = 6,
  };

  bool AppendIframe(const GURL& src) {
    bool result;
    CHECK(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "actions.appendIframe('" + src.spec() + "');", &result));
    return result;
  }

  Result CanConnectAndSendMessagesToMainFrame(const Extension* extension,
                                              const char* message = NULL) {
    return CanConnectAndSendMessagesToFrame(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
        extension,
        message);
  }

  Result CanConnectAndSendMessagesToIFrame(const Extension* extension,
                                           const char* message = NULL) {
    content::RenderFrameHost* frame = content::FrameMatchingPredicate(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::Bind(&content::FrameIsChildOfMainFrame));
    return CanConnectAndSendMessagesToFrame(frame, extension, message);
  }

  Result CanConnectAndSendMessagesToFrame(content::RenderFrameHost* frame,
                                          const Extension* extension,
                                          const char* message) {
    int result;
    std::string command = base::StringPrintf(
        "assertions.canConnectAndSendMessages('%s', %s, %s)",
        extension->id().c_str(),
        extension->is_platform_app() ? "true" : "false",
        message ? base::StringPrintf("'%s'", message).c_str() : "undefined");
    CHECK(content::ExecuteScriptAndExtractInt(frame, command, &result));
    return static_cast<Result>(result);
  }

  testing::AssertionResult AreAnyNonWebApisDefinedForMainFrame() {
    return AreAnyNonWebApisDefinedForFrame(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame());
  }

  testing::AssertionResult AreAnyNonWebApisDefinedForIFrame() {
    content::RenderFrameHost* frame = content::FrameMatchingPredicate(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::Bind(&content::FrameIsChildOfMainFrame));
    return AreAnyNonWebApisDefinedForFrame(frame);
  }

  testing::AssertionResult AreAnyNonWebApisDefinedForFrame(
      content::RenderFrameHost* frame) {
    // All runtime API methods are non-web except for sendRequest and connect.
    const char* const non_messaging_apis[] = {
        "getBackgroundPage",
        "getManifest",
        "getURL",
        "reload",
        "requestUpdateCheck",
        "restart",
        "connectNative",
        "sendNativeMessage",
        "onStartup",
        "onInstalled",
        "onSuspend",
        "onSuspendCanceled",
        "onUpdateAvailable",
        "onBrowserUpdateAvailable",
        "onConnect",
        "onConnectExternal",
        "onMessage",
        "onMessageExternal",
        "onRestartRequired",
        // Note: no "id" here because this test method is used for hosted apps,
        // which do have access to runtime.id.
    };

    // Turn the array into a JS array, which effectively gets eval()ed.
    std::string as_js_array;
    for (size_t i = 0; i < base::size(non_messaging_apis); ++i) {
      as_js_array += as_js_array.empty() ? "[" : ",";
      as_js_array += base::StringPrintf("'%s'", non_messaging_apis[i]);
    }
    as_js_array += "]";

    bool any_defined;
    CHECK(content::ExecuteScriptAndExtractBool(
        frame,
        "assertions.areAnyRuntimePropertiesDefined(" + as_js_array + ")",
        &any_defined));
    return any_defined ?
        testing::AssertionSuccess() : testing::AssertionFailure();
  }

  std::string GetTlsChannelIdFromPortConnect(const Extension* extension,
                                             bool include_tls_channel_id,
                                             const char* message = NULL) {
    return GetTlsChannelIdFromAssertion("getTlsChannelIdFromPortConnect",
                                        extension,
                                        include_tls_channel_id,
                                        message);
  }

  std::string GetTlsChannelIdFromSendMessage(const Extension* extension,
                                             bool include_tls_channel_id,
                                             const char* message = NULL) {
    return GetTlsChannelIdFromAssertion("getTlsChannelIdFromSendMessage",
                                        extension,
                                        include_tls_channel_id,
                                        message);
  }

  GURL GetURLForPath(const std::string& host, const std::string& path) {
    std::string port = base::NumberToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    return embedded_test_server()->GetURL(path).ReplaceComponents(replacements);
  }

  GURL chromium_org_url() {
    return GetURLForPath("www.chromium.org", "/chromium.org.html");
  }

  GURL popup_opener_url() {
    return GetURLForPath("www.chromium.org", "/popup_opener.html");
  }

  GURL google_com_url() {
    return GetURLForPath("www.google.com", "/google.com.html");
  }

  scoped_refptr<const Extension> LoadChromiumConnectableExtension() {
    scoped_refptr<const Extension> extension = LoadExtensionIntoDir(
        &web_connectable_dir_extension_,
        base::StringPrintf("{"
                           "  \"name\": \"chromium_connectable\","
                           "  %s,"
                           "  \"externally_connectable\": {"
                           "    \"matches\": [\"*://*.chromium.org:*/*\"]"
                           "  }"
                           "}",
                           common_manifest()));
    CHECK(extension.get());
    return extension;
  }

  scoped_refptr<const Extension> LoadChromiumConnectableApp(
      bool with_event_handlers = true) {
    scoped_refptr<const Extension> extension =
        LoadExtensionIntoDir(&web_connectable_dir_app_,
                             "{"
                             "  \"app\": {"
                             "    \"background\": {"
                             "      \"scripts\": [\"background.js\"]"
                             "    }"
                             "  },"
                             "  \"externally_connectable\": {"
                             "    \"matches\": [\"*://*.chromium.org:*/*\"]"
                             "  },"
                             "  \"manifest_version\": 2,"
                             "  \"name\": \"app_connectable\","
                             "  \"version\": \"1.0\""
                             "}",
                             with_event_handlers);
    CHECK(extension.get());
    return extension;
  }

  scoped_refptr<const Extension> LoadNotConnectableExtension() {
    scoped_refptr<const Extension> extension =
        LoadExtensionIntoDir(&not_connectable_dir_,
                             base::StringPrintf(
                                 "{"
                                 "  \"name\": \"not_connectable\","
                                 "  %s"
                                 "}",
                                 common_manifest()));
    CHECK(extension.get());
    return extension;
  }

  scoped_refptr<const Extension>
  LoadChromiumConnectableExtensionWithTlsChannelId() {
    return LoadExtensionIntoDir(&tls_channel_id_connectable_dir_,
                                connectable_with_tls_channel_id_manifest());
  }

  scoped_refptr<const Extension> LoadChromiumHostedApp() {
    scoped_refptr<const Extension> hosted_app =
        LoadExtensionIntoDir(&hosted_app_dir_,
                             base::StringPrintf(
                                 "{"
                                 "  \"name\": \"chromium_hosted_app\","
                                 "  \"version\": \"1.0\","
                                 "  \"manifest_version\": 2,"
                                 "  \"app\": {"
                                 "    \"urls\": [\"%s\"],"
                                 "    \"launch\": {"
                                 "      \"web_url\": \"%s\""
                                 "    }\n"
                                 "  }\n"
                                 "}",
                                 chromium_org_url().spec().c_str(),
                                 chromium_org_url().spec().c_str()));
    CHECK(hosted_app.get());
    return hosted_app;
  }

  void SetUpOnMainThread() override {
    base::FilePath test_data;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    embedded_test_server()->ServeFilesFromDirectory(test_data.AppendASCII(
        "extensions/api_test/messaging/externally_connectable/sites"));
    MessagingApiTest::SetUpOnMainThread();
  }

  const char* close_background_message() {
    return "closeBackgroundPage";
  }

 private:
  scoped_refptr<const Extension> LoadExtensionIntoDir(
      TestExtensionDir* dir,
      const std::string& manifest,
      bool with_event_handlers = true) {
    dir->WriteManifest(manifest);
    if (with_event_handlers) {
      dir->WriteFile(
          FILE_PATH_LITERAL("background.js"),
          base::StringPrintf(
              "function maybeClose(message) {\n"
              "  if (message.indexOf('%s') >= 0)\n"
              "    window.setTimeout(function() { window.close() }, 0);\n"
              "}\n"
              "chrome.runtime.onMessageExternal.addListener(\n"
              "    function(message, sender, reply) {\n"
              "  reply({ message: message, sender: sender });\n"
              "  maybeClose(message);\n"
              "});\n"
              "chrome.runtime.onConnectExternal.addListener(function(port) {\n"
              "  port.onMessage.addListener(function(message) {\n"
              "    port.postMessage({ message: message, sender: port.sender "
              "});\n"
              "    maybeClose(message);\n"
              "  });\n"
              "});\n",
              close_background_message()));
    } else {
      dir->WriteFile(FILE_PATH_LITERAL("background.js"), "");
    }
    return LoadExtension(dir->UnpackedPath());
  }

  const char* common_manifest() {
    return "\"version\": \"1.0\","
           "\"background\": {"
           "    \"scripts\": [\"background.js\"],"
           "    \"persistent\": false"
           "},"
           "\"manifest_version\": 2";
  }

  std::string connectable_with_tls_channel_id_manifest() {
    return base::StringPrintf(
        "{"
        "  \"name\": \"chromium_connectable_with_tls_channel_id\","
        "  %s,"
        "  \"externally_connectable\": {"
        "    \"matches\": [\"*://*.chromium.org:*/*\"],"
        "    \"accepts_tls_channel_id\": true"
        "  }"
        "}",
        common_manifest());
  }

  std::string GetTlsChannelIdFromAssertion(const char* method,
                                           const Extension* extension,
                                           bool include_tls_channel_id,
                                           const char* message) {
    std::string result;
    std::string args = "'" + extension->id() + "', ";
    args += include_tls_channel_id ? "true" : "false";
    if (message)
      args += std::string(", '") + message + "'";
    CHECK(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::StringPrintf("assertions.%s(%s)", method, args.c_str()),
        &result));
    return result;
  }

  TestExtensionDir web_connectable_dir_extension_;
  TestExtensionDir web_connectable_dir_app_;
  TestExtensionDir not_connectable_dir_;
  TestExtensionDir tls_channel_id_connectable_dir_;
  TestExtensionDir hosted_app_dir_;
};

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest, NotInstalled) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetID("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Fake extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToMainFrame(extension.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToMainFrame(extension.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());
}

// TODO(kalman): Most web messaging tests disabled on windows due to extreme
// flakiness. See http://crbug.com/350517.
#if !defined(OS_WIN)

// Tests two extensions on the same sites: one web connectable, one not.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       WebConnectableAndNotConnectable) {
  // Install the web connectable extension. chromium.org can connect to it,
  // google.com can't.
  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(OK,
            CanConnectAndSendMessagesToMainFrame(chromium_connectable.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToMainFrame(chromium_connectable.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  // Install the non-connectable extension. Nothing can connect to it.
  scoped_refptr<const Extension> not_connectable =
      LoadNotConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  // Namespace will be defined here because |chromium_connectable| can connect
  // to it - so this will be the "cannot establish connection" error.
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessagesToMainFrame(not_connectable.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToMainFrame(not_connectable.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());
}

// See http://crbug.com/297866
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       DISABLED_BackgroundPageClosesOnMessageReceipt) {
  // Install the web connectable extension.
  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  // If the background page closes after receipt of the message, it will still
  // reply to this message...
  EXPECT_EQ(OK,
            CanConnectAndSendMessagesToMainFrame(chromium_connectable.get(),
                                                 close_background_message()));
  // and be re-opened by receipt of a subsequent message.
  EXPECT_EQ(OK,
            CanConnectAndSendMessagesToMainFrame(chromium_connectable.get()));
}

// Tests a web connectable extension that doesn't receive TLS channel id.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       WebConnectableWithoutTlsChannelId) {
  // Install the web connectable extension. chromium.org can connect to it,
  // google.com can't.
  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtension();
  ASSERT_TRUE(chromium_connectable.get());

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  // The web connectable extension doesn't request the TLS channel ID, so it
  // doesn't get it, whether or not the page asks for it.
  EXPECT_EQ(std::string(),
            GetTlsChannelIdFromPortConnect(chromium_connectable.get(), false));
  EXPECT_EQ(std::string(),
            GetTlsChannelIdFromSendMessage(chromium_connectable.get(), true));
  EXPECT_EQ(std::string(),
            GetTlsChannelIdFromPortConnect(chromium_connectable.get(), false));
  EXPECT_EQ(std::string(),
            GetTlsChannelIdFromSendMessage(chromium_connectable.get(), true));
}

// Tests a web connectable extension that receives TLS channel id with a site
// that can't connect to it.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       WebConnectableWithTlsChannelIdWithNonMatchingSite) {
  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtensionWithTlsChannelId();
  ASSERT_TRUE(chromium_connectable.get());

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  // The extension requests the TLS channel ID, but it doesn't get it for a
  // site that can't connect to it, regardless of whether the page asks for it.
  EXPECT_EQ(base::NumberToString(NAMESPACE_NOT_DEFINED),
            GetTlsChannelIdFromPortConnect(chromium_connectable.get(), false));
  EXPECT_EQ(base::NumberToString(NAMESPACE_NOT_DEFINED),
            GetTlsChannelIdFromSendMessage(chromium_connectable.get(), true));
  EXPECT_EQ(base::NumberToString(NAMESPACE_NOT_DEFINED),
            GetTlsChannelIdFromPortConnect(chromium_connectable.get(), false));
  EXPECT_EQ(base::NumberToString(NAMESPACE_NOT_DEFINED),
            GetTlsChannelIdFromSendMessage(chromium_connectable.get(), true));
}

// Tests a web connectable extension that receives TLS channel id on a site
// that can connect to it, but with no TLS channel ID having been generated.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       WebConnectableWithTlsChannelIdWithEmptyTlsChannelId) {
  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtensionWithTlsChannelId();
  ASSERT_TRUE(chromium_connectable.get());

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());

  // Since the extension requests the TLS channel ID, it gets it for a site that
  // can connect to it, but only if the page also asks to include it.
  EXPECT_EQ(std::string(),
            GetTlsChannelIdFromPortConnect(chromium_connectable.get(), false));
  EXPECT_EQ(std::string(),
            GetTlsChannelIdFromSendMessage(chromium_connectable.get(), false));
  // If the page does ask for it, it isn't empty.
  std::string tls_channel_id =
      GetTlsChannelIdFromPortConnect(chromium_connectable.get(), true);
  // Because the TLS channel ID has never been generated for this domain,
  // no TLS channel ID is reported.
  EXPECT_EQ(std::string(), tls_channel_id);
}

// Flaky on Linux and Windows. http://crbug.com/315264
// Tests a web connectable extension that receives TLS channel id, but
// immediately closes its background page upon receipt of a message.
IN_PROC_BROWSER_TEST_F(
    ExternallyConnectableMessagingTest,
    DISABLED_WebConnectableWithEmptyTlsChannelIdAndClosedBackgroundPage) {
  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtensionWithTlsChannelId();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  // If the page does ask for it, it isn't empty, even if the background page
  // closes upon receipt of the connect.
  std::string tls_channel_id = GetTlsChannelIdFromPortConnect(
      chromium_connectable.get(), true, close_background_message());
  // Because the TLS channel ID has never been generated for this domain,
  // no TLS channel ID is reported.
  EXPECT_EQ(std::string(), tls_channel_id);
  // A subsequent connect will still succeed, even if the background page was
  // previously closed.
  tls_channel_id =
      GetTlsChannelIdFromPortConnect(chromium_connectable.get(), true);
  // And the empty value is still retrieved.
  EXPECT_EQ(std::string(), tls_channel_id);
}

// Tests that enabling and disabling an extension makes the runtime bindings
// appear and disappear.
//
// TODO(kalman): Test with multiple extensions that can be accessed by the same
// host.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       EnablingAndDisabling) {
  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtension();
  scoped_refptr<const Extension> not_connectable =
      LoadNotConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(OK,
            CanConnectAndSendMessagesToMainFrame(chromium_connectable.get()));
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessagesToMainFrame(not_connectable.get()));

  DisableExtension(chromium_connectable->id());
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessagesToMainFrame(chromium_connectable.get()));

  EnableExtension(chromium_connectable->id());
  EXPECT_EQ(OK,
            CanConnectAndSendMessagesToMainFrame(chromium_connectable.get()));
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessagesToMainFrame(not_connectable.get()));
}

// Tests connection from incognito tabs when the user denies the connection
// request. Spanning mode only. A separate test for apps and extensions.
//
// TODO(kalman): ensure that we exercise split vs spanning incognito logic
// somewhere. This is a test that should be shared with the content script logic
// so it's not really our specific concern for web connectable.
//
// TODO(kalman): test messages from incognito extensions too.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIncognitoDenyApp) {
  scoped_refptr<const Extension> app = LoadChromiumConnectableApp();
  ASSERT_TRUE(app->is_platform_app());

  Browser* incognito_browser = OpenURLOffTheRecord(
      profile()->GetOffTheRecordProfile(), chromium_org_url());
  content::RenderFrameHost* incognito_frame = incognito_browser->
      tab_strip_model()->GetActiveWebContents()->GetMainFrame();

  {
    IncognitoConnectability::ScopedAlertTracker alert_tracker(
        IncognitoConnectability::ScopedAlertTracker::ALWAYS_DENY);

    // No connection because incognito-enabled hasn't been set for the app, and
    // the user denied our interactive request.
    EXPECT_EQ(
        COULD_NOT_ESTABLISH_CONNECTION_ERROR,
        CanConnectAndSendMessagesToFrame(incognito_frame, app.get(), NULL));
    EXPECT_EQ(1, alert_tracker.GetAndResetAlertCount());

    // Try again. User has already denied so alert not shown.
    EXPECT_EQ(
        COULD_NOT_ESTABLISH_CONNECTION_ERROR,
        CanConnectAndSendMessagesToFrame(incognito_frame, app.get(), NULL));
    EXPECT_EQ(0, alert_tracker.GetAndResetAlertCount());
  }

  // It's not possible to allow an app in incognito.
  ExtensionPrefs::Get(profile())->SetIsIncognitoEnabled(app->id(), true);
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessagesToFrame(incognito_frame, app.get(), NULL));
}

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIncognitoDenyExtensionAndApp) {
  scoped_refptr<const Extension> extension = LoadChromiumConnectableExtension();
  EXPECT_FALSE(util::IsIncognitoEnabled(extension->id(), profile()));

  Browser* incognito_browser = OpenURLOffTheRecord(
      profile()->GetOffTheRecordProfile(), chromium_org_url());
  content::RenderFrameHost* incognito_frame =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetMainFrame();

  IncognitoConnectability::ScopedAlertTracker alert_tracker(
      IncognitoConnectability::ScopedAlertTracker::ALWAYS_DENY);

  // |extension| won't be loaded in the incognito renderer since it's not
  // enabled for incognito. Since there is no externally connectible extension
  // loaded into the incognito renderer, the chrome.runtime API won't be
  // defined.
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToFrame(incognito_frame, extension.get(),
                                             nullptr));

  // Loading a platform app in the renderer should cause the chrome.runtime
  // bindings to be generated in the renderer. A platform app is always loaded
  // in the incognito renderer.
  LoadChromiumConnectableApp();
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessagesToFrame(incognito_frame, extension.get(),
                                             nullptr));

  // Allowing the extension in incognito mode loads the extension in the
  // incognito renderer, allowing it to receive connections.
  TestExtensionRegistryObserver observer(
      ExtensionRegistry::Get(profile()->GetOffTheRecordProfile()),
      extension->id());
  util::SetIsIncognitoEnabled(extension->id(),
                              profile()->GetOffTheRecordProfile(), true);
  const Extension* loaded_extension = observer.WaitForExtensionLoaded();
  EXPECT_EQ(OK, CanConnectAndSendMessagesToFrame(incognito_frame,
                                                 loaded_extension, nullptr));

  // No alert is shown for extensions since they support being enabled in
  // incognito mode.
  EXPECT_EQ(0, alert_tracker.GetAndResetAlertCount());
}

// Tests connection from incognito tabs when the extension doesn't have an event
// handler for the connection event.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIncognitoNoEventHandlerInApp) {
  scoped_refptr<const Extension> app = LoadChromiumConnectableApp(false);
  ASSERT_TRUE(app->is_platform_app());

  Browser* incognito_browser = OpenURLOffTheRecord(
      profile()->GetOffTheRecordProfile(), chromium_org_url());
  content::RenderFrameHost* incognito_frame =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetMainFrame();

  {
    IncognitoConnectability::ScopedAlertTracker alert_tracker(
        IncognitoConnectability::ScopedAlertTracker::ALWAYS_ALLOW);

    // No connection because incognito-enabled hasn't been set for the app, and
    // the app hasn't installed event handlers.
    EXPECT_EQ(
        COULD_NOT_ESTABLISH_CONNECTION_ERROR,
        CanConnectAndSendMessagesToFrame(incognito_frame, app.get(), NULL));
    // No dialog should have been shown.
    EXPECT_EQ(0, alert_tracker.GetAndResetAlertCount());
  }
}

// Tests connection from incognito tabs when the user accepts the connection
// request. Spanning mode only. Separate tests for apps and extensions.
//
// TODO(kalman): see comment above about split mode.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIncognitoAllowApp) {
  scoped_refptr<const Extension> app = LoadChromiumConnectableApp();
  ASSERT_TRUE(app->is_platform_app());

  Browser* incognito_browser = OpenURLOffTheRecord(
      profile()->GetOffTheRecordProfile(), chromium_org_url());
  content::RenderFrameHost* incognito_frame = incognito_browser->
      tab_strip_model()->GetActiveWebContents()->GetMainFrame();

  {
    IncognitoConnectability::ScopedAlertTracker alert_tracker(
        IncognitoConnectability::ScopedAlertTracker::ALWAYS_ALLOW);

    // Connection allowed even with incognito disabled, because the user
    // accepted the interactive request.
    EXPECT_EQ(
        OK, CanConnectAndSendMessagesToFrame(incognito_frame, app.get(), NULL));
    EXPECT_EQ(1, alert_tracker.GetAndResetAlertCount());

    // Try again. User has already allowed.
    EXPECT_EQ(
        OK, CanConnectAndSendMessagesToFrame(incognito_frame, app.get(), NULL));
    EXPECT_EQ(0, alert_tracker.GetAndResetAlertCount());
  }

  // Apps can't be allowed in incognito mode, but it's moot because it's
  // already allowed.
  ExtensionPrefs::Get(profile())->SetIsIncognitoEnabled(app->id(), true);
  EXPECT_EQ(OK,
            CanConnectAndSendMessagesToFrame(incognito_frame, app.get(), NULL));
}

// Tests connection from incognito tabs when there are multiple tabs open to the
// same origin. The user should only need to accept the connection request once.
// Flaky: https://crbug.com/940952.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       DISABLED_FromIncognitoPromptApp) {
  scoped_refptr<const Extension> app = LoadChromiumConnectableApp();
  ASSERT_TRUE(app->is_platform_app());

  // Open an incognito browser with two tabs displaying "chromium.org".
  Browser* incognito_browser = OpenURLOffTheRecord(
      profile()->GetOffTheRecordProfile(), chromium_org_url());
  content::RenderFrameHost* incognito_frame1 =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetMainFrame();
  InfoBarService* infobar_service1 = InfoBarService::FromWebContents(
      incognito_browser->tab_strip_model()->GetActiveWebContents());

  CHECK(OpenURLOffTheRecord(profile()->GetOffTheRecordProfile(),
                            chromium_org_url()) == incognito_browser);
  content::RenderFrameHost* incognito_frame2 =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetMainFrame();
  InfoBarService* infobar_service2 = InfoBarService::FromWebContents(
      incognito_browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(2, incognito_browser->tab_strip_model()->count());
  EXPECT_NE(incognito_frame1, incognito_frame2);

  // Trigger a infobars in both tabs by trying to send messages.
  std::string script =
      base::StringPrintf("assertions.trySendMessage('%s')", app->id().c_str());
  CHECK(content::ExecuteScript(incognito_frame1, script));
  CHECK(content::ExecuteScript(incognito_frame2, script));
  EXPECT_EQ(1U, infobar_service1->infobar_count());
  EXPECT_EQ(1U, infobar_service2->infobar_count());

  // Navigating away will dismiss the infobar on the active tab only.
  ui_test_utils::NavigateToURL(incognito_browser, google_com_url());
  EXPECT_EQ(1U, infobar_service1->infobar_count());
  EXPECT_EQ(0U, infobar_service2->infobar_count());

  // Navigate back and accept the infobar this time. Both should be dismissed.
  {
    IncognitoConnectability::ScopedAlertTracker alert_tracker(
        IncognitoConnectability::ScopedAlertTracker::ALWAYS_ALLOW);

    ui_test_utils::NavigateToURL(incognito_browser, chromium_org_url());
    incognito_frame2 = incognito_browser->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetMainFrame();
    EXPECT_NE(incognito_frame1, incognito_frame2);

    EXPECT_EQ(1U, infobar_service1->infobar_count());
    EXPECT_EQ(OK, CanConnectAndSendMessagesToFrame(incognito_frame2, app.get(),
                                                   NULL));
    EXPECT_EQ(1, alert_tracker.GetAndResetAlertCount());
    EXPECT_EQ(0U, infobar_service1->infobar_count());
  }
}

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest, IllegalArguments) {
  // Tests that malformed arguments to connect() don't crash.
  // Regression test for crbug.com/472700.
  LoadChromiumConnectableExtension();
  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  bool result;
  CHECK(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "assertions.tryIllegalArguments()", &result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIncognitoAllowExtension) {
  scoped_refptr<const Extension> extension = LoadChromiumConnectableExtension();
  EXPECT_FALSE(util::IsIncognitoEnabled(extension->id(), profile()));

  Browser* incognito_browser = OpenURLOffTheRecord(
      profile()->GetOffTheRecordProfile(), chromium_org_url());
  content::RenderFrameHost* incognito_frame =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetMainFrame();

  IncognitoConnectability::ScopedAlertTracker alert_tracker(
      IncognitoConnectability::ScopedAlertTracker::ALWAYS_ALLOW);

  // |extension| won't be loaded in the incognito renderer since it's not
  // enabled for incognito. Since there is no externally connectible extension
  // loaded into the incognito renderer, the chrome.runtime API won't be
  // defined.
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToFrame(incognito_frame, extension.get(),
                                             nullptr));

  // Allowing the extension in incognito mode loads the extension in the
  // incognito renderer, causing the chrome.runtime bindings to be generated in
  // the renderer and allowing the extension to receive connections.
  TestExtensionRegistryObserver observer(
      ExtensionRegistry::Get(profile()->GetOffTheRecordProfile()),
      extension->id());
  util::SetIsIncognitoEnabled(extension->id(),
                              profile()->GetOffTheRecordProfile(), true);
  const Extension* loaded_extension = observer.WaitForExtensionLoaded();
  EXPECT_EQ(OK, CanConnectAndSendMessagesToFrame(incognito_frame,
                                                 loaded_extension, nullptr));

  // No alert is shown for extensions which support being enabled in incognito
  // mode.
  EXPECT_EQ(0, alert_tracker.GetAndResetAlertCount());
}

// Tests a connection from an iframe within a tab which doesn't have
// permission. Iframe should work.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIframeWithPermission) {
  scoped_refptr<const Extension> extension = LoadChromiumConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), google_com_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToMainFrame(extension.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  ASSERT_TRUE(AppendIframe(chromium_org_url()));

  EXPECT_EQ(OK, CanConnectAndSendMessagesToIFrame(extension.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForIFrame());
}

// Tests connection from an iframe without permission within a tab that does.
// Iframe shouldn't work.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIframeWithoutPermission) {
  scoped_refptr<const Extension> extension = LoadChromiumConnectableExtension();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(OK, CanConnectAndSendMessagesToMainFrame(extension.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  ASSERT_TRUE(AppendIframe(google_com_url()));

  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToIFrame(extension.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForIFrame());
}

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest, FromPopup) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kDisablePopupBlocking);

  scoped_refptr<const Extension> extension = LoadChromiumConnectableExtension();

  // This will let us wait for the chromium.org.html page to load in a popup.
  ui_test_utils::UrlLoadObserver url_observer(
      chromium_org_url(), content::NotificationService::AllSources());

  // The page at popup_opener_url() should open chromium_org_url() as a popup.
  ui_test_utils::NavigateToURL(browser(), popup_opener_url());
  url_observer.Wait();

  // Find the WebContents that committed the chromium_org_url().
  // TODO(devlin) - it would be nice if UrlLoadObserver handled this for
  // us, which it could pretty easily do.
  content::WebContents* popup_contents = nullptr;
  for (int i = 0; i < browser()->tab_strip_model()->count(); i++) {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(i);
    if (contents->GetLastCommittedURL() == chromium_org_url()) {
      popup_contents = contents;
      break;
    }
  }
  ASSERT_NE(nullptr, popup_contents) << "Could not find WebContents for popup";

  // Make sure the popup can connect and send messages to the extension.
  content::RenderFrameHost* popup_frame = popup_contents->GetMainFrame();

  EXPECT_EQ(OK, CanConnectAndSendMessagesToFrame(popup_frame, extension.get(),
                                                 nullptr));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForFrame(popup_frame));
}

// TODO(devlin): Remove this subclass - it doesn't seem to do anything.
class ExternallyConnectableMessagingTestNoChannelID
    : public ExternallyConnectableMessagingTest {
 public:
  ExternallyConnectableMessagingTestNoChannelID() {}
  ~ExternallyConnectableMessagingTestNoChannelID() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExternallyConnectableMessagingTest::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternallyConnectableMessagingTestNoChannelID);
};

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTestNoChannelID,
                       TlsChannelIdEmptyWhenDisabled) {
  std::string expected_tls_channel_id_value;

  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtensionWithTlsChannelId();
  ASSERT_TRUE(chromium_connectable.get());

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());

  // Check that both connect and sendMessage don't report a Channel ID.
  std::string tls_channel_id_from_port_connect =
      GetTlsChannelIdFromPortConnect(chromium_connectable.get(), true);
  EXPECT_EQ(0u, tls_channel_id_from_port_connect.size());

  std::string tls_channel_id_from_send_message =
      GetTlsChannelIdFromSendMessage(chromium_connectable.get(), true);
  EXPECT_EQ(0u, tls_channel_id_from_send_message.size());
}

// Tests a web connectable extension that receives TLS channel id, but
// immediately closes its background page upon receipt of a message.
// Same flakiness seen in http://crbug.com/297866
IN_PROC_BROWSER_TEST_F(
    ExternallyConnectableMessagingTest,
    DISABLED_WebConnectableWithNonEmptyTlsChannelIdAndClosedBackgroundPage) {
  std::string expected_tls_channel_id_value;

  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtensionWithTlsChannelId();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  // If the page does ask for it, it isn't empty, even if the background page
  // closes upon receipt of the connect.
  std::string tls_channel_id = GetTlsChannelIdFromPortConnect(
      chromium_connectable.get(), true, close_background_message());
  EXPECT_EQ(expected_tls_channel_id_value, tls_channel_id);
  // A subsequent connect will still succeed, even if the background page was
  // previously closed.
  tls_channel_id =
      GetTlsChannelIdFromPortConnect(chromium_connectable.get(), true);
  // And the expected value is still retrieved.
  EXPECT_EQ(expected_tls_channel_id_value, tls_channel_id);
}

IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingUserGesture) {
  const char kManifest[] = "{"
                          "  \"name\": \"user_gesture\","
                          "  \"version\": \"1.0\","
                          "  \"background\": {"
                          "    \"scripts\": [\"background.js\"]"
                          "  },"
                          "  \"manifest_version\": 2"
                          "}";

  TestExtensionDir receiver_dir;
  receiver_dir.WriteManifest(kManifest);
  receiver_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
      "chrome.runtime.onMessageExternal.addListener(\n"
      "    function(msg, sender, reply) {\n"
      "      reply({result:chrome.test.isProcessingUserGesture()});\n"
      "    });");
  const Extension* receiver = LoadExtension(receiver_dir.UnpackedPath());
  ASSERT_TRUE(receiver);

  TestExtensionDir sender_dir;
  sender_dir.WriteManifest(kManifest);
  sender_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
  const Extension* sender = LoadExtension(sender_dir.UnpackedPath());
  ASSERT_TRUE(sender);

  EXPECT_EQ(
      "false",
      ExecuteScriptInBackgroundPage(
          sender->id(),
          base::StringPrintf(
              "if (chrome.test.isProcessingUserGesture()) {\n"
              "  domAutomationController.send("
              "      'Error: unexpected user gesture');\n"
              "} else {\n"
              "  chrome.runtime.sendMessage('%s', {}, function(response) {\n"
              "    domAutomationController.send('' + response.result);\n"
              "  });\n"
              "}",
              receiver->id().c_str()),
          extensions::browsertest_util::ScriptUserActivation::kDontActivate));

  EXPECT_EQ("true",
      ExecuteScriptInBackgroundPage(sender->id(),
                                    base::StringPrintf(
          "chrome.test.runWithUserGesture(function() {\n"
          "  chrome.runtime.sendMessage('%s', {}, function(response)  {\n"
          "    window.domAutomationController.send('' + response.result);\n"
          "  });\n"
          "});", receiver->id().c_str())));
}

// Tests that a hosted app on a connectable site doesn't interfere with the
// connectability of that site.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest, HostedAppOnWebsite) {
  scoped_refptr<const Extension> app = LoadChromiumHostedApp();

  // The presence of the hosted app shouldn't give the ability to send messages.
  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToMainFrame(app.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  // Once a connectable extension is installed, it should.
  scoped_refptr<const Extension> extension = LoadChromiumConnectableExtension();
  EXPECT_EQ(OK, CanConnectAndSendMessagesToMainFrame(extension.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());
}

// Tests that an invalid extension ID specified in a hosted app does not crash
// the hosted app's renderer.
//
// This is a regression test for http://crbug.com/326250#c12.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       InvalidExtensionIDFromHostedApp) {
  // The presence of the chromium hosted app triggers this bug. The chromium
  // connectable extension needs to be installed to set up the runtime bindings.
  LoadChromiumHostedApp();
  LoadChromiumConnectableExtension();

  scoped_refptr<const Extension> invalid =
      ExtensionBuilder()
          .SetID(crx_file::id_util::GenerateId("invalid"))
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Fake extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();

  ui_test_utils::NavigateToURL(browser(), chromium_org_url());
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessagesToMainFrame(invalid.get()));
}

#endif  // !defined(OS_WIN) - http://crbug.com/350517.

// Tests that messages sent in the unload handler of a window arrive.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingOnUnload) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("messaging/on_unload"));
  ExtensionTestMessageListener listener("listening", false);
  ASSERT_TRUE(extension);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/empty.html"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  ExtensionHost* background_host =
      ProcessManager::Get(profile())->GetBackgroundHostForExtension(
          extension->id());
  ASSERT_TRUE(background_host);
  content::WebContents* background_contents = background_host->host_contents();
  ASSERT_TRUE(background_contents);
  int message_count = -1;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      background_contents,
      "window.domAutomationController.send(window.messageCount);",
      &message_count));
  // There shouldn't be any messages yet.
  EXPECT_EQ(0, message_count);

  content::WebContentsDestroyedWatcher destroyed_watcher(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::CloseTab(browser());
  destroyed_watcher.Wait();
  base::RunLoop().RunUntilIdle();
  // The extension should have sent a message from its unload handler.
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      background_contents,
      "window.domAutomationController.send(window.messageCount);",
      &message_count));
  EXPECT_EQ(1, message_count);
}

// Tests that messages over a certain size are not sent.
// https://crbug.com/766713.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, LargeMessages) {
  ASSERT_TRUE(RunExtensionTest("messaging/large_messages"));
}

}  // namespace

}  // namespace extensions
