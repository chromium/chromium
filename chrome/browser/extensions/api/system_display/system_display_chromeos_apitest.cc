// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/system_display/display_info_provider_chromeos.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

class SystemDisplayChromeOSApiTest : public extensions::ExtensionApiTest {
 public:
  SystemDisplayChromeOSApiTest() = default;
  ~SystemDisplayChromeOSApiTest() override = default;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
    extensions::ExtensionApiTest::SetUpDefaultCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemDisplayChromeOSApiTest);
};

IN_PROC_BROWSER_TEST_F(SystemDisplayChromeOSApiTest,
                       CheckOnDisplayChangedEvent) {
  ExtensionTestMessageListener listener_for_extension_ready("ready", false);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("system_display_chromeos")));
  ASSERT_TRUE(listener_for_extension_ready.WaitUntilSatisfied());

  extensions::DisplayInfoProviderChromeOS* provider =
      static_cast<extensions::DisplayInfoProviderChromeOS*>(
          extensions::DisplayInfoProvider::Get());

  // Change Tablet Mode then ensure that OnDisplayChangedEvent is triggered
  provider->OnTabletModeToggled(true);

  extensions::ResultCatcher catcher;
  EXPECT_TRUE(catcher.GetNextResult());
}
