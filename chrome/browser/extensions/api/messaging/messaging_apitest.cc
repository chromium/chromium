// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "components/embedder_support/switches.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/runtime.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace extensions {
namespace {

class MessageSender : public ExtensionHostRegistry::Observer {
 public:
  explicit MessageSender(content::BrowserContext* browser_context) {
    host_registry_observation_.Observe(
        ExtensionHostRegistry::Get(browser_context));
  }

 private:
  static base::Value::List BuildEventArguments(const bool last_message,
                                               const std::string& data) {
    return base::Value::List().Append(
        base::Value::Dict().Set("lastMessage", last_message).Set("data", data));
  }

  static std::unique_ptr<Event> BuildEvent(
      base::Value::List event_args,
      content::BrowserContext* browser_context,
      GURL event_url) {
    auto event =
        std::make_unique<Event>(events::TEST_ON_MESSAGE, "test.onMessage",
                                std::move(event_args), browser_context);
    event->event_url = std::move(event_url);
    return event;
  }

  // ExtensionHostRegistry::Observer:
  void OnExtensionHostCompletedFirstLoad(
      content::BrowserContext* browser_context,
      ExtensionHost* extension_host) override {
    EventRouter* event_router = EventRouter::Get(browser_context);

    // Sends four messages to the extension. All but the third message sent
    // from the origin http://b.com/ are supposed to arrive.
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(false, "no restriction"), browser_context, GURL()));
    event_router->BroadcastEvent(
        BuildEvent(BuildEventArguments(false, "http://a.com/"), browser_context,
                   GURL("http://a.com/")));
    event_router->BroadcastEvent(
        BuildEvent(BuildEventArguments(false, "http://b.com/"), browser_context,
                   GURL("http://b.com/")));
    event_router->BroadcastEvent(BuildEvent(
        BuildEventArguments(true, "last message"), browser_context, GURL()));
  }

  base::ScopedObservation<ExtensionHostRegistry,
                          ExtensionHostRegistry::Observer>
      host_registry_observation_{this};
};

