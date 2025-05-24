// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_sub_test_results.h"

#include <optional>

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/test/test_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class GtestSubTestResultsTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kTestLauncherOutput)) {
      GTEST_SKIP() << "XmlUnitTestResultPrinterTest is not initialized "
                   << "for single process tests.";
    }
  }
};

TEST_F(GtestSubTestResultsTest, EmptyName) {
  EXPECT_CHECK_DEATH(AddSubTestResult("", 1, std::nullopt));
}

TEST_F(GtestSubTestResultsTest, InvalidName) {
  EXPECT_CHECK_DEATH(AddSubTestResult("invalid-name", 1, std::nullopt));
}

TEST_F(GtestSubTestResultsTest, ValidName) {
  AddSubTestResult("name123_", 1, std::nullopt);
}

}  // namespace base
