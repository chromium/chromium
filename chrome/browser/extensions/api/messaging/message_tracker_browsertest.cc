// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/message_tracker.h"

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class MessageTrackerMessagingTest : public ExtensionApiTest {
 public:
  MessageTrackerMessagingTest() = default;

  MessageTrackerMessagingTest(const MessageTrackerMessagingTest&) = delete;
  MessageTrackerMessagingTest& operator=(const MessageTrackerMessagingTest&) =
      delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    message_tracker_ = MessageTracker::Get(profile());
  }

  void TearDownOnMainThread() override {
    message_tracker_ = nullptr;
    ExtensionApiTest::TearDownOnMainThread();
  }

  MessageTracker* message_tracker() { return message_tracker_; }

 private:
  raw_ptr<MessageTracker> message_tracker_;
};

class MessageTrackerMessagingTestWithOptimizeServiceWorkerStart
    : public MessageTrackerMessagingTest,
      public base::test::WithFeatureOverride {
 public:
  MessageTrackerMessagingTestWithOptimizeServiceWorkerStart()
      : WithFeatureOverride(
            extensions_features::kOptimizeServiceWorkerStartRequests) {}
};

// Tests the tracking of messages when sent from a tab to a SW extension
// background context.
IN_PROC_BROWSER_TEST_P(
    MessageTrackerMessagingTestWithOptimizeServiceWorkerStart,
    SendMessageToWorker) {
  const bool wakeup_optimization_enabled = IsParamFeatureEnabled();
  const int kExpectedWakeUps = wakeup_optimization_enabled ? 0 : 1;

  ExtensionTestMessageListener worker_listener("WORKER_RUNNING");
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/send_message_tab_to_worker"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(worker_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener reply_listener("CONTENT_SCRIPT_RECEIVED_REPLY");
  reply_listener.set_failure_message("FAILURE");

  base::HistogramTester histogram_tester;

  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    const GURL url =
        embedded_test_server()->GetURL("/extensions/test_file.html");
    content::WebContents* new_web_contents =
        browsertest_util::AddTab(browser(), url);
    EXPECT_TRUE(new_web_contents);
  }

  EXPECT_TRUE(reply_listener.WaitUntilSatisfied());

  // Overall channel open metrics expectations.
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatusWithActiveWorker",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatusWithActiveWorker."
      "SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelWorkerDispatchStatus."
      "SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelWorkerWakeUpStatus."
      "SendMessageChannel",
      /*expected_count=*/kExpectedWakeUps);
  // Per connect IPC dispatch metrics expectations.
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForWorker",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForWorker."
      "SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForFrame",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForFrame."
      "SendMessageChannel",
      /*expected_count=*/0);

  // Overall channel open metrics expectations.
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      /*sample=*/MessageTracker::OpenChannelMessagePipelineResult::kOpened,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelStatusWithActiveWorker."
      "SendMessageChannel",
      /*sample=*/MessageTracker::OpenChannelMessagePipelineResult::kOpened,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelWorkerDispatchStatus."
      "SendMessageChannel",
      /*sample=*/MessageTracker::OpenChannelMessagePipelineResult::kOpened,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelWorkerWakeUpStatus."
      "SendMessageChannel",
      /*sample=*/
      MessageTracker::OpenChannelMessagePipelineResult::kWorkerStarted,
      /*expected_count=*/kExpectedWakeUps);
  // Per connect IPC dispatch metrics expectations.
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForWorker",
      /*sample=*/
      MessageTracker::OpenChannelMessagePipelineResult::kOpenChannelAcked,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForWorker."
      "SendMessageChannel",
      /*sample=*/
      MessageTracker::OpenChannelMessagePipelineResult::kOpenChannelAcked,
      /*expected_count=*/1);
}

class MessageTrackerMessagingTestNonWorker
    : public ExtensionApiTest,
      public testing::WithParamInterface<const char*> {};

