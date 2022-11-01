// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_win.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/about_flags.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test GetRestartCommandLine function behavior. It should remove the program at
// the beginning of the command line, and any non-switch args, since the
// intention is to restore the session, by adding --restore-last-session. It
// should also remove any flags within the flag sentinels, and the sentinels
// themselves.
TEST(ChromeBrowserMainWinTest, GetRestartCommand) {
  base::FilePath chrome_path(L"chrome.exe");
  base::CommandLine simple_command_line(chrome_path);

  // Simple command line with just the program.
  const base::CommandLine::StringType kNoArgsResult =
      L" --restore-last-session --restart";
  base::CommandLine restart_command_line =
      ChromeBrowserMainPartsWin::GetRestartCommandLine(simple_command_line);
  EXPECT_EQ(restart_command_line.GetCommandLineString(), kNoArgsResult);

  // Command line with a url argument - url should be removed.
  base::CommandLine url_command_line(chrome_path);
  url_command_line.AppendArg("https://www.example.com");
  restart_command_line =
      ChromeBrowserMainPartsWin::GetRestartCommandLine(url_command_line);
  EXPECT_EQ(restart_command_line.GetCommandLineString(), kNoArgsResult);

  // Command line with a retained switch.
  const std::string kRetainedSwitch = "--enable-sandbox-audio";
  const base::CommandLine::StringType kRetainedSwitchResult =
      L" --enable-sandbox-audio --restore-last-session --restart";
  base::CommandLine retained_switch_command_line(chrome_path);
  retained_switch_command_line.AppendSwitch(kRetainedSwitch);
  restart_command_line = ChromeBrowserMainPartsWin::GetRestartCommandLine(
      retained_switch_command_line);
  EXPECT_EQ(restart_command_line.GetCommandLineString(), kRetainedSwitchResult);

  // Command line with flag switches.
  base::CommandLine experiments_command_line(chrome_path);
  // Add an --enable-features flag outside the flag sentinels, as if the user or
  // admin had added it. It should be retained, because Chrome won't add it
  // automatically.
  experiments_command_line.AppendSwitchASCII(switches::kEnableFeatures, "Exp2");
  experiments_command_line.AppendSwitch("enable-foo");
  experiments_command_line.AppendSwitch("--enable-sandbox-audio");

  // Setup the feature infrastructure so that ConvertFlagsToSwitches works.
  TestingPrefServiceSimple prefs;
  flags_ui::PrefServiceFlagsStorage flags_storage(&prefs);
  prefs.registry()->RegisterListPref(flags_ui::prefs::kAboutFlagsEntries);
  prefs.registry()->RegisterDictionaryPref(
      flags_ui::prefs::kAboutFlagsOriginLists);
  const char kExperimentName[] = "exp-flag";
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries(
      {{kExperimentName, "Exp", "description", static_cast<unsigned short>(-1),
        ORIGIN_LIST_VALUE_TYPE("flag-switch", "")}});
  about_flags::SetFeatureEntryEnabled(&flags_storage, kExperimentName,
                                      /*enable=*/true);
  about_flags::ConvertFlagsToSwitches(&flags_storage, &experiments_command_line,
                                      flags_ui::kAddSentinels);
  // Check that ConvertFlagsToSwitches added the start sentinel.
  ASSERT_TRUE(
      experiments_command_line.HasSwitch(::switches::kFlagSwitchesBegin));
  ASSERT_EQ(
      experiments_command_line.GetCommandLineString(),
      L"chrome.exe --enable-features=Exp2 --enable-foo --enable-sandbox-audio"
      L" --flag-switches-begin --flag-switch --flag-switches-end");
  // Check that the args and flag switches and sentinels are removed.
  restart_command_line = ChromeBrowserMainPartsWin::GetRestartCommandLine(
      experiments_command_line);
  EXPECT_EQ(restart_command_line.GetCommandLineString(),
            L" --enable-features=Exp2 --enable-foo"
            L" --enable-sandbox-audio"
            L" --restore-last-session --restart");
}

// Test RegisterApplicationRestart to make sure there are no crashes.
TEST(ChromeBrowserMainWinTest, RegisterRestart) {
  const base::CommandLine command_line = base::CommandLine::FromString(
      L"chrome.exe --enable-features=Exp2 --enable-foo -- "
      L"http://www.chromium.org");
  ChromeBrowserMainPartsWin::RegisterApplicationRestart(command_line);
}
