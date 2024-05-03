// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

namespace {

base::FilePath WriteExtensionWithMessagePortToDir(TestExtensionDir* test_dir) {
  test_dir->WriteManifest(R"(
      {
        "name": "Content script disconnect on worker stop test",
        "description": "Tests worker shutdown behavior for messaging",
        "version": "0.1",
        "manifest_version": 2,
        "background": {"scripts": ["background.js"]},
        "content_scripts": [{
          "matches": ["*://example.com:*/*"],
          "js": ["content_script.js"]
        }]
      })");
  test_dir->WriteFile(FILE_PATH_LITERAL("background.js"), R"(
      chrome.runtime.onConnect.addListener(port => {
        console.log('background: runtime.onConnect');
        port.onMessage.addListener(msg => {
          if (msg == 'foo') {
            chrome.test.notifyPass();
          } else
            chrome.test.notifyFail('FAILED');
        });
      });)");
  test_dir->WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    var port = chrome.runtime.connect({name:"bar"});
    port.postMessage('foo');)");
  return test_dir->UnpackedPath();
}

base::FilePath WriteServiceWorkerExtensionToDir(TestExtensionDir* test_dir) {
  test_dir->WriteManifest(R"(
      {
        "name": "Test Extension",
        "manifest_version": 2,
        "version": "1.0",
        "background": {"service_worker": "service_worker_background.js"}
      })");
  test_dir->WriteFile(FILE_PATH_LITERAL("service_worker_background.js"),
                      R"(chrome.test.sendMessage('worker_running');)");
  return test_dir->UnpackedPath();
}

}  // namespace

class ServiceWorkerMessagingTest : public ExtensionApiTest {
 public:
  ServiceWorkerMessagingTest() = default;

  ServiceWorkerMessagingTest(const ServiceWorkerMessagingTest&) = delete;
  ServiceWorkerMessagingTest& operator=(const ServiceWorkerMessagingTest&) =
      delete;

  ~ServiceWorkerMessagingTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  void StopServiceWorker(const Extension& extension) {
    // TODO(lazyboy): Ideally we'd want to test worker shutdown on idle, do that
    // once //content API allows to override test timeouts for Service Workers.
    browsertest_util::StopServiceWorkerForExtensionGlobalScope(
        browser()->profile(), extension.id());
  }

  extensions::ScopedTestNativeMessagingHost test_host_;
};

class ServiceWorkerMessagingTestWithActivityLog
    : public ServiceWorkerMessagingTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
    ServiceWorkerMessagingTest::SetUpCommandLine(command_line);
  }
};

// Tests one-way message from content script to SW extension using
// chrome.runtime.sendMessage.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, TabToWorkerOneWay) {
  ExtensionTestMessageListener worker_listener("WORKER_RUNNING");
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/send_message_tab_to_worker_one_way"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(worker_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener test_listener("WORKER_RECEIVED_MESSAGE");
  test_listener.set_failure_message("FAILURE");

  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    const GURL url =
        embedded_test_server()->GetURL("/extensions/test_file.html");
    content::WebContents* new_web_contents =
        browsertest_util::AddTab(browser(), url);
    EXPECT_TRUE(new_web_contents);
  }

  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
}

// Tests chrome.runtime.sendMessage from content script to SW extension.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, TabToWorker) {
  ExtensionTestMessageListener worker_listener("WORKER_RUNNING");
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/send_message_tab_to_worker"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(worker_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener reply_listener("CONTENT_SCRIPT_RECEIVED_REPLY");
  reply_listener.set_failure_message("FAILURE");

  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    const GURL url =
        embedded_test_server()->GetURL("/extensions/test_file.html");
    content::WebContents* new_web_contents =
        browsertest_util::AddTab(browser(), url);
    EXPECT_TRUE(new_web_contents);
  }

  EXPECT_TRUE(reply_listener.WaitUntilSatisfied());
}