// Tests the tracking of messages when sent from a tab to a extension background
// page context.
IN_PROC_BROWSER_TEST_P(MessageTrackerMessagingTestNonWorker,
                       SendMessageToNonWorker) {
  ExtensionTestMessageListener background_listener("BACKGROUND_RUNNING");
  constexpr char kManifest[] =
      R"(
      {
        "name": "runtime.sendMessage from content script to extension",
        "version": "0.1",
        "manifest_version": 2,
        "description": "non service worker",
        "content_scripts": [{
          "matches": ["*://*/*"],
          "js": ["content.js"]
        }],
        "background": {
          "scripts": ["background.js"],
          "persistent": %s
        }
      }
    )";

  constexpr char kContentScript[] =
      R"(
      chrome.runtime.sendMessage('tab->background', response => {
        if (response != 'tab->background->tab') {
          chrome.test.sendMessage('FAILURE');
          return;
        }
        chrome.test.sendMessage('CONTENT_SCRIPT_RECEIVED_REPLY');
      });
    )";

  constexpr char kBackground[] =
      R"(
      chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
        if (msg != 'tab->background') {
          chrome.test.sendMessage('FAILURE');
          return;
        }
        sendResponse('tab->background->tab');
      });

      chrome.test.sendMessage('BACKGROUND_RUNNING');
    )";
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(
      base::StringPrintf(kManifest, /*persistent=*/GetParam()));
  test_dir.WriteFile(FILE_PATH_LITERAL("content.js"), kContentScript);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(background_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener reply_listener("CONTENT_SCRIPT_RECEIVED_REPLY");
  reply_listener.set_failure_message("FAILURE");

  base::HistogramTester histogram_tester;

  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    const GURL url =
        embedded_test_server()->GetURL("/extensions/test_file.html");
    content::WebContents* new_web_contents =
        browsertest_util::AddTab(browser(), url);
    EXPECT_TRUE(new_web_contents);
  }

  EXPECT_TRUE(reply_listener.WaitUntilSatisfied());

  // Overall channel open metrics expectations.
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatusWithNoWorker."
      "SendMessageChannel",
      /*expected_count=*/1);
  // No worker metrics should emitted.
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatusWithActiveWorker",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatusWithIdleWorker",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatusWithActiveWorker."
      "SendMessageChannel",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelWorkerDispatchStatus."
      "SendMessageChannel",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelWorkerWakeUpStatus."
      "SendMessageChannel",
      /*expected_count=*/0);
  // Per connect IPC dispatch metrics expectations.
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForWorker",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForWorker."
      "SendMessageChannel",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForFrame",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForFrame."
      "SendMessageChannel",
      /*expected_count=*/1);

  // Overall channel open metrics expectations.
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      /*sample=*/MessageTracker::OpenChannelMessagePipelineResult::kOpened,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelStatusWithNoWorker."
      "SendMessageChannel",
      /*sample=*/MessageTracker::OpenChannelMessagePipelineResult::kOpened,
      /*expected_count=*/1);
  // Per connect IPC dispatch metrics expectations.
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForFrame",
      /*sample=*/
      MessageTracker::OpenChannelMessagePipelineResult::kOpenChannelAcked,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForFrame."
      "SendMessageChannel",
      /*sample=*/
      MessageTracker::OpenChannelMessagePipelineResult::kOpenChannelAcked,
      /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(EventPage,
                         MessageTrackerMessagingTestNonWorker,
                         testing::Values("false"));
INSTANTIATE_TEST_SUITE_P(PersistentBackgroundPage,
                         MessageTrackerMessagingTestNonWorker,
                         testing::Values("true"));

