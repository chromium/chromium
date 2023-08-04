// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/webui/settings/public/constants/routes_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::settings {

namespace {

class RoutesUtilTest : public testing::Test {};

TEST_F(RoutesUtilTest, WellKnownRoutesAreValid) {
  ASSERT_TRUE(IsOSSettingsSubPage("help"));
  ASSERT_TRUE(IsOSSettingsSubPage("internet"));
  ASSERT_TRUE(IsOSSettingsSubPage("power"));
  ASSERT_TRUE(IsOSSettingsSubPage("networks"));
  ASSERT_TRUE(IsOSSettingsSubPage("networks?type=Tether"));
  ASSERT_TRUE(IsOSSettingsSubPage("networkDetail?guid=123456"));
}

TEST_F(RoutesUtilTest, BadRoutesAreValid) {
  ASSERT_FALSE(IsOSSettingsSubPage("a_wrong_url"));
  ASSERT_FALSE(IsOSSettingsSubPage("a_wrong_url?param=bad_param"));
}

}  // namespace

}  // namespace chromeos::settings
