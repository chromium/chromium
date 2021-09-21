// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/async_log.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace {

const char kLogFileName[] = "test_async_log";

}  // namespace

class AsyncLogTest : public testing::Test {
 public:
  AsyncLogTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.GetPath().AppendASCII(kLogFileName);
  }

  ~AsyncLogTest() override = default;

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath log_path_;
};

TEST_F(AsyncLogTest, Empty) {
  AsyncLog log(log_path_);

  // The file won't until it is written to.
  EXPECT_FALSE(base::PathExists(log_path_));
}

}  // namespace diagnostics
}  // namespace ash