class MessagingApiTest : public ExtensionApiTest {
 public:
  explicit MessagingApiTest(
      bool enable_back_forward_cache = true,
      bool disconnect_extension_port_when_page_enters_bfcache = true) {
    if (!enable_back_forward_cache) {
      feature_list_.InitWithFeaturesAndParameters(
          {}, {features::kBackForwardCache});
      return;
    }

    std::vector<base::test::FeatureRefAndParams> enabled_features =
        content::GetBasicBackForwardCacheFeatureForTesting();
    std::vector<base::test::FeatureRef> disabled_features =
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting();

    if (disconnect_extension_port_when_page_enters_bfcache) {
      enabled_features.push_back(
          {features::kDisconnectExtensionMessagePortWhenPageEntersBFCache, {}});
    } else {
      disabled_features.push_back(
          features::kDisconnectExtensionMessagePortWhenPageEntersBFCache);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  MessagingApiTest(const MessagingApiTest&) = delete;
  MessagingApiTest& operator=(const MessagingApiTest&) = delete;

  ~MessagingApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class MessagingApiWithoutDisconnectExtensionMessagePortWhenPageEntersBFCacheTest
    : public MessagingApiTest {
 public:
  MessagingApiWithoutDisconnectExtensionMessagePortWhenPageEntersBFCacheTest()
      : MessagingApiTest(
            /*enable_back_forward_cache=*/true,
            /*disconnect_extension_port_when_page_enters_bfcache=*/false) {}
};

class MessagingApiWithoutBackForwardCacheTest : public MessagingApiTest {
 public:
  MessagingApiWithoutBackForwardCacheTest()
      : MessagingApiTest(/*enable_back_forward_cache=*/false) {}
};

IN_PROC_BROWSER_TEST_F(MessagingApiTest, Messaging) {
  ASSERT_TRUE(RunExtensionTest("messaging/connect", {.custom_arg = "bfcache"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(
    MessagingApiWithoutDisconnectExtensionMessagePortWhenPageEntersBFCacheTest,
    Messaging) {
  ASSERT_TRUE(RunExtensionTest("messaging/connect",
                               {.custom_arg = "bfcache/without_disconnection"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MessagingApiWithoutBackForwardCacheTest, Messaging) {
  ASSERT_TRUE(RunExtensionTest("messaging/connect")) << message_;
}

IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingCrash) {
  ExtensionTestMessageListener ready_to_crash("ready_to_crash");
  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("messaging/connect_crash")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html")));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(ready_to_crash.WaitUntilSatisfied());

  ResultCatcher catcher;
  CrashTab(tab);
  EXPECT_TRUE(catcher.GetNextResult());
}

// Tests sendMessage cases where the listener gets disconnected before it is
// able to reply with a message it said it would send. This is achieved by
// closing the page the listener is registered on.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, SendMessageDisconnect) {
  static constexpr char kManifest[] = R"(
      {
        "name": "sendMessageDisconnect",
        "version": "1.0",
        "manifest_version": 3,
        "background": {
          "service_worker": "test.js",
          "type": "module"
        }
      })";

  static constexpr char kListenerPage[] = R"(
    <script src="listener.js"></script>
  )";
  static constexpr char kListenerJS[] = R"(
    var sendResponseCallback;
    chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
      // Store the callback and return true to indicate we intend to respond
      // with it later. We store the callback because the port would be closed
      // automatically if it is garbage collected.
      sendResponseCallback = sendResponse;

      // Have the page close itself after a short delay to trigger the
      // disconnect.
      setTimeout(window.close, 0);
      return true;
    });
  )";
  static constexpr char kTestJS[] = R"(
    import {openTab} from '/_test_resources/test_util/tabs_util.js';
    let expectedError = 'A listener indicated an asynchronous response by ' +
        'returning true, but the message channel closed before a response ' +
        'was received';
    chrome.test.runTests([
      async function sendMessageWithCallbackExpectingUnsentAsyncResponse() {
        // Open the page which has the listener.
        let tab = await openTab(chrome.runtime.getURL('listener.html'));
        chrome.tabs.sendMessage(tab.id, 'async_true', (response) => {
          chrome.test.assertLastError(expectedError);
          chrome.test.succeed();
        });
      },

      async function sendMessageWithPromiseExpectingUnsentAsyncResponse() {
        // Open the page which has the listener.
        let tab = await openTab(chrome.runtime.getURL('listener.html'));
        chrome.runtime.sendMessage('async_true').then(() => {
          chrome.test.fail('Message unexpectedly succeeded');
        }).catch((error) => {
          chrome.test.assertEq(expectedError, error.message);
          chrome.test.succeed();
        });
      },
    ]);
  )";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("listener.html"), kListenerPage);
  dir.WriteFile(FILE_PATH_LITERAL("listener.js"), kListenerJS);
  dir.WriteFile(FILE_PATH_LITERAL("test.js"), kTestJS);

  ASSERT_TRUE(RunExtensionTest(dir.UnpackedPath(), {}, {}));
}

// Tests that message passing from one extension to another works.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingExternal) {
  ASSERT_TRUE(LoadExtension(
      shared_test_data_dir().AppendASCII("messaging").AppendASCII("receiver")));

  ASSERT_TRUE(RunExtensionTest("messaging/connect_external",
                               {.use_extensions_root_dir = true}))
      << message_;
}

// Tests that a content script can exchange messages with a tab even if there is
// no background page.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingNoBackground) {
  ASSERT_TRUE(RunExtensionTest("messaging/connect_nobackground",
                               {.extension_url = "page_in_main_frame.html"}))
      << message_;
}