// Tests that a message port disconnects if the extension SW is forcefully
// stopped.
// Regression test for https://crbug.com/1033783.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest,
                       TabToWorker_StopWorkerDisconnects) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Content script disconnect on worker stop test",
           "description": "Tests worker shutdown behavior for messaging",
           "version": "0.1",
           "manifest_version": 2,
           "background": {"service_worker": "service_worker_background.js"},
           "content_scripts": [{
             "matches": ["*://example.com:*/*"],
             "js": ["content_script.js"]
           }]
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("service_worker_background.js"),
                     R"(chrome.runtime.onConnect.addListener((port) => {
           console.log('background: runtime.onConnect');
           chrome.test.assertNoLastError();
           chrome.test.notifyPass();
         });
         chrome.test.notifyPass();
      )");
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"),
                     R"(var port = chrome.runtime.connect({name:"foo"});
         port.onDisconnect.addListener(() => {
           console.log('content script: port.onDisconnect');
           chrome.test.assertNoLastError();
           chrome.test.notifyPass();
         });
      )");
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(test_dir.UnpackedPath(),
                    // Wait for the registration to be stored so that it's
                    // persistent before the worker is stopped later.
                    {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);

  // Wait for the extension to register runtime.onConnect listener.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  GURL url =
      embedded_test_server()->GetURL("example.com", "/extensions/body1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Wait for the content script to connect to the worker's port.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Stop the service worker, this will disconnect the port.
  StopServiceWorker(*extension);

  // Wait for the port to disconnect in the content script.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Regression test for https://crbug.com/1176400.
// Tests that service worker shutdown closes messaging channel properly.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest,
                       WorkerShutsDownWhileNativeMessagePortIsOpen) {
  // Set up an observer to wait for the registration to be stored before
  // calling StopServiceWorker below.
  service_worker_test_utils::TestServiceWorkerContextObserver observer(
      browser()->profile());
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/native_message_after_worker_stop"));
  ASSERT_TRUE(extension);

  observer.WaitForRegistrationStored();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  size_t num_channels =
      MessageService::Get(profile())->GetChannelCountForTest();
  StopServiceWorker(*extension);
  // After worker shutdown, expect the channel count to reduce by 1.
  EXPECT_EQ(num_channels - 1,
            MessageService::Get(profile())->GetChannelCountForTest());
}

// Tests chrome.tabs.sendMessage from SW extension to content script.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, WorkerToTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionTest("service_worker/messaging/send_message_worker_to_tab"))
      << message_;
}

// Tests that chrome.tabs.sendMessage from SW extension without specifying
// callback doesn't crash.
//
// Regression test for https://crbug.com/1218569.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest,
                       TabsSendMessageWithoutCallback) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest(
      "service_worker/messaging/tabs_send_message_without_callback"))
      << message_;
}

// Tests port creation (chrome.runtime.connect) from content script to an
// extension SW and disconnecting the port.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest,
                       TabToWorker_ConnectAndDisconnect) {
  // Load an extension that will inject content script to |new_web_contents|.
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_to_worker/connect_and_disconnect"));
  ASSERT_TRUE(extension);

  // Load the tab with content script to open a Port to |extension|.
  // Test concludes when extension gets notified about port being disconnected.
  ResultCatcher catcher;
  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    content::WebContents* new_web_contents = browsertest_util::AddTab(
        browser(),
        embedded_test_server()->GetURL("/extensions/test_file.html"));
    EXPECT_TRUE(new_web_contents);
  }
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests port creation (chrome.runtime.connect) from content script to an
// extension and sending message through the port.
// TODO(lazyboy): Refactor common parts with TabToWorker_ConnectAndDisconnect.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest,
                       TabToWorker_ConnectAndPostMessage) {
  // Load an extension that will inject content script to |new_web_contents|.
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_to_worker/post_message"));
  ASSERT_TRUE(extension);

  // Load the tab with content script to send message to |extension| via port.
  // Test concludes when the content script receives a reply.
  ResultCatcher catcher;
  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    content::WebContents* new_web_contents = browsertest_util::AddTab(
        browser(),
        embedded_test_server()->GetURL("/extensions/test_file.html"));
    EXPECT_TRUE(new_web_contents);
  }
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests chrome.runtime.onMessageExternal between two Service Worker based
// extensions.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, ExternalMessageToWorker) {
  const std::string kTargetExtensionId = "pkplfbidichfdicaijlchgnapepdginl";

  // Load the receiver extension first.
  const Extension* target_extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/send_message_external/target"));
  ASSERT_TRUE(target_extension);
  EXPECT_EQ(kTargetExtensionId, target_extension->id());

  // Then run the test from initiator extension.
  ASSERT_TRUE(RunExtensionTest(
      "service_worker/messaging/send_message_external/initiator"))
      << message_;
}

