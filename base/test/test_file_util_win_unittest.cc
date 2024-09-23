// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <windows.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/win/scoped_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class ScopedFileForTest {
 public:
  ScopedFileForTest(const FilePath& filename)
      : long_path_(L"\\\\?\\" + filename.value()) {
    win::ScopedHandle handle(::CreateFile(long_path_.c_str(), GENERIC_WRITE, 0,
                                          nullptr, CREATE_NEW,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));

    valid_ = handle.is_valid();
  }

  ScopedFileForTest(ScopedFileForTest&&) = delete;
  ScopedFileForTest& operator=(ScopedFileForTest&&) = delete;

  bool IsValid() const { return valid_; }

  ~ScopedFileForTest() {
    if (valid_)
      ::DeleteFile(long_path_.c_str());
  }

 private:
  FilePath::StringType long_path_;
  bool valid_;
};

}  // namespace

TEST(TestFileUtil, EvictNonExistingFile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  FilePath path = temp_dir.GetPath().Append(FilePath(L"non_existing"));

  ASSERT_FALSE(EvictFileFromSystemCache(path));
}

TEST(TestFileUtil, EvictFileWithShortName) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  FilePath temp_file = temp_dir.GetPath().Append(FilePath(L"file_for_evict"));
  ASSERT_TRUE(temp_file.value().length() < MAX_PATH);
  ScopedFileForTest file(temp_file);
  ASSERT_TRUE(file.IsValid());

  ASSERT_TRUE(EvictFileFromSystemCache(temp_file));
}

TEST(TestFileUtil, EvictFileWithLongName) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create subdirectory with long name.
  FilePath subdir =
      temp_dir.GetPath().Append(FilePath(std::wstring(100, L'a')));
  ASSERT_TRUE(subdir.value().length() < MAX_PATH);
  ASSERT_TRUE(CreateDirectory(subdir));

  // Create file with long name in subdirectory.
  FilePath temp_file = subdir.Append(FilePath(std::wstring(200, L'b')));
  ASSERT_TRUE(temp_file.value().length() > MAX_PATH);
  ScopedFileForTest file(temp_file);
  ASSERT_TRUE(file.IsValid());

  ASSERT_TRUE(EvictFileFromSystemCache(temp_file));
}

TEST(TestFileUtil, GetTempDirForTesting) {
  ASSERT_FALSE(GetTempDirForTesting().value().empty());
}

}  // namespace base
