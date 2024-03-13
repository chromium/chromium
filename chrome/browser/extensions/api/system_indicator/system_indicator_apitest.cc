// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager.h"
#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

class SystemIndicatorApiTest : public ExtensionApiTest {
 public:
  SystemIndicatorApiTest() = default;

  SystemIndicatorApiTest(const SystemIndicatorApiTest&) = delete;
  SystemIndicatorApiTest& operator=(const SystemIndicatorApiTest&) = delete;

  ~SystemIndicatorApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    // Set shorter delays to prevent test timeouts in tests that need to wait
    // for the event page to unload.
    ProcessManager::SetEventPageIdleTimeForTesting(1);
    ProcessManager::SetEventPageSuspendingTimeForTesting(1);
  }

  const Extension* LoadExtensionAndWait(const std::string& test_name) {
    ExtensionHostTestHelper host_helper(profile());
    host_helper.RestrictToType(mojom::ViewType::kExtensionBackgroundPage);
    base::FilePath extdir = test_data_dir_.AppendASCII(test_name);
    const Extension* extension = LoadExtension(extdir);
    if (extension) {
      // Wait for the background page to cycle.
      host_helper.WaitForDocumentElementAvailable();
      host_helper.WaitForHostDestroyed();
    }
    return extension;
  }
};

IN_PROC_BROWSER_TEST_F(SystemIndicatorApiTest, SystemIndicatorBasic) {
  ASSERT_TRUE(RunExtensionTest("system_indicator/basics")) << message_;
}

IN_PROC_BROWSER_TEST_F(SystemIndicatorApiTest, SystemIndicatorUnloaded) {
  ResultCatcher catcher;

  const Extension* extension =
      LoadExtensionAndWait("system_indicator/unloaded");
  ASSERT_TRUE(extension) << message_;

  // Lazy Background Page has been shut down.
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));

  SystemIndicatorManager* manager =
      SystemIndicatorManagerFactory::GetForContext(profile());
  EXPECT_TRUE(manager->SendClickEventToExtensionForTest(extension->id()));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