// Tests chrome.runtime.onConnectExternal between two Service Worker based
// extensions.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, ConnectExternalToWorker) {
  const std::string kTargetExtensionId = "pkplfbidichfdicaijlchgnapepdginl";

  // Load the receiver extension first.
  const Extension* target_extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_external/target"));
  ASSERT_TRUE(target_extension);
  EXPECT_EQ(kTargetExtensionId, target_extension->id());

  // Then run the test from initiator extension.
  ASSERT_TRUE(
      RunExtensionTest("service_worker/messaging/connect_external/initiator"))
      << message_;
}

// Tests that an extension's message port isn't affected by an unrelated
// extension's service worker.
//
// This test opens a message port in an extension (message_port_extension) and
// then loads another extension that is service worker based. This test ensures
// that stopping the service worker based extension doesn't DCHECK in
// message_port_extension's message port.
//
// Regression test for https://crbug.com/1075751.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest,
                       UnrelatedPortsArentAffectedByServiceWorker) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Step 1/2: Load an extension that creates an ExtensionMessagePort from a
  // content script and connects to its background script.
  TestExtensionDir message_port_extension_dir;
  ResultCatcher content_script_connected_catcher;
  const Extension* message_port_extension = LoadExtension(
      WriteExtensionWithMessagePortToDir(&message_port_extension_dir));
  ASSERT_TRUE(message_port_extension);

  // Load the content script for |message_port_extension|, and wait for the
  // content script to connect to its background's port.
  GURL url =
      embedded_test_server()->GetURL("example.com", "/extensions/body1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(content_script_connected_catcher.GetNextResult())
      << content_script_connected_catcher.message();

  // Step 2/2: Load an extension with service worker background, verify that
  // stopping the service worker doesn't cause message port in
  // |message_port_extension| to crash.
  ExtensionTestMessageListener worker_running_listener("worker_running");

  TestExtensionDir worker_extension_dir;
  const Extension* service_worker_extension =
      LoadExtension(WriteServiceWorkerExtensionToDir(&worker_extension_dir),
                    {.wait_for_registration_stored = true});
  const ExtensionId worker_extension_id = service_worker_extension->id();
  ASSERT_TRUE(service_worker_extension);

  // Wait for the extension service worker to settle before moving to next step.
  EXPECT_TRUE(worker_running_listener.WaitUntilSatisfied());

  {
    // Stop the worker, and ensure its completion.
    service_worker_test_utils::UnregisterWorkerObserver
        unregister_worker_observer(ProcessManager::Get(profile()),
                                   worker_extension_id);
    StopServiceWorker(*service_worker_extension);
    unregister_worker_observer.WaitForUnregister();
  }
}

// Tests ActiviyLog from SW based extension.
// Regression test for https://crbug.com/1213074, https://crbug.com/1217343.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTestWithActivityLog, ActivityLog) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const Extension* friend_extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_to_worker/connect_and_disconnect"));
  ASSERT_TRUE(friend_extension);
  {
    ResultCatcher catcher;
    content::WebContents* new_web_contents = browsertest_util::AddTab(
        browser(),
        embedded_test_server()->GetURL("/extensions/test_file.html"));
    EXPECT_TRUE(new_web_contents);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  // The test passes when /activiy_log/ extension sees activities from
  // |friend_extension|.
  ASSERT_TRUE(RunExtensionTest("service_worker/messaging/activity_log/"));
}

