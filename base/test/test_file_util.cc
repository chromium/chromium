// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
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
        base::FileEnumerator enumerator(
            file_path, true,
            base::FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
        int num_non_ignored_files = 0;
        for (base::FilePath failed_to_delete = enumerator.Next();
             !failed_to_delete.empty(); failed_to_delete = enumerator.Next()) {
#if BUILDFLAG(IS_WIN)
          // Ignore failure to delete a directory. Most likely, it's because
          // we failed to delete one or more files or subdirectories in the
          // directory. Otherwise, the paths allowed to leak would all have to
          // be top-level sub-directories of `file_path`. This could be worked
          // around, but it's not worth complicating the code.
          if (enumerator.GetInfo().IsDirectory()) {
            continue;
          }
          // Ignore this file if its path contains one of the sub-path names
          // known to leak on Windows. If not, the test will fail.
          if (!std::ranges::any_of(
                  GetPathsAllowedToLeak(),
                  [&failed_to_delete =
                       failed_to_delete.value()](const auto& allowed_to_leak) {
                    return base::Contains(failed_to_delete, allowed_to_leak);
                  })) {
            ++num_non_ignored_files;
          }
#endif  // BUILDFLAG(IS_WIN)
          LOG(WARNING) << "failed to delete " << failed_to_delete;
        }
        EXPECT_EQ(num_non_ignored_files, 0)
            << "Failed to delete temporary directory for testing: "
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

FilePath RegisterPathDeleter(const FilePath& path) {
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

}  // namespace

bool EvictFileFromSystemCacheWithRetry(const FilePath& path) {
  const int kCycles = 10;
  const TimeDelta kDelay = TestTimeouts::action_timeout() / kCycles;
  for (int i = 0; i < kCycles; i++) {
    if (EvictFileFromSystemCache(path)) {
      return true;
    }
    PlatformThread::Sleep(kDelay);
  }
  return false;
}

FilePath GetTempDirForTesting() {
  FilePath path;
  CHECK(GetTempDir(&path));
  return path;
}

#if BUILDFLAG(IS_WIN)
std::vector<std::wstring>& GetPathsAllowedToLeak() {
  static base::NoDestructor<std::vector<std::wstring>> paths_allowed_to_leak;
  return *paths_allowed_to_leak;
}
#endif  // BUILDFLAG(IS_WIN)

FilePath CreateUniqueTempDirectoryScopedToTest() {
  ScopedAllowBlockingForTesting allow_blocking;
  FilePath path;
  if (!CreateNewTempDirectory(kDirPrefix, &path)) {
    ADD_FAILURE() << "Failed to create unique temporary directory for testing.";
    return FilePath();
  }
  return RegisterPathDeleter(path);
}

FilePath CreateUniqueTempDirectoryScopedToTestInDir(const base::FilePath& dir) {
  ScopedAllowBlockingForTesting allow_blocking;
  FilePath path;
  if (!CreateTemporaryDirInDir(dir, kDirPrefix, &path)) {
    ADD_FAILURE() << "Failed to create unique temporary directory for testing.";
    return FilePath();
  }
  return RegisterPathDeleter(path);
}

}  // namespace base
