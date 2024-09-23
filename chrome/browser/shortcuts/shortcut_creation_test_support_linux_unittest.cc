// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creation_test_support_linux.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace shortcuts::internal {

TEST(ShortcutCreationTestSupportLinuxTest, ParseDesktopExecForCommandLine) {
  const std::pair<std::string, std::vector<std::string>> kTestCases[] = {
      {"chrome", {"chrome"}},
      {"chrome hello  world", {"chrome", "hello", "world"}},
      {"\"quoted command\"  with arg", {"quoted command", "with", "arg"}},
      {"chrome \"with \\\"escaped\\\" args\"",
       {"chrome", "with \"escaped\" args"}},
  };
  for (auto& test_case : kTestCases) {
    EXPECT_EQ(ParseDesktopExecForCommandLine(test_case.first),
              test_case.second);
  }
}

}  // namespace shortcuts::internal