// Tests port creation (chrome.runtime.connect) from content script to an
// extension SW should not, by itself, create an external request that keeps the
// SW alive.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, LongLivedChannelNoMessage) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  service_worker_test_utils::TestServiceWorkerContextObserver observer(
      browser()->profile());
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_to_worker/connect"));
  ASSERT_TRUE(extension);
  const int64_t version_id = observer.WaitForWorkerStarted();

  // Load the tab with content script to open a Port to |extension|.
  ResultCatcher catcher;
  {
    content::WebContents* new_web_contents = browsertest_util::AddTab(
        browser(),
        embedded_test_server()->GetURL("/extensions/test_file.html"));
    EXPECT_TRUE(new_web_contents);
  }
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // The service worker will be terminated because there is no in-flight
  // external request.
  base::RunLoop().RunUntilIdle();
  content::ServiceWorkerContext* context =
      util::GetServiceWorkerContextForExtensionId(extension->id(),
                                                  browser()->profile());
  EXPECT_FALSE(
      content::TriggerTimeoutAndCheckRunningState(context, version_id));
}

// Tests that one-time messages (chrome.runtime.sendMessage) from content script
// to an extension SW should add in-flight external request.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, OneTimeChannel) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  service_worker_test_utils::TestServiceWorkerContextObserver observer(
      browser()->profile());
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_to_worker/send_message"));
  ASSERT_TRUE(extension);
  const int64_t version_id = observer.WaitForWorkerStarted();

  // Load the tab with content script to open a Port to |extension|.
  ResultCatcher catcher;
  {
    content::WebContents* new_web_contents = browsertest_util::AddTab(
        browser(),
        embedded_test_server()->GetURL("/extensions/test_file.html"));
    EXPECT_TRUE(new_web_contents);
  }
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // The service worker will not be terminated because there is an in-flight
  // external request. The onMessage listener indicates an asynchronous response
  // but never respond. It keeps the port open. As a result the in-flight
  // request is not finished.
  base::RunLoop().RunUntilIdle();
  content::ServiceWorkerContext* context =
      util::GetServiceWorkerContextForExtensionId(extension->id(),
                                                  browser()->profile());
  EXPECT_TRUE(content::TriggerTimeoutAndCheckRunningState(context, version_id));
}

// Tests post messages through a long-lived channel from content script to an
// extension SW should extend SW lifetime.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest,
                       LongLivedChannelPostMessage) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  service_worker_test_utils::TestServiceWorkerContextObserver observer(
      browser()->profile());
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_to_worker/connect_and_post"));
  ASSERT_TRUE(extension);
  const int64_t version_id = observer.WaitForWorkerStarted();

  // Set idle timeout to 1 second.
  content::ServiceWorkerContext* context =
      util::GetServiceWorkerContextForExtensionId(extension->id(),
                                                  browser()->profile());
  content::SetServiceWorkerIdleDelay(context, version_id, base::Seconds(1));

  // Load the tab with content script to open a Port to |extension|.
  ResultCatcher catcher;
  {
    content::WebContents* new_web_contents = browsertest_util::AddTab(
        browser(),
        embedded_test_server()->GetURL("/extensions/test_file.html"));
    EXPECT_TRUE(new_web_contents);
  }
  // Catching the succeed test message means the service worker has been alive
  // for longer than 2 seconds.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // The content script is sending messages over the long-lived channel every
  // 100ms. The service worker is still running at this point verifies that
  // sending messages prolongs the service worker lifetime.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::CheckServiceWorkerIsRunning(context, version_id));
}

}  // namespace extensions
