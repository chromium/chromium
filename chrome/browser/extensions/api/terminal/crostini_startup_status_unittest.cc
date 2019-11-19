// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/crostini_startup_status.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class CrostiniStartupStatusTest : public testing::Test {
 protected:
  void Print(const std::string& output) {
    output_.emplace_back(std::move(output));
  }

  void Done() { done_ = true; }

  CrostiniStartupStatus* NewStartupStatus(bool verbose) {
    return new CrostiniStartupStatus(
        base::BindRepeating(&CrostiniStartupStatusTest::Print,
                            base::Unretained(this)),
        verbose,
        base::BindOnce(&CrostiniStartupStatusTest::Done,
                       base::Unretained(this)));
  }

  void SetUp() override {}

  std::vector<std::string> output_;
  bool done_ = false;
};

TEST_F(CrostiniStartupStatusTest, TestNotVerbose) {
  auto* startup_status = NewStartupStatus(false);
  startup_status->OnStageStarted(InstallerState::kStart);
  startup_status->OnStageStarted(InstallerState::kInstallImageLoader);
  startup_status->OnComponentLoaded(crostini::CrostiniResult::SUCCESS);
  startup_status->OnCrostiniRestarted(crostini::CrostiniResult::SUCCESS);

  EXPECT_TRUE(done_);

  // Hides cursor, shows cursor.
  EXPECT_EQ(output_.size(), 2u);
  EXPECT_EQ(output_[0], "\x1b[?25l");
  EXPECT_EQ(output_[1], "\x1b[?25h");
}

TEST_F(CrostiniStartupStatusTest, TestVerbose) {
  auto* startup_status = NewStartupStatus(true);
  startup_status->OnStageStarted(InstallerState::kStart);
  startup_status->OnStageStarted(InstallerState::kInstallImageLoader);
  startup_status->OnComponentLoaded(crostini::CrostiniResult::SUCCESS);
  startup_status->OnCrostiniRestarted(crostini::CrostiniResult::SUCCESS);
  EXPECT_TRUE(done_);

  // Hides cursor, version, start, status, component, status, done, status,
  // ready, shows cursor.
  EXPECT_EQ(output_.size(), 10u);
  EXPECT_EQ(output_[0], "\x1b[?25l");
  EXPECT_EQ(output_[1].find("Chrome OS "), 24u);
  EXPECT_EQ(output_[2].substr(24), "Starting... 🤔\r\n");
  EXPECT_EQ(output_[3],
            "[\x1b[7m \x1b[27m\x1b[35m........\x1b[0m] \x1b[34m|\x1b[0m\r");
  EXPECT_EQ(output_[4].substr(24), "Checking cros-termina component...\r\n");
  EXPECT_EQ(output_[5],
            "[\x1b[7m  \x1b[27m\x1b[35m.......\x1b[0m] \x1b[34m|\x1b[0m\r");
  std::string expected = "\x1b[A";
  for (int i = 0; i < 59; ++i)
    expected += "\x1b[C";
  expected += "\x1b[32mdone\x1b[0m \xE2\x9C\x94\xEF\xb8\x8F\r\n";
  EXPECT_EQ(output_[6], expected);
  EXPECT_EQ(output_[7],
            "[\x1b[7m  \x1b[27m\x1b[35m.......\x1b[0m] \x1b[34m|\x1b[0m\r");
  EXPECT_EQ(output_[8].find("Ready"), 24u);
  EXPECT_EQ(output_[9], "\x1b[?25h");
}

}  // namespace extensions
