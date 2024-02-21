// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/block_tests_writing_to_special_dirs.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class BlockTestsWritingToSpecialDirsTest : public testing::Test {
 public:
  BlockTestsWritingToSpecialDirsTest() {
    auto& prev_block_tests = Get();
    if (prev_block_tests.has_value()) {
      save_block_tests_.emplace(std::move(prev_block_tests->blocked_dirs_),
                                prev_block_tests->failure_callback_);
      prev_block_tests.reset();
    }
  }

  ~BlockTestsWritingToSpecialDirsTest() override {
    if (save_block_tests_.has_value()) {
      Get().emplace(std::move(save_block_tests_->blocked_dirs_),
                    save_block_tests_->failure_callback_);
    }
  }

 protected:
  std::optional<BlockTestsWritingToSpecialDirs>& Get() {
    return BlockTestsWritingToSpecialDirs::Get();
  }
  std::optional<BlockTestsWritingToSpecialDirs> save_block_tests_;
};

// Test that with no special dirs blocked,
// BlockTestsWritingToSpecialDirs::CanWriteToPath returns true.
TEST_F(BlockTestsWritingToSpecialDirsTest, NoSpecialDirWriteBlocker) {
  EXPECT_TRUE(BlockTestsWritingToSpecialDirs::CanWriteToPath(
      PathService::CheckedGet(DIR_SRC_TEST_DATA_ROOT).AppendASCII("file")));
}

TEST_F(BlockTestsWritingToSpecialDirsTest, SpecialDirWriteBlocker) {
  std::vector<int> dirs_to_block = {DIR_SRC_TEST_DATA_ROOT};
  if (PathService::IsOverriddenForTesting(dirs_to_block[0])) {
    GTEST_SKIP() << "DIR_SRC_TEST_DATA_ROOT is already overridden";
  }
  Get().emplace(std::move(dirs_to_block), ([](const FilePath& path) {}));

  EXPECT_FALSE(BlockTestsWritingToSpecialDirs::CanWriteToPath(
      PathService::CheckedGet(DIR_SRC_TEST_DATA_ROOT).AppendASCII("file")));
}

}  // namespace base
