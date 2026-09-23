// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_launch/browser_launch_data_collector_desktop.h"

#include <string>

#include "base/command_line.h"
#include "base/process/process.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/enterprise/reporting/browser_launch/scoped_initial_command_line.h"
#include "components/prefs/testing_pref_service.h"
#include "components/webui/flags/feature_entry_macros.h"
#include "components/webui/flags/pref_service_flags_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

class BrowserLaunchDataCollectorDesktopTest : public testing::Test {
 public:
  BrowserLaunchDataCollectorDesktopTest() = default;
  ~BrowserLaunchDataCollectorDesktopTest() override = default;

 protected:
  base::CommandLine stubbed_cli_{base::CommandLine::NO_PROGRAM};
};

TEST_F(BrowserLaunchDataCollectorDesktopTest, GetEvent) {
  // Configure the command line in non-alphabetical order to verify sorting.
  stubbed_cli_.AppendSwitchASCII("switch-3", "value-3");
  stubbed_cli_.AppendSwitch("switch-1");
  stubbed_cli_.AppendSwitchASCII("switch-2", "value-2");

  ScopedInitialCommandLine scoped_cli(&stubbed_cli_);

  BrowserLaunchDataCollectorDesktop collector;
  auto&& event = collector.GetEvent();

  // Verify that all 3 switch keys are captured in sorted order.
  EXPECT_THAT(event.command_line_switch_keys(),
              testing::ElementsAre("switch-1", "switch-2", "switch-3"));

  // Verify launch time matches the system process creation time exactly.
  int64_t expected_time_ms =
      base::Process::Current().CreationTime().InMillisecondsSinceUnixEpoch();
  EXPECT_EQ(event.launch_time_millis(), expected_time_ms);
}

TEST_F(BrowserLaunchDataCollectorDesktopTest, GetEventWithFlagSwitches) {
  stubbed_cli_.AppendSwitchASCII("switch-2", "value-2");
  stubbed_cli_.AppendSwitch("switch-1");
  ScopedInitialCommandLine scoped_cli(&stubbed_cli_);

  // Register flags for this specific test case.
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries(
      {{"flag-duplicate", "Duplicate Switch Flag", "description",
        flags_ui::FlagsState::GetCurrentPlatform(),
        SINGLE_VALUE_TYPE_AND_VALUE("switch-2", "other-val")},
       {"flag-2", "Flag 2", "description",
        flags_ui::FlagsState::GetCurrentPlatform(),
        SINGLE_VALUE_TYPE_AND_VALUE("flag-switch-2", "flag-value-2")},
       {"flag-1", "Flag 1", "description",
        flags_ui::FlagsState::GetCurrentPlatform(),
        SINGLE_VALUE_TYPE("flag-switch-1")}});

  TestingPrefServiceSimple prefs;
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(prefs.registry());
  flags_ui::PrefServiceFlagsStorage flags_storage(&prefs);

  // Enable flags.
  about_flags::SetFeatureEntryEnabled(&flags_storage, "flag-duplicate",
                                      /*enable=*/true);
  about_flags::SetFeatureEntryEnabled(&flags_storage, "flag-2",
                                      /*enable=*/true);
  about_flags::SetFeatureEntryEnabled(&flags_storage, "flag-1",
                                      /*enable=*/true);

  base::test::ScopedCommandLine scoped_current_cli;
  base::CommandLine* current_cli = scoped_current_cli.GetProcessCommandLine();
  current_cli->AppendSwitch("internal-switch-before");
  about_flags::ConvertFlagsToSwitches(&flags_storage, current_cli,
                                      flags_ui::kAddSentinels);
  current_cli->AppendSwitch("internal-switch-after");

  BrowserLaunchDataCollectorDesktop collector;
  auto&& event = collector.GetEvent();

  // Verify that flag switches are captured in addition to the initial
  // command line switches, and that all switches are sorted alphabetically.
  EXPECT_THAT(event.command_line_switch_keys(),
              testing::ElementsAre("flag-switch-1", "flag-switch-2", "switch-1",
                                   "switch-2"));
}

}  // namespace enterprise_reporting