// Tests that messages with event_urls are only passed to extensions with
// appropriate permissions.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingEventURL) {
  MessageSender sender(profile());
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
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "actions.appendIframe('" + src.spec() + "');")
        .ExtractBool();
  }

  Result CanConnectAndSendMessagesToMainFrame(const Extension* extension,
                                              const char* message = nullptr) {
    return CanConnectAndSendMessagesToFrame(browser()
                                                ->tab_strip_model()
                                                ->GetActiveWebContents()
                                                ->GetPrimaryMainFrame(),
                                            extension, message);
  }

  Result CanConnectAndSendMessagesToIFrame(const Extension* extension,
                                           const char* message = nullptr) {
    content::RenderFrameHost* frame = content::FrameMatchingPredicate(
        browser()->tab_strip_model()->GetActiveWebContents()->GetPrimaryPage(),
        base::BindRepeating(&content::FrameIsChildOfMainFrame));
    return CanConnectAndSendMessagesToFrame(frame, extension, message);
  }

  Result CanConnectAndSendMessagesToFrame(content::RenderFrameHost* frame,
                                          const Extension* extension,
                                          const char* message) {
    std::string command = base::StringPrintf(
        "assertions.canConnectAndSendMessages('%s', %s, %s)",
        extension->id().c_str(),
        extension->is_platform_app() ? "true" : "false",
        message ? base::StringPrintf("'%s'", message).c_str() : "undefined");
    int result = content::EvalJs(frame, command).ExtractInt();
    return static_cast<Result>(result);
  }

  Result CanUseSendMessagePromise(const Extension* extension) {
    content::RenderFrameHost* frame = browser()
                                          ->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetPrimaryMainFrame();
    std::string command =
        content::JsReplace("assertions.canUseSendMessagePromise($1, $2)",
                           extension->id(), extension->is_platform_app());
    int result = content::EvalJs(frame, command).ExtractInt();
    return static_cast<Result>(result);
  }

  testing::AssertionResult AreAnyNonWebApisDefinedForMainFrame() {
    return AreAnyNonWebApisDefinedForFrame(browser()
                                               ->tab_strip_model()
                                               ->GetActiveWebContents()
                                               ->GetPrimaryMainFrame());
  }

  testing::AssertionResult AreAnyNonWebApisDefinedForIFrame() {
    content::RenderFrameHost* frame = content::FrameMatchingPredicate(
        browser()->tab_strip_model()->GetActiveWebContents()->GetPrimaryPage(),
        base::BindRepeating(&content::FrameIsChildOfMainFrame));
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
    for (const auto* non_messaging_api : non_messaging_apis) {
      as_js_array += as_js_array.empty() ? "[" : ",";
      as_js_array += base::StringPrintf("'%s'", non_messaging_api);
    }
    as_js_array += "]";

    bool any_defined =
        content::EvalJs(frame, "assertions.areAnyRuntimePropertiesDefined(" +
                                   as_js_array + ")")
            .ExtractBool();
    return any_defined ?
        testing::AssertionSuccess() : testing::AssertionFailure();
  }

  std::string GetTlsChannelIdFromPortConnect(const Extension* extension,
                                             bool include_tls_channel_id,
                                             const char* message = nullptr) {
    return GetTlsChannelIdFromAssertion("getTlsChannelIdFromPortConnect",
                                        extension,
                                        include_tls_channel_id,
                                        message);
  }

  std::string GetTlsChannelIdFromSendMessage(const Extension* extension,
                                             bool include_tls_channel_id,
                                             const char* message = nullptr) {
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
    std::string args = "'" + extension->id() + "', ";
    args += include_tls_channel_id ? "true" : "false";
    if (message)
      args += std::string(", '") + message + "'";
    return content::EvalJs(
               browser()->tab_strip_model()->GetActiveWebContents(),
               base::StringPrintf("assertions.%s(%s)", method, args.c_str()))
        .ExtractString();
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
          .SetManifest(base::Value::Dict()
                           .Set("name", "Fake extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2))
          .Build();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToMainFrame(extension.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_com_url()));
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToMainFrame(extension.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());
}

// TODO(kalman): Most web messaging tests disabled on windows due to extreme
// flakiness. See http://crbug.com/350517.
#if !BUILDFLAG(IS_WIN)

// Tests two extensions on the same sites: one web connectable, one not.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       WebConnectableAndNotConnectable) {
  // Install the web connectable extension. chromium.org can connect to it,
  // google.com can't.
  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtension();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
  EXPECT_EQ(OK,
            CanConnectAndSendMessagesToMainFrame(chromium_connectable.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_com_url()));
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToMainFrame(chromium_connectable.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  // Install the non-connectable extension. Nothing can connect to it.
  scoped_refptr<const Extension> not_connectable =
      LoadNotConnectableExtension();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
  // Namespace will be defined here because |chromium_connectable| can connect
  // to it - so this will be the "cannot establish connection" error.
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessagesToMainFrame(not_connectable.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_com_url()));
  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToMainFrame(not_connectable.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());
}

// Tests that an externally connectable web page context can use the promise
// based form of sendMessage.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       SendMessagePromiseSignatureExposed) {
  // Install the web connectable extension.
  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtension();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
  EXPECT_EQ(OK, CanUseSendMessagePromise(chromium_connectable.get()));
}

