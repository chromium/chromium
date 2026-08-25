// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shutdown_watchdog_mac.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shutdown_watchdog {
namespace {

TEST(ShutdownWatchdogMacTest, ArmForSessionEnding) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitch(switches::kTestType);
  // Verify that calling ArmForSessionEnding() safely signals shutdown complete
  // and is idempotent without creating unwanted threads.
  ArmForSessionEnding();
  ArmForSessionEnding();
}

TEST(ShutdownWatchdogMacTest, SafeWhenWatchdogsDisabled) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();

  // Explicitly ensure test-type switch is present to simulate disabled
  // watchdogs.
  command_line->AppendSwitch(switches::kTestType);
  command_line->AppendSwitch("disable-hang-monitor");
  command_line->AppendSwitch("no-watchdog");

  // Verify that all watchdog entry points are safe and non-blocking when
  // disabled.
  BlockOnSigtermShutdown();
  OnBrowserTearDownStarted();
  OnShutdownComplete();
  ArmForSessionEnding();
}

}  // namespace
}  // namespace shutdown_watchdog
