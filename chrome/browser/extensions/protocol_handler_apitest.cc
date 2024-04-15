// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/test/base/launchservices_utils_mac.h"
#endif

namespace extensions {

class ProtocolHandlerApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

#if BUILDFLAG(IS_MAC)
    ASSERT_TRUE(test::RegisterAppWithLaunchServices());
#endif
  }
};

class ProtocolHandlerChangeWaiter
    : public custom_handlers::ProtocolHandlerRegistry::Observer {
 public:
  explicit ProtocolHandlerChangeWaiter(
      custom_handlers::ProtocolHandlerRegistry* registry) {
    registry_observation_.Observe(registry);
  }
  ProtocolHandlerChangeWaiter(const ProtocolHandlerChangeWaiter&) = delete;
  ProtocolHandlerChangeWaiter& operator=(const ProtocolHandlerChangeWaiter&) =
      delete;
  ~ProtocolHandlerChangeWaiter() override = default;
  void Wait() { run_loop_.Run(); }

  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override { run_loop_.Quit(); }

 private:
  base::ScopedObservation<custom_handlers::ProtocolHandlerRegistry,
                          custom_handlers::ProtocolHandlerRegistry::Observer>
      registry_observation_{this};
  base::RunLoop run_loop_;
};

// This test verifies correct registration of protocol handlers using HTML5's
// registerProtocolHandler in extension context and its validation with relaxed
// security checks.
// TODO(crbug.com/40168716): Flaky on win/mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_Registration DISABLED_Registration
#else
#define MAYBE_Registration Registration
#endif
IN_PROC_BROWSER_TEST_F(ProtocolHandlerApiTest, MAYBE_Registration) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Initialize listener and result catcher before the test page is loaded to
  // be sure not to miss any message.
  ExtensionTestMessageListener listener;
  ResultCatcher result_catcher;

  // Load the extension test page.
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("protocol_handler");
  const Extension* extension = LoadExtension(extension_path);
  ASSERT_TRUE(extension);
  GURL url = extension->GetResourceURL("test_registration.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Bypass permission dialogs for registering new protocol handlers.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser()->profile());

  // This synchronizes communication with the JavaScript test. To handle each
  // registerProtocolHandlerWithUserGesture() promise on the JavaScript side,
  // the following actions happen:
  // 1. The C++ side waits for a "request_register_protocol" message.
  // 2. The JS side waits for an "observing_change" message and sends a
  //    "request_register_protocol" message.
  // 3. The C++ side waits for a protocol handler change and sends an
  //    "observing_change" message.
  // 4. The JS side waits for a "change_observed" message and performs a call to
  //    navigator.registerProtocolHandler that is expected to trigger a protocol
  //    handler change. Note that this is performed with a user gesture since
  //    this event is triggered by a content::ExecJs call.
  // 5. The C++ side sends a "change_observed" message and waits for the next
  //    message to the listener.
  // 6. The JS side resolves the promise and moves to the next checks.
  {
    bool wait_for_requests = true;
    while (wait_for_requests) {
      ASSERT_TRUE(listener.WaitUntilSatisfied());
      EXPECT_TRUE(listener.message() == "request_register_protocol" ||
                  listener.message() == "request_complete");
      if (listener.message() == "request_register_protocol") {
        listener.Reset();
        ProtocolHandlerChangeWaiter waiter(registry);
        ASSERT_TRUE(content::ExecJs(web_contents,
                                    "self.postMessage('observing_change');"));
        waiter.Wait();
        ASSERT_TRUE(content::ExecJs(web_contents,
                                    "self.postMessage('change_observed');"));
      } else {
        wait_for_requests = false;
      }
    }
  }

  // This synchronizes final communication with the JavaScript test:
  // 1. The JS side waits for a "complete" message and sends a
  //    "request_complete" message.
  // 2. The C++ side exits the loop above, sends the "complete" message and
  //    waits for a final result.
  // 3. The JS side completes the finalizeTests() and sends the final
  //    notification for chrome.test.runTests.
  // 4. The C++ side catches the final result of the test.
  ASSERT_TRUE(content::ExecJs(web_contents, "self.postMessage('complete');"));

  // Wait for the result of chrome.test.runTests
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// This test verifies the security level applied by the browser process for
// registration of protocol handlers. It ensures that only extension contexts
// have special privilege.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerApiTest, BrowserProcessSecurityLevel) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Run the extension subtest and wait for the initialization.
  ASSERT_TRUE(RunExtensionTest(
      "protocol_handler",
      {.extension_url = "test_browser_process_security_level.html"}))
      << message_;

  content::WebContentsDelegate* web_contents_delegate =
      browser()->tab_strip_model()->GetActiveWebContents()->GetDelegate();
  content::RenderFrameHost* main_frame = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  std::vector<content::RenderFrameHost*> subframes =
      CollectAllRenderFrameHosts(main_frame);
  ASSERT_EQ(3u, subframes.size());

  // Main frame has extension privilege.
  ASSERT_EQ(main_frame, subframes[0]);
  EXPECT_EQ(
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures,
      web_contents_delegate->GetProtocolHandlerSecurityLevel(subframes[0]));

  // First subframe is in strict mode.
  ASSERT_EQ("localhost", subframes[1]->GetFrameName());
  EXPECT_EQ(
      blink::ProtocolHandlerSecurityLevel::kStrict,
      web_contents_delegate->GetProtocolHandlerSecurityLevel(subframes[1]));

  // Nested subframe has extension privilege.
  ASSERT_EQ("chrome_extension", subframes[2]->GetFrameName());
  EXPECT_EQ(
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures,
      web_contents_delegate->GetProtocolHandlerSecurityLevel(subframes[2]));
}

}  // namespace extensions
