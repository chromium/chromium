// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager.h"
#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager_factory.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/lazy_background_page_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "components/version_info/channel.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_test.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

class SystemIndicatorApiTest : public ExtensionApiTest {
 public:
  SystemIndicatorApiTest() : scoped_channel_(version_info::Channel::DEV) {}
  ~SystemIndicatorApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    // Set shorter delays to prevent test timeouts in tests that need to wait
    // for the event page to unload.
    ProcessManager::SetEventPageIdleTimeForTesting(1);
    ProcessManager::SetEventPageSuspendingTimeForTesting(1);
  }

  const Extension* LoadExtensionAndWait(const std::string& test_name) {
    LazyBackgroundObserver page_complete;
    base::FilePath extdir = test_data_dir_.AppendASCII(test_name);
    const Extension* extension = LoadExtension(extdir);
    if (extension)
      page_complete.Wait();
    return extension;
  }

 private:
  ScopedCurrentChannel scoped_channel_;

  DISALLOW_COPY_AND_ASSIGN(SystemIndicatorApiTest);
};

// https://crbug.com/960363: Test crashes on ChromeOS.
#if defined(OS_CHROMEOS)
#define MAYBE_SystemIndicatorBasic DISABLED_SystemIndicatorBasic
#else
#define MAYBE_SystemIndicatorBasic SystemIndicatorBasic
#endif
IN_PROC_BROWSER_TEST_F(SystemIndicatorApiTest, MAYBE_SystemIndicatorBasic) {
  ASSERT_TRUE(RunExtensionTest("system_indicator/basics")) << message_;
}

// Failing on 10.6, flaky elsewhere http://crbug.com/497643
IN_PROC_BROWSER_TEST_F(SystemIndicatorApiTest,
                       DISABLED_SystemIndicatorUnloaded) {
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
