// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/block_tests_writing_to_special_dirs.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
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

TEST_F(BlockTestsWritingToSpecialDirsTest, AllowTempDirSubdirectories) {
  FilePath src_dir = PathService::CheckedGet(DIR_SRC_TEST_DATA_ROOT);
  FilePath temp_sub_dir = src_dir.AppendASCII("temp_test_dir");
  // Use should_skip_check=true because `temp_sub_dir` is a synthetic path that
  // does not exist on disk. On POSIX/Fuchsia, MakeAbsoluteFilePath requires
  // the path to exist on disk and would fail otherwise (and on Fuchsia,
  // DIR_SRC_TEST_DATA_ROOT is a read-only /pkg mount).
  base::ScopedPathOverride temp_override(DIR_TEMP, temp_sub_dir,
                                         /*should_skip_check=*/true);

  std::vector<int> dirs_to_block = {DIR_SRC_TEST_DATA_ROOT};
  if (PathService::IsOverriddenForTesting(dirs_to_block[0])) {
    GTEST_SKIP() << "DIR_SRC_TEST_DATA_ROOT is already overridden";
  }
  Get().emplace(std::move(dirs_to_block), ([](const FilePath& path) {}));

  // Writing to `src_dir` outside `temp_sub_dir' should be blocked.
  EXPECT_FALSE(BlockTestsWritingToSpecialDirs::CanWriteToPath(
      src_dir.AppendASCII("other_file")));

  // Writing to `temp_sub_dir` (or any subdirectory of it) should be allowed
  // even though it is located under the blocked `src_dir`.
  EXPECT_TRUE(BlockTestsWritingToSpecialDirs::CanWriteToPath(
      temp_sub_dir.AppendASCII("temp_file")));
  EXPECT_TRUE(BlockTestsWritingToSpecialDirs::CanWriteToPath(
      temp_sub_dir.AppendASCII("subdir").AppendASCII("temp_file")));
}

}  // namespace base
