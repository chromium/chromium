// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_crash_keys.h"

#include "components/crash/core/common/crash_key.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <set>
#include <string>

namespace android_webview {

namespace {

class AwCrashKeysTest : public testing::Test {
 public:
  void SetUp() override { crash_reporter::InitializeCrashKeys(); }

  void TearDown() override {
    // Breakpad doesn't properly support ResetCrashKeysForTesting() and usually
    // CHECK fails after it is called.
#if BUILDFLAG(USE_CRASHPAD_ANNOTATION)
    crash_reporter::ResetCrashKeysForTesting();
#endif
  }
};

}  // namespace

TEST_F(AwCrashKeysTest, AddFeaturesAndSwitches) {
  // Sets are stored in ascending order so making the names alphabetical to make
  // the expectations below super obvious
  std::set<std::string> switches{"--a-switch", "--b-switch"};
  std::set<std::string> features{"AFeature:enabled", "BFeature:enabled",
                                 "CFeature:disabled"};

  SetCrashKeysFromFeaturesAndSwitches(switches, features);

  EXPECT_EQ("--a-switch", crash_reporter::GetCrashKeyValue("switch-1"));
  EXPECT_EQ("--b-switch", crash_reporter::GetCrashKeyValue("switch-2"));
  EXPECT_EQ("2", crash_reporter::GetCrashKeyValue("num-switches"));

  EXPECT_EQ("AFeature",
            crash_reporter::GetCrashKeyValue("commandline-enabled-feature-1"));
  EXPECT_EQ("BFeature",
            crash_reporter::GetCrashKeyValue("commandline-enabled-feature-2"));
  EXPECT_EQ("CFeature",
            crash_reporter::GetCrashKeyValue("commandline-disabled-feature-1"));
}

}  // namespace android_webview