// Tests the tracking of messages when sent from a tab content script to a
// extension background page context and an extension tab script.
IN_PROC_BROWSER_TEST_P(
    MessageTrackerMessagingTestWithOptimizeServiceWorkerStart,
    SendMessageToTabAndWorker) {
  const bool wakeup_optimization_enabled = IsParamFeatureEnabled();
  const int kExpectedWakeUps = wakeup_optimization_enabled ? 0 : 1;

  constexpr char kManifest[] =
      R"(
        {
          "name": "runtime.sendMessage from content script to SW extension",
          "version": "0.1",
          "manifest_version": 3,
          "description": "service worker",
          "content_scripts": [{
            "matches": ["*://*/*"],
            "js": ["content_script.js"]
          }],
          "background": {"service_worker": "background.js"}
        }
      )";

  constexpr char kContentScript[] =
      R"(
      chrome.runtime.sendMessage('tab->worker', response => {
        if (response != 'tab->worker->tab') {
          chrome.test.sendMessage('FAILURE');
          return;
        }
        chrome.test.sendMessage('CONTENT_SCRIPT_RECEIVED_REPLY');
      });
    )";

  constexpr char kTestExtTabHtml[] =
      R"(
      <!DOCTYPE html>
      <body>
        <script src="test_tab.js"></script>
      </body>
      </html>
      )";

  constexpr char kTestExtTab[] =
      R"(
      chrome.runtime.onMessage.addListener((details) => {
        chrome.test.sendMessage('TAB_LISTENER_RECEIVED_MESSAGE');
      });
    )";

  constexpr char kBackground[] =
      R"(
      chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
        if (msg != 'tab->worker') {
          chrome.test.sendMessage('FAILURE');
          return;
        }
        sendResponse('tab->worker->tab');
      });

      chrome.runtime.onInstalled.addListener((details) => {
        chrome.test.sendMessage('WORKER_RUNNING');
      });
    )";
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  test_dir.WriteFile(FILE_PATH_LITERAL("test_ext_tab.html"), kTestExtTabHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("test_tab.js"), kTestExtTab);
  ExtensionTestMessageListener background_listener("WORKER_RUNNING");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(background_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener reply_listener("CONTENT_SCRIPT_RECEIVED_REPLY");
  reply_listener.set_failure_message("REPLY_FAILURE");
  ExtensionTestMessageListener ext_tab_listener_fired(
      "TAB_LISTENER_RECEIVED_MESSAGE");
  ext_tab_listener_fired.set_failure_message("TAB_LISTENER_FAILURE");

  base::HistogramTester histogram_tester;

  {
    ASSERT_TRUE(StartEmbeddedTestServer());

    // Load the extension tab (and it's script).
    GURL ext_url = extension->GetResourceURL("test_ext_tab.html");
    content::WebContents* new_ext_tab_web_contents = GetActiveWebContents();
    ASSERT_TRUE(new_ext_tab_web_contents);
    ASSERT_TRUE(NavigateToURL(new_ext_tab_web_contents, GURL(ext_url)));

    // Must load the tab content script second since it's loading sends a
    // message to the extension tab.
    const GURL url =
        embedded_test_server()->GetURL("/extensions/test_file.html");
    content::WebContents* new_web_contents =
        browsertest_util::AddTab(browser(), url);
    ASSERT_TRUE(new_web_contents);
  }

  ASSERT_TRUE(reply_listener.WaitUntilSatisfied());
  ASSERT_TRUE(ext_tab_listener_fired.WaitUntilSatisfied());

  // Overall channel open metrics expectations.
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatusWithActiveWorker",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatusWithActiveWorker."
      "SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelWorkerDispatchStatus."
      "SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelWorkerWakeUpStatus."
      "SendMessageChannel",
      /*expected_count=*/kExpectedWakeUps);
  // Per connect IPC dispatch metrics expectations.
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForWorker",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForWorker."
      "SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForFrame",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelDispatchOnConnectStatus.ForFrame."
      "SendMessageChannel",
      /*expected_count=*/1);

  // Overall channel open metrics expectations.
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      /*sample=*/MessageTracker::OpenChannelMessagePipelineResult::kOpened,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelStatusWithActiveWorker."
      "SendMessageChannel",
      /*sample=*/MessageTracker::OpenChannelMessagePipelineResult::kOpened,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelWorkerDispatchStatus."
      "SendMessageChannel",
      /*sample=*/MessageTracker::OpenChannelMessagePipelineResult::kOpened,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelWorkerWakeUpStatus."
      "SendMessageChannel",
      /*sample=*/
      MessageTracker::OpenChannelMessagePipelineResult::kWorkerStarted,
      /*expected_count=*/kExpectedWakeUps);
  // Per connect IPC dispatch metrics expectations cannot be specified in this
  // test because the channel will be closed by the first port responder to the
  // IPC and that will can change the value emitted for the other port.
}

// TODO(crbug.com/371011217): Once we start tracking message dispatch metrics
// add a test case for a worker that never responds to the message.

// Toggle `extensions_features::OptimizeServiceWorkerStartRequests`.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    MessageTrackerMessagingTestWithOptimizeServiceWorkerStart);

}  // namespace extensions
