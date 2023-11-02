// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/switch_utils.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SwitchUtilsTest, RemoveSwitches) {
  static const base::CommandLine::CharType* const argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--app=http://www.google.com/"),
      FILE_PATH_LITERAL("--force-first-run"),
      FILE_PATH_LITERAL("--make-default-browser"),
      FILE_PATH_LITERAL("--foo"),
      FILE_PATH_LITERAL("--bar")};
  base::CommandLine cmd_line(std::size(argv), argv);
  EXPECT_FALSE(cmd_line.GetCommandLineString().empty());

  base::CommandLine::SwitchMap switches = cmd_line.GetSwitches();
  EXPECT_EQ(5U, switches.size());

  switches::RemoveSwitchesForAutostart(&switches);
  EXPECT_EQ(2U, switches.size());
  EXPECT_TRUE(cmd_line.HasSwitch("foo"));
  EXPECT_TRUE(cmd_line.HasSwitch("bar"));
}

#if BUILDFLAG(IS_WIN)
TEST(SwitchUtilsTest, RemoveSwitchesFromString) {
  // All these command line args (except foo and bar) will
  // be removed after RemoveSwitchesForAutostart:
  base::CommandLine cmd_line = base::CommandLine::FromString(
      L"program"
      L" --app=http://www.google.com/"
      L" --force-first-run"
      L" --make-default-browser"
      L" --foo"
      L" --bar");
  EXPECT_FALSE(cmd_line.GetCommandLineString().empty());

  base::CommandLine::SwitchMap switches = cmd_line.GetSwitches();
  EXPECT_EQ(5U, switches.size());

  switches::RemoveSwitchesForAutostart(&switches);
  EXPECT_EQ(2U, switches.size());
  EXPECT_TRUE(cmd_line.HasSwitch("foo"));
  EXPECT_TRUE(cmd_line.HasSwitch("bar"));
}

TEST(SwitchUtilsTest, RemovePrefetchSwitch) {
  static const base::CommandLine::CharType* const argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--foo"),
      FILE_PATH_LITERAL("/prefetch:1"),
      FILE_PATH_LITERAL("--bar")};
  base::CommandLine cmd_line(std::size(argv), argv);
  EXPECT_FALSE(cmd_line.GetCommandLineString().empty());

  base::CommandLine::SwitchMap switches = cmd_line.GetSwitches();
  EXPECT_EQ(3U, switches.size());

  switches::RemoveSwitchesForAutostart(&switches);
  EXPECT_EQ(2U, switches.size());
  EXPECT_TRUE(cmd_line.HasSwitch("foo"));
  EXPECT_TRUE(cmd_line.HasSwitch("bar"));
}

TEST(SwitchUtilsTest, RemovePrefetchSwitchAndNormalSwitch) {
  static const base::CommandLine::CharType* const argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--foo"),
      FILE_PATH_LITERAL("/prefetch:1"),
      FILE_PATH_LITERAL("--force-first-run"),
      FILE_PATH_LITERAL("--bar")};
  base::CommandLine cmd_line(std::size(argv), argv);
  EXPECT_FALSE(cmd_line.GetCommandLineString().empty());

  base::CommandLine::SwitchMap switches = cmd_line.GetSwitches();
  EXPECT_EQ(4U, switches.size());

  switches::RemoveSwitchesForAutostart(&switches);
  EXPECT_EQ(2U, switches.size());
  EXPECT_TRUE(cmd_line.HasSwitch("foo"));
  EXPECT_TRUE(cmd_line.HasSwitch("bar"));
}
#endif  // BUILDFLAG(IS_WIN)
