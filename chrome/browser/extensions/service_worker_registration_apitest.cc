// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

// Tests related to the registration state of extension background service
// workers.
class ServiceWorkerRegistrationApiTest : public ExtensionApiTest {
 public:
  ServiceWorkerRegistrationApiTest() = default;
  ~ServiceWorkerRegistrationApiTest() override = default;

  // Retrieves the registration state of the service worker for the given
  // `extension` from the //content layer.
  content::ServiceWorkerCapability GetServiceWorkerRegistrationState(
      const Extension& extension) {
    const GURL& root_scope = extension.url();
    const blink::StorageKey storage_key =
        blink::StorageKey::CreateFirstParty(extension.origin());
    base::test::TestFuture<content::ServiceWorkerCapability> future;
    content::ServiceWorkerContext* service_worker_context =
        util::GetStoragePartitionForExtensionId(extension.id(), profile())
            ->GetServiceWorkerContext();
    service_worker_context->CheckHasServiceWorker(root_scope, storage_key,
                                                  future.GetCallback());
    return future.Get();
  }
};

// TODO(devlin): There's overlap with service_worker_apitest.cc in this file,
// and other tests in that file that should go here so that it's less
// monolithic.

// Tests that a service worker registration is properly stored after extension
// installation, both at the content layer and in the cached state in the
// extensions layer.
IN_PROC_BROWSER_TEST_F(ServiceWorkerRegistrationApiTest,
                       ServiceWorkerIsProperlyRegisteredAfterInstallation) {
  static constexpr char kManifest[] =
      R"({
           "name": "Extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kBackground[] = "// Blank";

  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);

  const Extension* extension = LoadExtension(
      extension_dir.UnpackedPath(), {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);

  ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
  ASSERT_TRUE(task_queue);

  base::Version stored_version =
      task_queue->RetrieveRegisteredServiceWorkerVersionForTest(
          extension->id());
  ASSERT_TRUE(stored_version.IsValid());
  EXPECT_EQ("0.1", stored_version.GetString());
  EXPECT_EQ(content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER,
            GetServiceWorkerRegistrationState(*extension));
}

}  // namespace extensions
