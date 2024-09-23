// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

// This tests an extension that starts a shared worker.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SharedWorker) {
  EXPECT_TRUE(RunExtensionTest("shared_worker/basic")) << message_;
}

// This tests an extension that is controlled by a service worker and starts a
// shared worker. The requests for the shared worker scripts and the requests
// initiated by the shared worker should be seen by the service worker.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       SharedWorker_ControlledByServiceWorker) {
  // Load the extension. It will register a service worker.
  ExtensionTestMessageListener listener("READY");
  listener.set_failure_message("FAIL");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("shared_worker/service_worker_controlled"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_TRUE(extension);
  ProcessManager* process_manager = ProcessManager::Get(profile());
  ExtensionHost* background_page =
      process_manager->GetBackgroundHostForExtension(extension->id());

  // Close the background page and start it again, so it is controlled
  // by the service worker.
  ExtensionTestMessageListener listener2("CONTROLLED");
  listener2.set_failure_message("FAIL");
  background_page->Close();
  ExtensionBackgroundPageWaiter(profile(), *extension)
      .WaitForBackgroundClosed();
  background_page = nullptr;
  process_manager->WakeEventPage(extension->id(), base::DoNothing());
  ExtensionBackgroundPageWaiter(profile(), *extension).WaitForBackgroundOpen();
  EXPECT_TRUE(listener2.WaitUntilSatisfied());

  // The background page should conduct the tests.
  ExtensionTestMessageListener listener3("PASS");
  listener3.set_failure_message("FAIL");
  EXPECT_TRUE(listener3.WaitUntilSatisfied());
}

}  // namespace extensions
