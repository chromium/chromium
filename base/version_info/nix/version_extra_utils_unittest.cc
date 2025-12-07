// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version_info/nix/version_extra_utils.h"

#include <memory>
#include <optional>
#include <string>

#include "base/environment.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/cstring_view.h"
#include "base/test/nix/scoped_chrome_version_extra_override.h"
#include "base/version_info/channel.h"
#include "build/branding_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace version_info::nix {

namespace {

class MockEnvironment : public base::Environment {
 public:
  MOCK_METHOD(std::optional<std::string>,
              GetVar,
              (base::cstring_view),
              (override));
  MOCK_METHOD(bool,
              SetVar,
              (base::cstring_view, const std::string& new_value),
              (override));
  MOCK_METHOD(bool, UnSetVar, (base::cstring_view), (override));
};

}  // namespace

TEST(VersionExtraUtilsTest, GetChannel) {
  MockEnvironment env;

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("stable"));
  EXPECT_EQ(version_info::Channel::STABLE, GetChannel(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("extended"));
  EXPECT_EQ(version_info::Channel::STABLE, GetChannel(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra))).WillOnce(Return("beta"));
  EXPECT_EQ(version_info::Channel::BETA, GetChannel(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("unstable"));
  EXPECT_EQ(version_info::Channel::DEV, GetChannel(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("canary"));
  EXPECT_EQ(version_info::Channel::CANARY, GetChannel(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return(std::nullopt));
  EXPECT_EQ(version_info::Channel::UNKNOWN, GetChannel(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("invalid"));
  EXPECT_EQ(version_info::Channel::UNKNOWN, GetChannel(env));
}

TEST(VersionExtraUtilsTest, IsExtendedStable) {
  MockEnvironment env;

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("extended"));
  EXPECT_TRUE(IsExtendedStable(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("stable"));
  EXPECT_FALSE(IsExtendedStable(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra))).WillOnce(Return("beta"));
  EXPECT_FALSE(IsExtendedStable(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return(std::nullopt));
  EXPECT_FALSE(IsExtendedStable(env));
}

TEST(VersionExtraUtilsTest, GetAppName) {
  MockEnvironment env;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const std::string kExpectedPrefix = "com.google.Chrome";
#else
  const std::string kExpectedPrefix = "org.chromium.Chromium";
#endif

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("stable"));
  EXPECT_EQ(kExpectedPrefix, GetAppName(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("extended"));
  EXPECT_EQ(kExpectedPrefix, GetAppName(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra))).WillOnce(Return("beta"));
  EXPECT_EQ(kExpectedPrefix + ".beta", GetAppName(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("unstable"));
  EXPECT_EQ(kExpectedPrefix + ".unstable", GetAppName(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("canary"));
  EXPECT_EQ(kExpectedPrefix + ".canary", GetAppName(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return(std::nullopt));
  EXPECT_EQ(kExpectedPrefix, GetAppName(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("invalid"));
  EXPECT_EQ(kExpectedPrefix, GetAppName(env));
}

TEST(VersionExtraUtilsTest, GetSessionNamePrefix) {
  MockEnvironment env;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const std::string kExpectedPrefix = "chrome";
#else
  const std::string kExpectedPrefix = "chromium";
#endif

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("stable"));
  EXPECT_EQ(kExpectedPrefix, GetSessionNamePrefix(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("extended"));
  EXPECT_EQ(kExpectedPrefix, GetSessionNamePrefix(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra))).WillOnce(Return("beta"));
  EXPECT_EQ(kExpectedPrefix + "_beta", GetSessionNamePrefix(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("unstable"));
  EXPECT_EQ(kExpectedPrefix + "_unstable", GetSessionNamePrefix(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("canary"));
  EXPECT_EQ(kExpectedPrefix + "_canary", GetSessionNamePrefix(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return(std::nullopt));
  EXPECT_EQ(kExpectedPrefix, GetSessionNamePrefix(env));

  EXPECT_CALL(env, GetVar(StrEq(kChromeVersionExtra)))
      .WillOnce(Return("invalid"));
  EXPECT_EQ(kExpectedPrefix, GetSessionNamePrefix(env));
}

}  // namespace version_info::nix
