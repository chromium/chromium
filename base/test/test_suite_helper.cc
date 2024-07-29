// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite_helper.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"

namespace base::test {

void InitScopedFeatureListForTesting(ScopedFeatureList& scoped_feature_list) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  // We set up a FeatureList via ScopedFeatureList::InitFromCommandLine().
  // This ensures that code using that API will not hit an error that it's
  // not set. It will be cleared by ~ScopedFeatureList().

  // TestFeatureForBrowserTest1 and TestFeatureForBrowserTest2 used in
  // ContentBrowserTestScopedFeatureListTest to ensure ScopedFeatureList keeps
  // features from command line.
  // TestBlinkFeatureDefault is used in RuntimeEnabledFeaturesTest to test a
  // behavior with OverrideState::OVERIDE_USE_DEFAULT.
  std::string enabled =
      command_line->GetSwitchValueASCII(switches::kEnableFeatures);
  std::string disabled =
      command_line->GetSwitchValueASCII(switches::kDisableFeatures);
  enabled += ",TestFeatureForBrowserTest1,*TestBlinkFeatureDefault";
  disabled += ",TestFeatureForBrowserTest2";
  scoped_feature_list.InitFromCommandLine(enabled, disabled);

  // The enable-features and disable-features flags were just slurped into a
  // FeatureList, so remove them from the command line. Tests should enable
  // and disable features via the ScopedFeatureList API rather than
  // command-line flags.
  CommandLine new_command_line(command_line->GetProgram());
  CommandLine::SwitchMap switches = command_line->GetSwitches();

  switches.erase(switches::kEnableFeatures);
  switches.erase(switches::kDisableFeatures);

  for (const auto& iter : switches) {
    new_command_line.AppendSwitchNative(iter.first, iter.second);
  }

  *CommandLine::ForCurrentProcess() = new_command_line;
}

}  // namespace base::test
