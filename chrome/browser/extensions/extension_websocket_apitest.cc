// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/test_data_directory.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebSocket) {
  ASSERT_TRUE(StartWebSocketServer(net::GetWebSocketTestDataDirectory()));
  ASSERT_TRUE(RunExtensionTest("websocket")) << message_;
}

// Tests that web socket activity resets the service worker idle timer for
// service worker-based extensions.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebSocketsResetServiceWorkerIdleTime) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(StartWebSocketServer(net::GetWebSocketTestDataDirectory()));

  ExtensionTestMessageListener socket_ready_listener("socket ready");
  service_worker_test_utils::TestRegistrationObserver observer(
      browser()->profile());
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("websocket_idle_timer"));
  ASSERT_TRUE(extension);
  observer.WaitForWorkerStart();

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
  content::SetServiceWorkerIdleDelay(
      context, observer.GetServiceWorkerVersionId(), base::Seconds(1));

  // Send messages back and forth to the web socket for two seconds, after
  // which the socket will be closed and the extension will return. If we
  // make it to the two seconds, the test succeeded (because the service worker
  // didn't time out, indicating the web socket extended its lifetime).
  base::Value close_result = BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(), "sendMessagesFor2Seconds()",
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  EXPECT_EQ("closed", close_result);
}

}  // namespace extensions
