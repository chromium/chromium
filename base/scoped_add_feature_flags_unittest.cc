// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_add_feature_flags.h"

#include <cstring>
#include <string>
#include <string_view>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Converts a string to CommandLine::StringType, which is std::wstring on
// Windows and std::string on other platforms.
CommandLine::StringType ToCommandLineStringType(std::string_view s) {
  return CommandLine::StringType(s.begin(), s.end());
}

}  // namespace

TEST(ScopedAddFeatureFlags, ConflictWithExistingFlags) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kEnableFeatures,
                                 "ExistingEnabledFoo,ExistingEnabledBar");
  command_line.AppendSwitchASCII(switches::kDisableFeatures,
                                 "ExistingDisabledFoo,ExistingDisabledBar");

  static BASE_FEATURE(kExistingEnabledFoo, "ExistingEnabledFoo",
                      FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(kExistingDisabledFoo, "ExistingDisabledFoo",
                      FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(kEnabledBaz, "EnabledBaz", FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(kDisabledBaz, "DisabledBaz", FEATURE_DISABLED_BY_DEFAULT);
  {
    ScopedAddFeatureFlags scoped_add(&command_line);
    scoped_add.EnableIfNotSet(kExistingEnabledFoo);
    scoped_add.EnableIfNotSet(kExistingDisabledFoo);
    scoped_add.EnableIfNotSet(kEnabledBaz);
    scoped_add.DisableIfNotSet(kExistingEnabledFoo);
    scoped_add.DisableIfNotSet(kExistingDisabledFoo);
    scoped_add.DisableIfNotSet(kDisabledBaz);
  }

  EXPECT_EQ(std::string("ExistingEnabledFoo,ExistingEnabledBar,EnabledBaz"),
            command_line.GetSwitchValueASCII(switches::kEnableFeatures));
  EXPECT_EQ(std::string("ExistingDisabledFoo,ExistingDisabledBar,DisabledBaz"),
            command_line.GetSwitchValueASCII(switches::kDisableFeatures));

  // There should not be duplicate --enable-features or --disable-features flags
  EXPECT_EQ(
      ToCommandLineStringType(
          " --enable-features=ExistingEnabledFoo,ExistingEnabledBar,EnabledBaz"
          " --disable-features=ExistingDisabledFoo,ExistingDisabledBar,"
          "DisabledBaz"),
      JoinString(command_line.argv(), ToCommandLineStringType(" ")));
}

TEST(ScopedAddFeatureFlags, FlagWithParameter) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kEnableFeatures,
                                 "ExistingEnabledFoo");
  static BASE_FEATURE(kExistingEnabledFoo, "ExistingEnabledFoo",
                      FEATURE_DISABLED_BY_DEFAULT);
  static BASE_FEATURE(kFeatureWithParameter, "FeatureWithParam",
                      FEATURE_DISABLED_BY_DEFAULT);

  {
    ScopedAddFeatureFlags scoped_add(&command_line);
    scoped_add.EnableIfNotSet(kExistingEnabledFoo);
    scoped_add.EnableIfNotSetWithParameter(kFeatureWithParameter, "name",
                                           "value");
    EXPECT_TRUE(scoped_add.IsEnabledWithParameter(kFeatureWithParameter, "name",
                                                  "value"));
  }

  EXPECT_EQ(std::string("ExistingEnabledFoo,FeatureWithParam:name/value"),
            command_line.GetSwitchValueASCII(switches::kEnableFeatures));
}

}  // namespace base