// See http://crbug.com/297866
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       DISABLED_BackgroundPageClosesOnMessageReceipt) {
  // Install the web connectable extension.
  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtension();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_com_url()));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));

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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
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
  // TODO(crbug.com/40937027): Convert test to use HTTPS and then remove.
  ScopedAllowHttpForHostnamesForTesting allow_http({"www.chromium.org"},
                                                   profile()->GetPrefs());

  scoped_refptr<const Extension> app = LoadChromiumConnectableApp();
  ASSERT_TRUE(app->is_platform_app());

  Browser* incognito_browser = OpenURLOffTheRecord(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      chromium_org_url());
  content::RenderFrameHost* incognito_frame =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame();

  {
    IncognitoConnectability::ScopedAlertTracker alert_tracker(
        IncognitoConnectability::ScopedAlertTracker::ALWAYS_DENY);

    // No connection because incognito-enabled hasn't been set for the app, and
    // the user denied our interactive request.
    EXPECT_EQ(
        COULD_NOT_ESTABLISH_CONNECTION_ERROR,
        CanConnectAndSendMessagesToFrame(incognito_frame, app.get(), nullptr));
    EXPECT_EQ(1, alert_tracker.GetAndResetAlertCount());

    // Try again. User has already denied so alert not shown.
    EXPECT_EQ(
        COULD_NOT_ESTABLISH_CONNECTION_ERROR,
        CanConnectAndSendMessagesToFrame(incognito_frame, app.get(), nullptr));
    EXPECT_EQ(0, alert_tracker.GetAndResetAlertCount());
  }

  // It's not possible to allow an app in incognito.
  ExtensionPrefs::Get(profile())->SetIsIncognitoEnabled(app->id(), true);
  EXPECT_EQ(
      COULD_NOT_ESTABLISH_CONNECTION_ERROR,
      CanConnectAndSendMessagesToFrame(incognito_frame, app.get(), nullptr));
}

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIncognitoDenyExtensionAndApp) {
  // TODO(crbug.com/40937027): Convert test to use HTTPS and then remove.
  ScopedAllowHttpForHostnamesForTesting allow_http({"www.chromium.org"},
                                                   profile()->GetPrefs());

  scoped_refptr<const Extension> extension = LoadChromiumConnectableExtension();
  EXPECT_FALSE(util::IsIncognitoEnabled(extension->id(), profile()));

  Browser* incognito_browser = OpenURLOffTheRecord(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      chromium_org_url());
  content::RenderFrameHost* incognito_frame =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame();

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
      ExtensionRegistry::Get(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)),
      extension->id());
  util::SetIsIncognitoEnabled(
      extension->id(),
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), true);
  scoped_refptr<const Extension> loaded_extension =
      observer.WaitForExtensionLoaded();
  EXPECT_EQ(OK, CanConnectAndSendMessagesToFrame(
                    incognito_frame, loaded_extension.get(), nullptr));

  // No alert is shown for extensions since they support being enabled in
  // incognito mode.
  EXPECT_EQ(0, alert_tracker.GetAndResetAlertCount());
}

