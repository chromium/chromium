// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

constexpr FilePath::CharType kDirPrefix[] =
    FILE_PATH_LITERAL("test_scoped_temp_dir");

// Deletes all registered file paths upon test completion. There can only be
// one instance at a time.
class PathDeleterOnTestEnd : public testing::EmptyTestEventListener {
 public:
  PathDeleterOnTestEnd() {
    DCHECK(!instance_);
    instance_ = this;
  }

  ~PathDeleterOnTestEnd() override {
    DCHECK_EQ(instance_, this);
    instance_ = nullptr;
  }

  PathDeleterOnTestEnd(const PathDeleterOnTestEnd&) = delete;
  PathDeleterOnTestEnd& operator=(const PathDeleterOnTestEnd&) = delete;

  static PathDeleterOnTestEnd* GetInstance() { return instance_; }

  void DeletePathRecursivelyUponTestEnd(const FilePath& path) {
    file_paths_to_delete_.push_back(path);
  }

  // EmptyTestEventListener overrides.
  void OnTestEnd(const testing::TestInfo& test_info) override {
    if (file_paths_to_delete_.empty()) {
      // Nothing to delete since the last test ended.
      return;
    }

    ScopedAllowBlockingForTesting allow_blocking;
    for (const FilePath& file_path : file_paths_to_delete_) {
      if (!DieFileDie(file_path, /*recurse=*/true)) {
        ADD_FAILURE() << "Failed to delete temporary directory for testing: "
                      << file_path;
      }
    }
    file_paths_to_delete_.clear();
  }

 private:
  static PathDeleterOnTestEnd* instance_;
  std::vector<FilePath> file_paths_to_delete_;
};

// static
PathDeleterOnTestEnd* PathDeleterOnTestEnd::instance_ = nullptr;

}  // namespace

bool EvictFileFromSystemCacheWithRetry(const FilePath& path) {
  const int kCycles = 10;
  const TimeDelta kDelay = TestTimeouts::action_timeout() / kCycles;
  for (int i = 0; i < kCycles; i++) {
    if (EvictFileFromSystemCache(path))
      return true;
    PlatformThread::Sleep(kDelay);
  }
  return false;
}

FilePath GetTempDirForTesting() {
  FilePath path;
  CHECK(GetTempDir(&path));
  return path;
}

FilePath CreateUniqueTempDirectoryScopedToTest() {
  ScopedAllowBlockingForTesting allow_blocking;
  FilePath path;
  if (!CreateNewTempDirectory(kDirPrefix, &path)) {
    ADD_FAILURE() << "Failed to create unique temporary directory for testing.";
    return FilePath();
  }

  if (!PathDeleterOnTestEnd::GetInstance()) {
    // Append() transfers ownership of the listener. This means
    // PathDeleterOnTestEnd::GetInstance() will return non-null until all tests
    // are run and the test suite destroyed.
    testing::UnitTest::GetInstance()->listeners().Append(
        new PathDeleterOnTestEnd());
    DCHECK(PathDeleterOnTestEnd::GetInstance());
  }

  PathDeleterOnTestEnd::GetInstance()->DeletePathRecursivelyUponTestEnd(path);

  return path;
}

}  // namespace base
