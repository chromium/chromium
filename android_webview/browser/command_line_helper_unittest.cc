// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/command_line_helper.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Test;
using base::CommandLine;

const base::Feature kSomeSpecialFeature{"SomeSpecialFeature",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

class CommandLineHelperTest : public Test {
 public:
  CommandLineHelperTest() {}

  void EnableFeatureAndVerify(CommandLine& command_line,
                              const std::string& enabled_expected,
                              const std::string& disabled_expected) {
    CommandLineHelper::AddEnabledFeature(command_line,
                                         kSomeSpecialFeature.name);
    Verify(command_line, enabled_expected, disabled_expected);
  }

  void DisableFeatureAndVerify(CommandLine& command_line,
                               const std::string& enabled_expected,
                               const std::string& disabled_expected) {
    CommandLineHelper::AddDisabledFeature(command_line,
                                          kSomeSpecialFeature.name);
    Verify(command_line, enabled_expected, disabled_expected);
  }

  void Verify(const CommandLine& command_line,
              const std::string& enabled_expected,
              const std::string& disabled_expected) {
    EXPECT_EQ(enabled_expected,
              command_line.GetSwitchValueASCII(switches::kEnableFeatures));
    EXPECT_EQ(disabled_expected,
              command_line.GetSwitchValueASCII(switches::kDisableFeatures));
  }
};

TEST_F(CommandLineHelperTest, EnableForEmptyCommandLine) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  EnableFeatureAndVerify(command_line, "SomeSpecialFeature", "");
}

TEST_F(CommandLineHelperTest, EnableForNoEnabledFeatures) {
  const CommandLine::CharType* argv[] = {FILE_PATH_LITERAL("program")};
  CommandLine command_line(base::size(argv), argv);
  EnableFeatureAndVerify(command_line, "SomeSpecialFeature", "");
}

TEST_F(CommandLineHelperTest, EnableForEnabledTestFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--enable-features=TestFeature")};
  CommandLine command_line(base::size(argv), argv);
  EnableFeatureAndVerify(command_line, "TestFeature,SomeSpecialFeature", "");
}

TEST_F(CommandLineHelperTest, EnableForEnabledSomeSpecialFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--enable-features=SomeSpecialFeature,TestFeature")};
  CommandLine command_line(base::size(argv), argv);
  EnableFeatureAndVerify(command_line, "SomeSpecialFeature,TestFeature", "");
}

TEST_F(CommandLineHelperTest, EnableForDisabledSomeSpecialFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--disable-features=SomeSpecialFeature")};
  CommandLine command_line(base::size(argv), argv);
  EnableFeatureAndVerify(command_line, "", "SomeSpecialFeature");
}

TEST_F(CommandLineHelperTest, EnableForDisabledTestFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--disable-features=TestFeature")};
  CommandLine command_line(base::size(argv), argv);
  EnableFeatureAndVerify(command_line, "SomeSpecialFeature", "TestFeature");
}

TEST_F(CommandLineHelperTest, DisableForEmptyCommandLine) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  DisableFeatureAndVerify(command_line, "", "SomeSpecialFeature");
}

TEST_F(CommandLineHelperTest, DisableForNoDisabledFeatures) {
  const CommandLine::CharType* argv[] = {FILE_PATH_LITERAL("program")};
  CommandLine command_line(base::size(argv), argv);
  DisableFeatureAndVerify(command_line, "", "SomeSpecialFeature");
}

TEST_F(CommandLineHelperTest, DisableForDisabledTestFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--disable-features=TestFeature")};
  CommandLine command_line(base::size(argv), argv);
  DisableFeatureAndVerify(command_line, "", "TestFeature,SomeSpecialFeature");
}

TEST_F(CommandLineHelperTest, DisableForDisabledSomeSpecialFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--disable-features=SomeSpecialFeature,TestFeature")};
  CommandLine command_line(base::size(argv), argv);
  DisableFeatureAndVerify(command_line, "", "SomeSpecialFeature,TestFeature");
}

TEST_F(CommandLineHelperTest, DisableForEnabledSomeSpecialFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--enable-features=SomeSpecialFeature")};
  CommandLine command_line(base::size(argv), argv);
  DisableFeatureAndVerify(command_line, "SomeSpecialFeature", "");
}

TEST_F(CommandLineHelperTest, DisableForEnabledTestFeature) {
  const CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("program"),
      FILE_PATH_LITERAL("--enable-features=TestFeature")};
  CommandLine command_line(base::size(argv), argv);
  DisableFeatureAndVerify(command_line, "TestFeature", "SomeSpecialFeature");
}
