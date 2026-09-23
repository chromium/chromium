// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_launch/browser_launch_data_collector_desktop.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "base/process/process.h"
#include "chrome/browser/enterprise/reporting/browser_launch/scoped_initial_command_line.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/webui/flags/flags_ui_switches.h"

namespace enterprise_reporting {

namespace {

// Extracts the switch keys located between `--flag-switches-begin` and
// `--flag-switches-end` (i.e., flags enabled via `chrome://flags`) from
// `command_line`.
std::set<std::string> GetFlagSwitchKeysFromCommandLine(
    const base::CommandLine& command_line) {
  // Use a temporary `CommandLine` to format the sentinel switches into native
  // `argv` strings with the platform command-line prefix (`--` or `/`) so we
  // can search `command_line.argv()` for them.
  base::CommandLine sentinels(base::CommandLine::NO_PROGRAM);
  sentinels.AppendSwitch(switches::kFlagSwitchesBegin);
  sentinels.AppendSwitch(switches::kFlagSwitchesEnd);
  const auto& begin_flag = sentinels.argv()[1];
  const auto& end_flag = sentinels.argv()[2];

  const auto& argv = command_line.argv();
  auto begin_it = std::ranges::find(argv, begin_flag);
  if (begin_it == argv.end()) {
    return {};
  }
  auto end_it = std::ranges::find(begin_it + 1, argv.end(), end_flag);
  if (end_it == argv.end()) {
    return {};
  }
  base::CommandLine flag_switches_cli =
      base::CommandLine::FromArgvWithoutProgram(
          base::CommandLine::StringVector(begin_it + 1, end_it));

  std::set<std::string> switch_keys;
  for (const auto& [name, value] : flag_switches_cli.GetSwitches()) {
    switch_keys.insert(name);
  }
  return switch_keys;
}

}  // namespace

BrowserLaunchDataCollectorDesktop::BrowserLaunchDataCollectorDesktop() =
    default;
BrowserLaunchDataCollectorDesktop::~BrowserLaunchDataCollectorDesktop() =
    default;

::chrome::cros::reporting::proto::BrowserLaunchEvent&&
BrowserLaunchDataCollectorDesktop::GetEvent() {
  event_.Clear();

  std::set<std::string> switch_keys;

  // Capture initial command line switch keys.
  const base::CommandLine& initial_cli = GetInitialBrowserCommandLine();
  for (const auto& [name, value] : initial_cli.GetSwitches()) {
    switch_keys.insert(name);
  }

  // Capture flag switch keys from the current process command line.
  CHECK(base::CommandLine::InitializedForCurrentProcess());
  switch_keys.merge(GetFlagSwitchKeysFromCommandLine(
      *base::CommandLine::ForCurrentProcess()));

  for (const std::string& key : switch_keys) {
    event_.add_command_line_switch_keys(key);
  }

  // Capture Process Creation Time.
  event_.set_launch_time_millis(
      base::Process::Current().CreationTime().InMillisecondsSinceUnixEpoch());

  return std::move(event_);
}

}  // namespace enterprise_reporting
