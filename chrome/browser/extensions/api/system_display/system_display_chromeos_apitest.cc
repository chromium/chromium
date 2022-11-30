// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/system_display/display_info_provider_chromeos.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/test/extension_test_message_listener.h"

using ContextType = extensions::ExtensionBrowserTest::ContextType;

class SystemDisplayChromeOSApiTest
    : public extensions::ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  SystemDisplayChromeOSApiTest() : ExtensionApiTest(GetParam()) {}
  ~SystemDisplayChromeOSApiTest() override = default;
  SystemDisplayChromeOSApiTest(const SystemDisplayChromeOSApiTest&) = delete;
  SystemDisplayChromeOSApiTest& operator=(const SystemDisplayChromeOSApiTest&) =
      delete;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
    extensions::ExtensionApiTest::SetUpDefaultCommandLine(command_line);
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         SystemDisplayChromeOSApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         SystemDisplayChromeOSApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(SystemDisplayChromeOSApiTest,
                       CheckOnDisplayChangedEvent) {
  ExtensionTestMessageListener listener_for_extension_ready("ready");
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("system_display_chromeos")));
  ASSERT_TRUE(listener_for_extension_ready.WaitUntilSatisfied());
  // Give the mojo CrosDisplayConfig.AddObserver() call a chance to go through.
  base::RunLoop().RunUntilIdle();

  {
    // Change tablet physical state then ensure that OnDisplayChangedEvent is
    // triggered.
    ExtensionTestMessageListener listener_for_success("success");
    ash::TabletMode::Get()->SetEnabledForTest(true);
    ASSERT_TRUE(listener_for_success.WaitUntilSatisfied());
  }
}
