// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_links.h"

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/test/test_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class GtestLinksTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kTestLauncherOutput)) {
      GTEST_SKIP() << "XmlUnitTestResultPrinterTest is not initialized "
                   << "for single process tests.";
    }
  }
};

TEST_F(GtestLinksTest, AddInvalidLink) {
  EXPECT_DCHECK_DEATH(AddLinkToTestResult("unique_link", "invalid`"));
}

TEST_F(GtestLinksTest, AddInvalidName) {
  EXPECT_DCHECK_DEATH(AddLinkToTestResult("invalid-name", "http://google.com"));
}

TEST_F(GtestLinksTest, AddValidLink) {
  AddLinkToTestResult("name", "http://google.com");
}

}  // namespace base