// Tests connection from incognito tabs when the extension doesn't have an event
// handler for the connection event.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIncognitoNoEventHandlerInApp) {
  // TODO(crbug.com/40937027): Convert test to use HTTPS and then remove.
  ScopedAllowHttpForHostnamesForTesting allow_http({"www.chromium.org"},
                                                   profile()->GetPrefs());

  scoped_refptr<const Extension> app = LoadChromiumConnectableApp(false);
  ASSERT_TRUE(app->is_platform_app());

  Browser* incognito_browser = OpenURLOffTheRecord(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      chromium_org_url());
  content::RenderFrameHost* incognito_frame =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame();

  {
    IncognitoConnectability::ScopedAlertTracker alert_tracker(
        IncognitoConnectability::ScopedAlertTracker::ALWAYS_ALLOW);

    // No connection because incognito-enabled hasn't been set for the app, and
    // the app hasn't installed event handlers.
    EXPECT_EQ(
        COULD_NOT_ESTABLISH_CONNECTION_ERROR,
        CanConnectAndSendMessagesToFrame(incognito_frame, app.get(), nullptr));
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
  // TODO(crbug.com/40937027): Convert test to use HTTPS and then remove.
  ScopedAllowHttpForHostnamesForTesting allow_http({"www.chromium.org"},
                                                   profile()->GetPrefs());

  scoped_refptr<const Extension> app = LoadChromiumConnectableApp();
  ASSERT_TRUE(app->is_platform_app());

  Browser* incognito_browser = OpenURLOffTheRecord(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      chromium_org_url());
  content::RenderFrameHost* incognito_frame =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame();

  {
    IncognitoConnectability::ScopedAlertTracker alert_tracker(
        IncognitoConnectability::ScopedAlertTracker::ALWAYS_ALLOW);

    // Connection allowed even with incognito disabled, because the user
    // accepted the interactive request.
    EXPECT_EQ(OK, CanConnectAndSendMessagesToFrame(incognito_frame, app.get(),
                                                   nullptr));
    EXPECT_EQ(1, alert_tracker.GetAndResetAlertCount());

    // Try again. User has already allowed.
    EXPECT_EQ(OK, CanConnectAndSendMessagesToFrame(incognito_frame, app.get(),
                                                   nullptr));
    EXPECT_EQ(0, alert_tracker.GetAndResetAlertCount());
  }

  // Apps can't be allowed in incognito mode, but it's moot because it's
  // already allowed.
  ExtensionPrefs::Get(profile())->SetIsIncognitoEnabled(app->id(), true);
  EXPECT_EQ(OK, CanConnectAndSendMessagesToFrame(incognito_frame, app.get(),
                                                 nullptr));
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
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      chromium_org_url());
  content::RenderFrameHost* incognito_frame1 =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame();
  infobars::ContentInfoBarManager* infobar_manager1 =
      infobars::ContentInfoBarManager::FromWebContents(
          incognito_browser->tab_strip_model()->GetActiveWebContents());

  CHECK(OpenURLOffTheRecord(
            profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
            chromium_org_url()) == incognito_browser);
  content::RenderFrameHost* incognito_frame2 =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame();
  infobars::ContentInfoBarManager* infobar_manager2 =
      infobars::ContentInfoBarManager::FromWebContents(
          incognito_browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(2, incognito_browser->tab_strip_model()->count());
  EXPECT_NE(incognito_frame1, incognito_frame2);

  // Trigger a infobars in both tabs by trying to send messages.
  std::string script =
      base::StringPrintf("assertions.trySendMessage('%s')", app->id().c_str());
  CHECK(content::ExecJs(incognito_frame1, script));
  CHECK(content::ExecJs(incognito_frame2, script));
  EXPECT_EQ(1U, infobar_manager1->infobars().size());
  EXPECT_EQ(1U, infobar_manager2->infobars().size());

  // Navigating away will dismiss the infobar on the active tab only.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(incognito_browser, google_com_url()));
  EXPECT_EQ(1U, infobar_manager1->infobars().size());
  EXPECT_EQ(0U, infobar_manager2->infobars().size());

  // Navigate back and accept the infobar this time. Both should be dismissed.
  {
    IncognitoConnectability::ScopedAlertTracker alert_tracker(
        IncognitoConnectability::ScopedAlertTracker::ALWAYS_ALLOW);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(incognito_browser, chromium_org_url()));
    incognito_frame2 = incognito_browser->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetPrimaryMainFrame();
    EXPECT_NE(incognito_frame1, incognito_frame2);

    EXPECT_EQ(1U, infobar_manager1->infobars().size());
    EXPECT_EQ(OK, CanConnectAndSendMessagesToFrame(incognito_frame2, app.get(),
                                                   nullptr));
    EXPECT_EQ(1, alert_tracker.GetAndResetAlertCount());
    EXPECT_EQ(0U, infobar_manager1->infobars().size());
  }
}

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest, IllegalArguments) {
  // Tests that malformed arguments to connect() don't crash.
  // Regression test for crbug.com/472700.
  LoadChromiumConnectableExtension();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
  EXPECT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "assertions.tryIllegalArguments()"));
}

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIncognitoAllowExtension) {
  // TODO(crbug.com/40937027): Convert test to use HTTPS and then remove.
  ScopedAllowHttpForHostnamesForTesting allow_http({"www.chromium.org"},
                                                   profile()->GetPrefs());

  scoped_refptr<const Extension> extension = LoadChromiumConnectableExtension();
  EXPECT_FALSE(util::IsIncognitoEnabled(extension->id(), profile()));

  Browser* incognito_browser = OpenURLOffTheRecord(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      chromium_org_url());
  content::RenderFrameHost* incognito_frame =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame();

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
      ExtensionRegistry::Get(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)),
      extension->id());
  util::SetIsIncognitoEnabled(
      extension->id(),
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), true);
  scoped_refptr<const Extension> loaded_extension =
      observer.WaitForExtensionLoaded();
  EXPECT_EQ(OK, CanConnectAndSendMessagesToFrame(
                    incognito_frame, loaded_extension.get(), nullptr));

  // No alert is shown for extensions which support being enabled in incognito
  // mode.
  EXPECT_EQ(0, alert_tracker.GetAndResetAlertCount());
}

// Tests a connection from an iframe within a tab which doesn't have
// permission. Iframe should work.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest,
                       FromIframeWithPermission) {
  scoped_refptr<const Extension> extension = LoadChromiumConnectableExtension();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_com_url()));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
  EXPECT_EQ(OK, CanConnectAndSendMessagesToMainFrame(extension.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForMainFrame());

  ASSERT_TRUE(AppendIframe(google_com_url()));

  EXPECT_EQ(NAMESPACE_NOT_DEFINED,
            CanConnectAndSendMessagesToIFrame(extension.get()));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForIFrame());
}

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest, FromPopup) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      embedder_support::kDisablePopupBlocking);

  scoped_refptr<const Extension> extension = LoadChromiumConnectableExtension();

  // This will let us wait for the chromium.org.html page to load in a popup.
  ui_test_utils::UrlLoadObserver url_observer(chromium_org_url());

  // The page at popup_opener_url() should open chromium_org_url() as a popup.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), popup_opener_url()));
  url_observer.Wait();

  content::WebContents* popup_contents = url_observer.web_contents();
  ASSERT_NE(nullptr, popup_contents) << "Could not find WebContents for popup";

  // Make sure the popup can connect and send messages to the extension.
  content::RenderFrameHost* popup_frame = popup_contents->GetPrimaryMainFrame();

  EXPECT_EQ(OK, CanConnectAndSendMessagesToFrame(popup_frame, extension.get(),
                                                 nullptr));
  EXPECT_FALSE(AreAnyNonWebApisDefinedForFrame(popup_frame));
}

