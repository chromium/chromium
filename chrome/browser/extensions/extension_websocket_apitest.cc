// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/script_executor.h"
#include "extensions/browser/script_result_queue.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/test_data_directory.h"

namespace extensions {

class ExtensionWebSocketApiTest : public ExtensionApiTest {
 public:
  ExtensionWebSocketApiTest() = default;
  ~ExtensionWebSocketApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    ASSERT_TRUE(StartEmbeddedTestServer());
    ASSERT_TRUE(StartWebSocketServer(net::GetWebSocketTestDataDirectory()));
  }

  // Runs a specific setup for service worker-based extensions. We open a web
  // socket, set the idle timeout for the worker to one second, then wait for
  // two seconds of web socket activity. If the worker is still alive and
  // responds, it indicates the web socket correctly extended the worker's
  // lifetime.
  // `test_directory` indicates the path from which to load the extension,
  // since different extensions test different kinds of web socket activity.
  void RunServiceWorkerWebSocketTest(const char* test_directory);
};

void ExtensionWebSocketApiTest::RunServiceWorkerWebSocketTest(
    const char* test_directory) {
  ExtensionTestMessageListener socket_ready_listener("socket ready");
  service_worker_test_utils::TestServiceWorkerContextObserver observer(
      browser()->profile());
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(test_directory));
  ASSERT_TRUE(extension);
  const int64_t version_id = observer.WaitForWorkerStarted();

  // Open the web socket in the extension.
  base::Value open_result = BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(), "openSocket()",
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  EXPECT_EQ("open", open_result);

  // Tricky: `content::SetServiceWorkerIdleDelay() can only be called when the
  // idle timer is already active; that is, when there are no pending events.
  // Run until idle to let the result from the BackgroundScriptExecutor fully
  // finish, and then set the idle delay to 1s.
  base::RunLoop().RunUntilIdle();

  // Set idle timeout to 1 second.
  content::ServiceWorkerContext* context =
      util::GetServiceWorkerContextForExtensionId(extension->id(),
                                                  browser()->profile());
  content::SetServiceWorkerIdleDelay(context, version_id, base::Seconds(1));

  // Wait for two seconds of web socket activity, after which the socket will
  // be closed and the extension will return. If we make it to the two seconds,
  // the test succeeded (because the service worker didn't time out, indicating
  // the web socket extended its lifetime).
  base::Value close_result = BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(), "perform2SecondsOfWebSocketActivity()",
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  EXPECT_EQ("closed", close_result);
}

IN_PROC_BROWSER_TEST_F(ExtensionWebSocketApiTest, BasicWebSocketUsage) {
  ASSERT_TRUE(RunExtensionTest("websocket")) << message_;
}

// Tests that client-side web socket activity (like sending messages) resets the
// service worker idle timer for service worker-based extensions.
// TODO(devlin): This test uses an echoing web socket, so it has both sending
// and receiving messages. It'd be better if this verified it purely via
// sending messages.
IN_PROC_BROWSER_TEST_F(ExtensionWebSocketApiTest,
                       SendingWebSocketMessagesResetsServiceWorkerIdleTime) {
  RunServiceWorkerWebSocketTest("websocket_idle_timer_send_messages");
}

// Tests that server-initiated web socket activity (incoming messages from the
// server) resets the service worker idle timer for service worker-based
// extensions.
// Regression test for https://cbrug.com/1476142.
IN_PROC_BROWSER_TEST_F(ExtensionWebSocketApiTest,
                       ReceivingWebSocketMessagesResetsServiceWorkerIdleTime) {
  RunServiceWorkerWebSocketTest("websocket_idle_timer_server_pings");
}

}  // namespace extensions
