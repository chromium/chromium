// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/device_name/device_name_validator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class DeviceNameValidatorTest : public testing::Test {
 public:
  DeviceNameValidatorTest() {}
};

TEST_F(DeviceNameValidatorTest, ValidName) {
  EXPECT_EQ(true, IsValidDeviceName("TestName"));
}

TEST_F(DeviceNameValidatorTest, ValidNameWithNumbers) {
  EXPECT_EQ(true, IsValidDeviceName("TestName123"));
}

TEST_F(DeviceNameValidatorTest, ValidNameWithHyphen) {
  EXPECT_EQ(true, IsValidDeviceName("TestName-"));
}

TEST_F(DeviceNameValidatorTest, ValidNameWith15Characters) {
  EXPECT_EQ(true, IsValidDeviceName("012345678901234"));
}

TEST_F(DeviceNameValidatorTest, InvalidNameEmptyString) {
  EXPECT_EQ(false, IsValidDeviceName(""));
}

TEST_F(DeviceNameValidatorTest, InvalidNameWithWhitespace) {
  EXPECT_EQ(false, IsValidDeviceName("Test Name"));
}

TEST_F(DeviceNameValidatorTest, InvalidNameWithSpecialCharacters) {
  EXPECT_EQ(false, IsValidDeviceName("Testname@#!&"));
}

TEST_F(DeviceNameValidatorTest, InValidNameWithMoreThan15Characters) {
  EXPECT_EQ(false, IsValidDeviceName("0123456789012345"));
}

}  // namespace ash
