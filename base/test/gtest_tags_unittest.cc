// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_tags.h"

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/test/test_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class GtestTagsTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kTestLauncherOutput)) {
      GTEST_SKIP() << "XmlUnitTestResultPrinterTest is not initialized "
                   << "for single process tests.";
    }
  }
};

TEST_F(GtestTagsTest, AddInvalidName) {
  EXPECT_DCHECK_DEATH(AddTagToTestResult("", "value"));
}

TEST_F(GtestTagsTest, AddValidTag) {
  AddTagToTestResult("name", "value");
}

}  // namespace base
