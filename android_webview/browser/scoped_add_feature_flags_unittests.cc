// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/scoped_add_feature_flags.h"

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::CommandLine;

namespace android_webview {

TEST(ScopedAddFeatureFlags, ConflictWithExistingFlags) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kEnableFeatures,
                                 "ExistingEnabledFoo,ExistingEnabledBar");
  command_line.AppendSwitchASCII(switches::kDisableFeatures,
                                 "ExistingDisabledFoo,ExistingDisabledBar");

  const base::Feature kExistingEnabledFoo{"ExistingEnabledFoo",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
  const base::Feature kExistingDisabledFoo{"ExistingDisabledFoo",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
  const base::Feature kEnabledBaz{"EnabledBaz",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
  const base::Feature kDisabledBaz{"DisabledBaz",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
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
}

}  // namespace android_webview
