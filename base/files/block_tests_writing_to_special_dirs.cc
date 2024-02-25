// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/block_tests_writing_to_special_dirs.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/path_service.h"

namespace base {

// static
std::optional<BlockTestsWritingToSpecialDirs>&
BlockTestsWritingToSpecialDirs::Get() {
  static NoDestructor<std::optional<BlockTestsWritingToSpecialDirs>>
      block_tests_writing_to_special_dirs;
  return *block_tests_writing_to_special_dirs;
}

// static
bool BlockTestsWritingToSpecialDirs::CanWriteToPath(const FilePath& path) {
  auto& dir_blocker = Get();
  if (!dir_blocker.has_value()) {
    return true;
  }
  if (!dir_blocker->blocked_dirs_.empty()) {
    // `blocked_paths_` needs to be initialized lazily because PathService::Get
    // can't be called from the test harness code before the indiviudal tests
    // run. On Windows, calling PathService::Get in the test harness startup
    // codel causes user32.dll to get loaded, which breaks  delayload_unittests.
    // On the Mac, it triggers a change in `AmIBundled`.
    for (const int dir_key : dir_blocker->blocked_dirs_) {
      // If test infrastructure has overridden `dir_key` already, there is no
      // need to block writes to it. Android tests apparently do this.
      if (PathService::IsOverriddenForTesting(dir_key)) {
        continue;
      }
      FilePath path_to_block;
      // Sandbox can make PathService::Get fail.
      if (PathService::Get(dir_key, &path_to_block)) {
        dir_blocker->blocked_paths_.insert(std::move(path_to_block));
      }
    }
    dir_blocker->blocked_dirs_.clear();
  }
  for (const auto& path_to_block : dir_blocker->blocked_paths_) {
    if (path_to_block.IsParent(path)) {
      (*dir_blocker->failure_callback_)(path);
      return false;
    }
  }
  return true;
}

BlockTestsWritingToSpecialDirs::BlockTestsWritingToSpecialDirs(
    std::vector<int> blocked_dirs,
    FileWriteBlockedForTestingFunctionPtr failure_callback)
    : blocked_dirs_(std::move(blocked_dirs)),
      failure_callback_(failure_callback) {}

BlockTestsWritingToSpecialDirs::~BlockTestsWritingToSpecialDirs() = default;

}  // namespace base
