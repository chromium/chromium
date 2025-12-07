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
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/embedder_support/switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/runtime.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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
  explicit MessagingApiTest(bool enable_back_forward_cache = true) {
    if (!enable_back_forward_cache) {
      feature_list_.InitWithFeaturesAndParameters(
          {}, {features::kBackForwardCache});
      return;
    }

    feature_list_.InitWithFeaturesAndParameters(
        content::GetBasicBackForwardCacheFeatureForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
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

class MessagingApiWithBackForwardCacheTest : public MessagingApiTest {
 public:
  MessagingApiWithBackForwardCacheTest()
      : MessagingApiTest(
            /*enable_back_forward_cache=*/true) {}
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

IN_PROC_BROWSER_TEST_F(MessagingApiWithoutBackForwardCacheTest, Messaging) {
  ASSERT_TRUE(RunExtensionTest("messaging/connect")) << message_;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)

IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingCrash) {
  ExtensionTestMessageListener ready_to_crash("ready_to_crash");
  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("messaging/connect_crash")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html")));
  content::WebContents* tab = GetActiveWebContents();
  EXPECT_TRUE(ready_to_crash.WaitUntilSatisfied());

  ResultCatcher catcher;
  CrashTab(tab);
  EXPECT_TRUE(catcher.GetNextResult());
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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

// TODO(kalman): Most web messaging tests disabled on windows due to extreme
// flakiness. See http://crbug.com/350517.
#if !BUILDFLAG(IS_WIN)

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

#endif  // !BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_EXTENSIONS)

// Tests that messages sent in the pagehide handler of a window arrive.
IN_PROC_BROWSER_TEST_F(MessagingApiTest, MessagingOnPagehide) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("messaging/on_pagehide"));
  ExtensionTestMessageListener listener("listening");
  ASSERT_TRUE(extension);
  // Open a new tab to example.com. Since we'll be closing it later, we need
  // to make sure there's still a tab around to extend the life of the
  // browser.
  NavigateToURLInNewTab(
      embedded_test_server()->GetURL("example.com", "/empty.html"));
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
      GetActiveWebContents());
  chrome::CloseTab(browser());
  destroyed_watcher.Wait();
  base::RunLoop().RunUntilIdle();
  // The extension should have sent a message from its pagehide handler.
  EXPECT_EQ(1, content::EvalJs(background_contents, "window.messageCount;"));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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

  NavigateToURLInNewTab(extension->GetResourceURL("connectee.html"));
  NavigateToURLInNewTab(extension->GetResourceURL("connector.html"));

  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

class MessagingApiTestWithPageUrlLoad
    : public MessagingApiTest,
      public testing::WithParamInterface<bool> {
 public:
  MessagingApiTestWithPageUrlLoad() = default;

  void SetUpOnMainThread() override {
    MessagingApiTest::SetUpOnMainThread();
    url_ = embedded_test_server()->GetURL("/extensions/test_file.html");
  }

 protected:
  // Runs the extension test located at `extension_name` but first loads a tab
  // to //chrome/test/data/extensions/test_file.html.
  testing::AssertionResult RunMessagingTest(const char* extension_name) {
    return RunExtensionTest(extension_name, {.page_url = url_.spec().c_str(),
                                             .use_extensions_root_dir = true})
               ? testing::AssertionSuccess()
               : testing::AssertionFailure();
  }

 private:
  GURL url_;
};

class MessagingSerializationApiTest : public MessagingApiTestWithPageUrlLoad {
 public:
  MessagingSerializationApiTest() {
    // This feature treats some messaging response failures differently so let's
    // force it on to have consistent response behavior.
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that various objects can be JSON serialized to/from v8 for one-time and
// long-lived messaging APIs. It tests both the `runtime` and `tabs` APIs by
// sending messages from a content script to the extension background and then
// vice versa.
IN_PROC_BROWSER_TEST_F(MessagingSerializationApiTest, JSONSerialization) {
  // Waiters that confirm the background test can run.
  // `content_script_ready_for_background_tests` confirms the message listeners
  // are ready to receive messages from the background test.
  // `worker_background_ready_to_run_tests` is used to pause the background
  // tests from running until we can provide the tab's (content script's) ID to
  // the backgrounds tests so that they have a tab target to send messages to.
  ExtensionTestMessageListener content_script_ready_for_background_tests(
      "content-message-handlers-registered");
  ExtensionTestMessageListener worker_background_waiting_to_run_tests(
      "background-script-evaluated", ReplyBehavior::kWillReply);

  // This first runs the `runtime` API tests sending messages from a content
  // script to the extension's background.
  EXPECT_TRUE(RunMessagingTest("messaging/serialization")) << message_;

  // After the above tests have finished the below runs the `tab` API tests
  // sending messages from the extension's background to the content script in a
  // tab (opened during `RunMessagingTest()`).
  ASSERT_TRUE(content_script_ready_for_background_tests.WaitUntilSatisfied());
  ASSERT_TRUE(worker_background_waiting_to_run_tests.WaitUntilSatisfied());
  content::WebContents* tab = GetActiveWebContents();
  ASSERT_TRUE(tab);
  int tab_id = ExtensionTabUtil::GetTabId(tab);
  SetCustomArg(base::NumberToString(tab_id));
  ResultCatcher result_catcher;
  worker_background_waiting_to_run_tests.Reply("start background tests");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

class OnMessagePromiseReturnMessagingApiTest
    : public MessagingApiTestWithPageUrlLoad {
 public:
  OnMessagePromiseReturnMessagingApiTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Runs multiple test scenarios for runtime.OnMessage() listeners returning
// promises.
IN_PROC_BROWSER_TEST_F(OnMessagePromiseReturnMessagingApiTest,
                       OnMessagePromiseReturnResolvesBehavior) {
  ASSERT_TRUE(RunMessagingTest("messaging/on_message_promise_resolve"))
      << message_;
}

// Tests that when multiple listeners return promises, the sender receives a
// response from the first promise to resolve if the faster promise is
// registered first.
IN_PROC_BROWSER_TEST_F(
    OnMessagePromiseReturnMessagingApiTest,
    OnMessageMultiPromiseReturnResolvesBehavior_FasterPromiseRegisteredFirst) {
  ASSERT_TRUE(
      RunMessagingTest("messaging/on_message_multi_promise_faster_first"))
      << message_;
}

// Tests that when multiple listeners return promises, the sender receives a
// response from the first promise to resolve if the faster promise is
// registered second.
IN_PROC_BROWSER_TEST_F(
    OnMessagePromiseReturnMessagingApiTest,
    OnMessageMultiPromiseReturnResolvesBehavior_SlowerPromiseRegisteredFirst) {
  ASSERT_TRUE(
      RunMessagingTest("messaging/on_message_multi_promise_slower_first"))
      << message_;
}

// Tests that when the first listener returns true and the second returns a
// promise, the faster sendResponse response is used to send the response.
IN_PROC_BROWSER_TEST_F(
    OnMessagePromiseReturnMessagingApiTest,
    OnMessageMultiPromiseReturnResolvesBehavior_ReturnTrueThenPromise) {
  ASSERT_TRUE(RunMessagingTest("messaging/on_message_return_true_then_promise"))
      << message_;
}

// Tests that when the first listener returns true and the second returns a
// promise, the faster promise response is used to send the response.
IN_PROC_BROWSER_TEST_F(
    OnMessagePromiseReturnMessagingApiTest,
    OnMessageMultiPromiseReturnResolvesBehavior_ReturnTrueThenPromiseFaster) {
  ASSERT_TRUE(
      RunMessagingTest("messaging/on_message_return_true_then_promise_faster"))
      << message_;
}

// Tests that when the first listener returns a promise and the second returns
// true, the faster promise response is used to send the response.
IN_PROC_BROWSER_TEST_F(
    OnMessagePromiseReturnMessagingApiTest,
    OnMessageMultiPromiseReturnResolvesBehavior_ReturnPromiseThenTrue) {
  ASSERT_TRUE(RunMessagingTest("messaging/on_message_return_promise_then_true"))
      << message_;
}

// Tests that when the first listener returns a promise and the second returns
// true, the faster sendResponse response is used to send the response.
IN_PROC_BROWSER_TEST_F(
    OnMessagePromiseReturnMessagingApiTest,
    OnMessageMultiPromiseReturnResolvesBehavior_ReturnPromiseThenTrueFaster) {
  const GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(RunExtensionTest(
      "messaging/on_message_return_promise_then_true_faster",
      {.page_url = url.spec().c_str(), .use_extensions_root_dir = true}))
      << message_;
}

// Tests that when there are multiple listener functions that are registered as
// `async functions` the faster function (promise) to resolve is used as the
// response to the sender.
IN_PROC_BROWSER_TEST_F(
    OnMessagePromiseReturnMessagingApiTest,
    OnMessageMultiPromiseReturnResolvesBehavior_MultipleAsyncFunctionsRace) {
  ExtensionTestMessageListener faster_async_function_called(
      "faster async function called");
  ExtensionTestMessageListener slower_async_function_called(
      "slower async function called");
  const GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(RunExtensionTest(
      "messaging/on_message_return_promise_as_multiple_async_functions",
      {.page_url = url.spec().c_str(), .use_extensions_root_dir = true}))
      << message_;

  // Confirm that all async functions are called and can respond to the message.
  {
    SCOPED_TRACE("waiting to confirm that both async functions were called");
    EXPECT_TRUE(faster_async_function_called.WaitUntilSatisfied());
    EXPECT_TRUE(slower_async_function_called.WaitUntilSatisfied());
  }
}

// Tests that when there are multiple listener functions that are registered as
// `async functions` the faster function (promise) to resolve (even if it's not
// the first registered function) is used as the response to the sender.
IN_PROC_BROWSER_TEST_F(
    OnMessagePromiseReturnMessagingApiTest,
    OnMessageMultiPromiseReturnResolvesBehavior_LaterRegisteredAsyncFunctionsCanRespond) {
  ExtensionTestMessageListener faster_async_function_called(
      "faster async function called");
  ExtensionTestMessageListener slower_async_function_called(
      "slower async function called");
  const GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(RunExtensionTest(
      "messaging/on_message_return_promise_as_later_registered_async_function",
      {.page_url = url.spec().c_str(), .use_extensions_root_dir = true}))
      << message_;

  // Confirm that all async functions are called and can respond to the message.
  {
    SCOPED_TRACE("waiting to confirm that both async functions were called");
    EXPECT_TRUE(faster_async_function_called.WaitUntilSatisfied());
    EXPECT_TRUE(slower_async_function_called.WaitUntilSatisfied());
  }
}

IN_PROC_BROWSER_TEST_F(OnMessagePromiseReturnMessagingApiTest,
                       OnMessagePromiseReturnRejectsBehavior) {
  ASSERT_TRUE(RunMessagingTest("messaging/on_message_promise_reject"))
      << message_;
}

// TODO(crbug.com/439644930): PolyfillSupportMessagingApiTest and its test case
// becomes unnecessary when the feature becomes the default (there are plenty of
// other tests that test synchronous responses).
// Helps test messaging behavior when
// `extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport` is
// enabled or disabled.
class PolyfillSupportMessagingApiTest : public base::test::WithFeatureOverride,
                                        public MessagingApiTestWithPageUrlLoad {
 public:
 public:
  PolyfillSupportMessagingApiTest()
      : base::test::WithFeatureOverride(
            extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport) {
  }
};

// The PolyfillSupport* tests are testing various runtime.sendMessage()
// behaviors compared to mozilla/webextension-polyfill
// (https://github.com/mozilla/webextension-polyfill) when
// `extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport` is
// enabled or disabled. The polyfill doesn't support callbacks so we do not test
// the sendMessage() callback version
// (https://github.com/mozilla/webextension-polyfill/issues/102).
IN_PROC_BROWSER_TEST_P(PolyfillSupportMessagingApiTest,
                       SendMessageListenerBehavior) {
  ASSERT_TRUE(RunMessagingTest("messaging/send_message_polyfill_sync"))
      << message_;
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PolyfillSupportMessagingApiTest);

#if BUILDFLAG(ENABLE_EXTENSIONS)

// TODO(crbug.com/439644930):PolyfillSupportWithWorkerShutdownMessagingApiTest
// and its test case becomes unnecessary when the feature becomes the default
// (the polyfill feature makes it so the errors tested here are handled without
// worker shutdown).
// Helps with testing messaging scenarios where the worker background must be
// stopped in order to elicit a response to message. The polyfill feature
// handles these scenarios without worker shutdown so we keep it disabled.
class PolyfillSupportWithWorkerShutdownMessagingApiTest
    : public MessagingApiTestWithPageUrlLoad {
 public:
  // Wait until the message listener finishes running and then stop the worker.
  // Then inform the message sender that the worker shutdown.
  void OnShutdownMessage(const ExtensionId& extension_id,
                         const std::string& message) {
    // Wait for the worker listener to process the message so we don't shutdown
    // the worker so quickly that the sender's message never gets to the
    // listener.
    ASSERT_TRUE(message_processed_listener_.WaitUntilSatisfied(
        base::RunLoop::Type::kNestableTasksAllowed));
    message_processed_listener_.Reset();
    // Shut down the worker to start garbage collection of the sendResponse in
    // the listener context. This elicits the browser to respond on behalf of
    // the listener.
    browsertest_util::StopServiceWorkerForExtensionGlobalScope(
        profile(), extension_id, base::RunLoop::Type::kNestableTasksAllowed);
    // Notify the test cases to proceed.
    worker_shutdown_listener_.Reply("");
    worker_shutdown_listener_.Reset();
  }

 protected:
  PolyfillSupportWithWorkerShutdownMessagingApiTest() {
    scoped_feature_list_.InitAndDisableFeature(
        extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport);
  }

  void SetUpOnMainThread() override {
    MessagingApiTestWithPageUrlLoad::SetUpOnMainThread();
    worker_shutdown_listener_.SetOnRepeatedlySatisfied(base::BindRepeating(
        &PolyfillSupportWithWorkerShutdownMessagingApiTest::OnShutdownMessage,
        base::Unretained(this), extension_id_));
  }

 private:
  // Waits for the message listener to finish processing the message it
  // received.
  ExtensionTestMessageListener message_processed_listener_ =
      ExtensionTestMessageListener("listener_processed_message");
  // Waits for message sender to indicate that it would like for the worker to
  // shutdown so it can receive a reply.
  ExtensionTestMessageListener worker_shutdown_listener_ =
      ExtensionTestMessageListener("shutdown_worker",
                                   ReplyBehavior::kWillReply);
  ExtensionId extension_id_ = ExtensionId("iegclhlplifhodhkoafiokenjoapiobj");
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test how does messaging handle when the listener never responds but also
// never releases its reference to the v8 reply function (when the polyfill
// feature is disabled). Also see PolyfillSupportMessagingApiTest.
IN_PROC_BROWSER_TEST_F(PolyfillSupportWithWorkerShutdownMessagingApiTest,
                       SendMessageListenerBehavior_Asynchronous) {
  ASSERT_TRUE(RunMessagingTest("messaging/send_message_polyfill_async"))
      << message_;
}

// Test class that sets `chrome.test.getConfig()`'s 'customArg' key to the
// `extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport` state so
// the extension test can adjust its expectations at test runtime.
class PolyfillSupportMessagingErrorsApiTest
    : public PolyfillSupportMessagingApiTest {
 public:

  void SetUpOnMainThread() override {
    PolyfillSupportMessagingApiTest::SetUpOnMainThread();
    // Set "customArg" to be whether the feature is enabled in
    // chrome.test.getConfig().
    js_test_config_.Set(
        "customArg", base::Value(IsParamFeatureEnabled() ? "true" : "false"));
    extensions::TestGetConfigFunction::set_test_config_state(&js_test_config_);
  }

  void TearDownOnMainThread() override {
    PolyfillSupportMessagingApiTest::TearDownOnMainThread();
    extensions::TestGetConfigFunction::set_test_config_state(nullptr);
  }

 private:
  base::Value::Dict js_test_config_;
};

// Test the sender's promise behavior when there are two listeners and:
// 1) the first registered throws a synchronous error
// 2) the second registered responds to the message
IN_PROC_BROWSER_TEST_P(PolyfillSupportMessagingErrorsApiTest,
                       ListenerErrorHandlingWhenErrorIsFirst) {
  ASSERT_TRUE(
      RunMessagingTest("messaging/one_time_message_handler_error_first"))
      << message_;
}

// Test the sender's promise behavior when there are two listeners and:
// 1) the first registered responds to the message
// 2) the second registered throws a synchronous error
IN_PROC_BROWSER_TEST_P(PolyfillSupportMessagingErrorsApiTest,
                       ListenerErrorHandlingWhenResponseIsFirst) {
  ASSERT_TRUE(RunMessagingTest(
      "messaging/one_time_message_handler_send_response_first"))
      << message_;
}

// Test the sender's promise behavior when there is one listener that replies
// and then throws an error immediately afterward.
IN_PROC_BROWSER_TEST_P(PolyfillSupportMessagingErrorsApiTest,
                       ListenerErrorHandlingWhenOneListenerResponseIsFirst) {
  ASSERT_TRUE(RunMessagingTest(
      "messaging/one_time_message_handler_send_response_first_same_listener"))
      << message_;
}

// Test the sender's promise behavior when there is one listener that throws an
// error immediately.
IN_PROC_BROWSER_TEST_P(PolyfillSupportMessagingErrorsApiTest,
                       ListenerErrorHandlingWhenOneListenerErrorFirst) {
  const GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(RunMessagingTest(
      "messaging/one_time_message_handler_error_first_same_listener"))
      << message_;
}

// Test the sender's promise behavior when there are two listeners and:
// 1) the first registered responds asynchronously (with `return true`)
// 2) the second registered throws a synchronous error
IN_PROC_BROWSER_TEST_P(PolyfillSupportMessagingErrorsApiTest,
                       ListenerErrorHandlingWhenAsyncResponseIsFirst) {
  const GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(RunMessagingTest(
      "messaging/one_time_message_handler_send_async_response_first"))
      << message_;
}

// Test the sender's promise behavior when there are two listeners and:
// 1) the first registered throws an error synchronously
// 2) the second registered also throws an error synchronously
IN_PROC_BROWSER_TEST_P(PolyfillSupportMessagingErrorsApiTest,
                       ListenerErrorHandlingWhenMultipleSyncErrorsThrown) {
  ASSERT_TRUE(
      RunMessagingTest("messaging/one_time_message_handler_sync_errors"))
      << message_;
}

// Test the sender's promise behavior when there is a single listener that
// throws a variety of error types.
IN_PROC_BROWSER_TEST_P(PolyfillSupportMessagingErrorsApiTest,
                       ListenerErrorHandlingForManySyncErrorTypesThrown) {
  const GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(RunMessagingTest(
      "messaging/one_time_message_handler_many_error_types_same_listener"))
      << message_;
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PolyfillSupportMessagingErrorsApiTest);

class PolyfillFeatureEnabledMessagingApiTest
    : public MessagingApiTestWithPageUrlLoad {
 public:
  PolyfillFeatureEnabledMessagingApiTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test the sender's promise behavior when there are two listeners and:
// 1) the first registered responds by returning a promise that resolves
// 2) the second registered throws a synchronous error
IN_PROC_BROWSER_TEST_F(PolyfillFeatureEnabledMessagingApiTest,
                       ListenerErrorHandlingWhenPromiseResolveIsFirst) {
  ASSERT_TRUE(RunMessagingTest(
      "messaging/one_time_message_handler_promise_resolve_first"))
      << message_;
}

// Test the sender's promise behavior when there are two listeners and:
// 1) the first registered responds by returning a promise that rejects
// 2) the second registered throws a synchronous error
IN_PROC_BROWSER_TEST_F(PolyfillFeatureEnabledMessagingApiTest,
                       ListenerErrorHandlingWhenPromiseRejectIsFirst) {
  ASSERT_TRUE(RunMessagingTest(
      "messaging/one_time_message_handler_promise_reject_first"))
      << message_;
}

using PolyfillUnserializableMessageResponseShutdownTest =
    PolyfillSupportWithWorkerShutdownMessagingApiTest;

// Tests messaging behavior when a listener sends a response that is not JSON
// serializable and the polyfill feature is not enabled.
IN_PROC_BROWSER_TEST_F(PolyfillUnserializableMessageResponseShutdownTest,
                       UnserializableResponse) {
  ASSERT_TRUE(
      RunMessagingTest("messaging/send_message_non_polyfill_unserializable"))
      << message_;
}

// The tests for when the feature is disabled are in
// PolyfillUnserializableMessageResponseShutdownTest.UnserializableResponse
// since they require extra logic to test.
using PolyfillUnserializableMessageResponseTest =
    PolyfillFeatureEnabledMessagingApiTest;

// Tests similar behavior to PolyfillSupportMessagingApiTest, but specifically
// when the message listener attempts to send unserializable data back to the
// sender. In this case we close the channel and return an error. It is closer
// to the behavior of mozilla/webextension-polyfill
// (https://github.com/mozilla/webextension-polyfill), but different in that an
// error is returned.
IN_PROC_BROWSER_TEST_F(PolyfillUnserializableMessageResponseTest,
                       UnserializableResponseClosesChannel) {
  ASSERT_TRUE(
      RunMessagingTest("messaging/send_message_polyfill_unserializable"))
      << message_;
}

// Helps in testing that
// extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport doesn't
// regress asynchronous listener behavior when multiple listeners can return for
// a single message.
class OnMessageMultiListenerMessagingApiTest
    : public MessagingApiTestWithPageUrlLoad,
      public base::test::WithFeatureOverride {
 public:
  OnMessageMultiListenerMessagingApiTest()
      : base::test::WithFeatureOverride(
            extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport) {
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that, when a synchronous onMessage listener is registered first (it's
// return value is examined first) and an asynchronous listener is registered
// second, it doesn't prevent the asynchronous listeners sendResponse() call
// from getting to the message sender. Regression test for crbug.com/424560420.
IN_PROC_BROWSER_TEST_P(OnMessageMultiListenerMessagingApiTest,
                       OnMessageSyncListenerReturnsFirst) {
  ASSERT_TRUE(RunMessagingTest(
      "messaging/on_message_multi_listener/sync_listener_called_first"))
      << message_;
}

// Tests that, when a asynchronous onMessage listener is registered first (it's
// return value is examined first) and a synchronous listener is registered
// second, it doesn't prevent the asynchronous listeners sendResponse() call
// from getting to the message sender. Regression test for crbug.com/424560420.
IN_PROC_BROWSER_TEST_P(OnMessageMultiListenerMessagingApiTest,
                       OnMessageAsyncListenerReturnsFirst) {
  ASSERT_TRUE(RunMessagingTest(
      "messaging/on_message_multi_listener/async_listener_called_first"))
      << message_;
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(OnMessageMultiListenerMessagingApiTest);

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class ServiceWorkerMessagingApiTest : public MessagingApiTest {
 protected:
  ~ServiceWorkerMessagingApiTest() override = default;

  size_t GetWorkerRefCount(const blink::StorageKey& key) {
    content::ServiceWorkerContext* sw_context =
        profile()->GetDefaultStoragePartition()->GetServiceWorkerContext();
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

  content::WebContents* web_contents = GetActiveWebContents();
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