// TODO(devlin): Remove this subclass - it doesn't seem to do anything.
class ExternallyConnectableMessagingTestNoChannelID
    : public ExternallyConnectableMessagingTest {
 public:
  ExternallyConnectableMessagingTestNoChannelID() {}

  ExternallyConnectableMessagingTestNoChannelID(
      const ExternallyConnectableMessagingTestNoChannelID&) = delete;
  ExternallyConnectableMessagingTestNoChannelID& operator=(
      const ExternallyConnectableMessagingTestNoChannelID&) = delete;

  ~ExternallyConnectableMessagingTestNoChannelID() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExternallyConnectableMessagingTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTestNoChannelID,
                       TlsChannelIdEmptyWhenDisabled) {
  std::string expected_tls_channel_id_value;

  scoped_refptr<const Extension> chromium_connectable =
      LoadChromiumConnectableExtensionWithTlsChannelId();
  ASSERT_TRUE(chromium_connectable.get());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));

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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
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
              "  chrome.test.sendScriptResult("
              "      'Error: unexpected user gesture');\n"
              "} else {\n"
              "  chrome.runtime.sendMessage('%s', {}, function(response) {\n"
              "    chrome.test.sendScriptResult('' + response.result);\n"
              "  });\n"
              "}",
              receiver->id().c_str()),
          extensions::browsertest_util::ScriptUserActivation::kDontActivate));

  EXPECT_EQ(
      "true",
      ExecuteScriptInBackgroundPage(
          sender->id(),
          base::StringPrintf(
              "chrome.test.runWithUserGesture(function() {\n"
              "  chrome.runtime.sendMessage('%s', {}, function(response)  {\n"
              "    chrome.test.sendScriptResult('' + response.result);\n"
              "  });\n"
              "});",
              receiver->id().c_str())));
}

