// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/crostini_startup_status.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using crostini::mojom::InstallerState;

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
        verbose);
  }

  void SetUp() override {}

  std::vector<std::string> output_;
  bool done_ = false;
};

TEST_F(CrostiniStartupStatusTest, TestNotVerbose) {
  auto* startup_status = NewStartupStatus(false);
  startup_status->OnStageStarted(InstallerState::kStart);
  startup_status->OnStageStarted(InstallerState::kInstallImageLoader);
  startup_status->OnCrostiniRestarted(crostini::CrostiniResult::SUCCESS);

  EXPECT_EQ(output_.size(), 1u);
  // CR, delete line, default color, show cursor.
  EXPECT_EQ(output_[0], "\r\x1b[K\x1b[0m\x1b[?25h");
}

TEST_F(CrostiniStartupStatusTest, TestVerbose) {
  auto* startup_status = NewStartupStatus(true);
  startup_status->OnStageStarted(InstallerState::kStart);
  startup_status->OnStageStarted(InstallerState::kInstallImageLoader);
  startup_status->OnCrostiniRestarted(crostini::CrostiniResult::SUCCESS);

  ASSERT_EQ(output_.size(), 5u);
  // Hide cursor, init progress.
  EXPECT_EQ(output_[0], "\x1b[?25l\x1b[35m[          ] ");

  // CR, purple, forward 12, yellow, stage.
  EXPECT_EQ(output_[1], "\r\x1b[35m[\x1b[12C\x1b[K\x1b[33mInitializing ");

  // CR, purple, progress, forward 11, erase, yellow, stage.
  EXPECT_EQ(output_[2],
            "\r\x1b[35m[=\x1b[11C\x1b[K\x1b[33mChecking the virtual machine ");

  // CR, purple, progress, forward 2, erase, green, done, symbol, CRLF.
  EXPECT_EQ(output_[3],
            "\r\x1b[35m[==========\x1b[2C\x1b[K\x1b[1;32mReady\r\n ");

  // CR, delete line, default color, show cursor;
  EXPECT_EQ(output_[4], "\r\x1b[K\x1b[0m\x1b[?25h");
}

}  // namespace extensions