IN_PROC_BROWSER_TEST_F(MessagingApiTest, UserGestureFromContentScript) {
  static constexpr char kBackground[] = R"(
    chrome.runtime.onMessage.addListener(function() {
      chrome.test.assertTrue(chrome.test.isProcessingUserGesture());
      chrome.test.notifyPass();
    });
  )";

  static constexpr char kContentScript[] = R"(
    chrome.test.runWithUserGesture(function() {
      chrome.runtime.sendMessage('');
    });
  )";

  static constexpr char kManifest[] = R"(
    {
      "name": "Test user gesture from content script.",
      "version": "1.0",
      "manifest_version": 3,
      "background": {
        "service_worker": "background.js"
      },
      "content_scripts": [{
        "matches": ["*://example.com/*"],
        "js": ["content_script.js"]
      }]
    }
  )";

  TestExtensionDir test_dir;
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);
  test_dir.WriteManifest(kManifest);

  GURL url = embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(),
                               {.page_url = url.spec().c_str()}, {}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MessagingApiTest, UserGestureFromExtensionPage) {
  static constexpr char kBackground[] = R"(
    chrome.runtime.onMessage.addListener(function() {
      chrome.test.assertTrue(chrome.test.isProcessingUserGesture());
      chrome.test.notifyPass();
    });
  )";

  static constexpr char kPage[] = R"(
    <script src='page.js'></script>
  )";

  static constexpr char kScript[] = R"(
    chrome.test.runWithUserGesture(function() {
      chrome.runtime.sendMessage('');
    });
  )";

  static constexpr char kManifest[] = R"(
    {
      "name": "Test user gesture from extension page.",
      "version": "1.0",
      "manifest_version": 3,
      "background": {
        "service_worker": "background.js"
      }
    }
  )";

  TestExtensionDir test_dir;
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPage);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kScript);
  test_dir.WriteManifest(kManifest);

  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(),
                               {.extension_url = "page.html"}, {}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MessagingApiTest,
                       RestrictedActivationTriggerBetweenExtensions) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      embedder_support::kDisablePopupBlocking);

  static constexpr char kManifest[] = R"({
    "name": "activation_state_thru_send_reply",
    "version": "1.0",
    "background": {
      "scripts": ["background.js"]
    },
    "manifest_version": 2
  })";

  // The receiver replies back with its transient activation state after a
  // delay.
  TestExtensionDir receiver_dir;
  receiver_dir.WriteManifest(kManifest);
  receiver_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                         R"(
        chrome.runtime.onMessageExternal.addListener(
          (msg, sender, callback) => {
            setTimeout(() =>
              callback({active:navigator.userActivation.isActive}), 200);
          });
      )");
  const Extension* receiver = LoadExtension(receiver_dir.UnpackedPath());
  ASSERT_TRUE(receiver);

  TestExtensionDir sender_dir;
  sender_dir.WriteManifest(kManifest);
  sender_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
  const Extension* sender = LoadExtension(sender_dir.UnpackedPath());
  ASSERT_TRUE(sender);

  static constexpr char send_script_template[] = R"(
    log = [];
    log.push('sender-initial:' + navigator.userActivation.isActive);
    chrome.runtime.sendMessage('%s', {}, response => {
      log.push('receiver:' + response.active);
      log.push('sender-received:' + navigator.userActivation.isActive);
      chrome.test.sendScriptResult(log.toString());
    });
    log.push('sender-sent:' + navigator.userActivation.isActive);
  )";
  std::string send_script =
      base::StringPrintf(send_script_template, receiver->id().c_str());

  // Without any user activation, neither the sender nor the receiver should be
  // in active state at any moment.
  EXPECT_EQ(
      "sender-initial:false,sender-sent:false,receiver:false,"
      "sender-received:false",
      ExecuteScriptInBackgroundPage(
          sender->id(), send_script,
          extensions::browsertest_util::ScriptUserActivation::kDontActivate));

  // With user activation before sending, the sender should be in active state
  // all the time, and the receiver should be in active state.
  //
  // TODO(crbug.com/40094773): The receiver should be inactive here.
  EXPECT_EQ(
      "sender-initial:true,sender-sent:true,receiver:true,"
      "sender-received:true",
      ExecuteScriptInBackgroundPage(
          sender->id(), send_script,
          extensions::browsertest_util::ScriptUserActivation::kActivate));

  std::string send_and_consume_script = send_script + R"(
    setTimeout(() => {
      open().close();
      log.push('sender-consumed:' + navigator.userActivation.isActive);
    }, 0);
  )";

  // With user activation consumed right after sending, the sender should be in
  // active state until consumption, and the receiver should be in active state.
  //
  // TODO(crbug.com/40094773): The receiver should be inactive here.
  EXPECT_EQ(
      "sender-initial:true,sender-sent:true,sender-consumed:false,"
      "receiver:true,sender-received:false",
      ExecuteScriptInBackgroundPage(
          sender->id(), send_and_consume_script,
          extensions::browsertest_util::ScriptUserActivation::kActivate));
}

// Tests that a hosted app on a connectable site doesn't interfere with the
// connectability of that site.
IN_PROC_BROWSER_TEST_F(ExternallyConnectableMessagingTest, HostedAppOnWebsite) {
  scoped_refptr<const Extension> app = LoadChromiumHostedApp();

  // The presence of the hosted app shouldn't give the ability to send messages.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
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
          .SetManifest(base::Value::Dict()
                           .Set("name", "Fake extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2))
          .Build();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_org_url()));
  EXPECT_EQ(COULD_NOT_ESTABLISH_CONNECTION_ERROR,
            CanConnectAndSendMessagesToMainFrame(invalid.get()));
}

#endif  // !BUILDFLAG(IS_WIN)

// Tests that messages sent in the pagehide handler of a window arrive.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingOnPagehide) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("messaging/on_pagehide"));
  ExtensionTestMessageListener listener("listening");
  ASSERT_TRUE(extension);
  // Open a new tab to example.com. Since we'll be closing it later, we need
  // to make sure there's still a tab around to extend the life of the
  // browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("example.com", "/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  ExtensionHost* background_host =
      ProcessManager::Get(profile())->GetBackgroundHostForExtension(
          extension->id());
  ASSERT_TRUE(background_host);
  content::WebContents* background_contents = background_host->host_contents();
  ASSERT_TRUE(background_contents);
  // There shouldn't be any messages yet.
  EXPECT_EQ(0, content::EvalJs(background_contents, "window.messageCount;"));

  content::WebContentsDestroyedWatcher destroyed_watcher(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::CloseTab(browser());
  destroyed_watcher.Wait();
  base::RunLoop().RunUntilIdle();
  // The extension should have sent a message from its pagehide handler.
  EXPECT_EQ(1, content::EvalJs(background_contents, "window.messageCount;"));
}

// Tests that messages over a certain size are not sent.
// https://crbug.com/766713.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, LargeMessages) {
  ASSERT_TRUE(RunExtensionTest("messaging/large_messages"));
}

// Tests that the channel name used in runtime.connect() cannot redirect the
// message to another event (like onMessage).
// See https://crbug.com/1430999.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessageChannelName) {
  static constexpr char kManifest[] =
      R"({
           "name": "Ext",
           "manifest_version": 3,
           "version": "0.1"
         })";
  static constexpr char kConnectorJs[] =
      R"(chrome.test.runTests([
           async function portWithSendMessageName() {
             let port = chrome.runtime.connect(
                 {name: 'chrome.runtime.sendMessage'});
             chrome.test.assertEq('chrome.runtime.sendMessage', port.name);
             port.onMessage.addListener((msg) => {
               chrome.test.assertEq('pong', msg);
               chrome.test.succeed();
             });
             port.postMessage('ping');
           }
         ]);)";
  static constexpr char kConnecteeJs[] =
      R"(chrome.runtime.onConnect.addListener((port) => {
           self.port = port;
           port.onMessage.addListener((msg) => {
             chrome.test.assertEq(port.name, 'chrome.runtime.sendMessage');
             chrome.test.assertEq(msg, 'ping');
             port.postMessage('pong');
           });
         });
         chrome.runtime.onMessage.addListener((msg) => {
           // We don't expect anything to hit the `onMessage` listener.
           // See https://crbug.com/1430999.
           chrome.test.fail(`Unexpected onMessage received: ${msg}`);
         });)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("connector.html"),
                     R"(<html><script src="connector.js"></script></html>)");
  test_dir.WriteFile(FILE_PATH_LITERAL("connector.js"), kConnectorJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("connectee.html"),
                     R"(<html><script src="connectee.js"></script></html>)");
  test_dir.WriteFile(FILE_PATH_LITERAL("connectee.js"), kConnecteeJs);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ResultCatcher result_catcher;

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension->GetResourceURL("connectee.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension->GetResourceURL("connector.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

class ServiceWorkerMessagingApiTest : public MessagingApiTest {
 protected:
  ~ServiceWorkerMessagingApiTest() override = default;

  size_t GetWorkerRefCount(const blink::StorageKey& key) {
    content::ServiceWorkerContext* sw_context =
        browser()
            ->profile()
            ->GetDefaultStoragePartition()
            ->GetServiceWorkerContext();
    return sw_context->CountExternalRequestsForTest(key);
  }
};

// After sending message from extension and got response back, there should be
// no in-flight request hanging.
// TODO(crbug.com/40257364): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingApiTest,
                       DISABLED_InflightCountAfterSendMessage) {
  constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": {
              "service_worker": "script.js",
              "type": "module"
            }
         })";
  constexpr char kScript[] =
      R"(
          import { openTab } from '/_test_resources/test_util/tabs_util.js';

          self.addEventListener('activate', async (event) => {
            await openTab('page.html');
            sendMessage();
          });

          function sendMessage() {
            chrome.runtime.sendMessage({ greeting: 'hello' }, (response) => {
              chrome.test.notifyPass();
              console.log('pass');
            });
          }
        )";
  constexpr char kPageHtml[] =
      R"(
          <title>Page Title</title>
          <html>
          <body>
            <p>Test page</p>
            <script src="page.js"></script>
          </body>
          </html>
        )";
  constexpr char kPageJs[] =
      R"(
          function onMessage(request, sender, sendResponse) {
            sendResponse({ greeting: 'there' });
          }

          chrome.runtime.onMessage.addListener(onMessage);
        )";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"), kScript);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageJs);

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(catcher.GetNextResult());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(extension->origin(),
            web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  // This is a hack to make sure messaging IPCs are finished. Since IPCs
  // are sent synchronously, anything started prior to this method will finish
  // before this method returns (as content::ExecJs() blocks until
  // completion).
  ASSERT_TRUE(content::ExecJs(web_contents, "1 == 1;"));

  content::RunAllTasksUntilIdle();

  url::Origin extension_origin = url::Origin::Create(extension->url());
  const blink::StorageKey extension_key =
      blink::StorageKey::CreateFirstParty(extension_origin);
  EXPECT_EQ(0u, GetWorkerRefCount(extension_key));
}

class MessagingApiFencedFrameTest : public MessagingApiTest {
 protected:
  MessagingApiFencedFrameTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}},
         {blink::features::kFencedFramesAPIChanges, {}},
         {blink::features::kFencedFramesDefaultMode, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        {/* disabled_features */});
  }
  ~MessagingApiFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MessagingApiFencedFrameTest, Load) {
  ASSERT_TRUE(RunExtensionTest("messaging/connect_fenced_frames", {}))
      << message_;
}

}  // namespace

}  // namespace extensions
