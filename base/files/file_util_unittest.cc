// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/features.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "testing/platform_test.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(IS_WIN)
#include <tchar.h>
#include <windows.h>

#include <fileapi.h>
#include <shellapi.h>
#include <shlobj.h>

#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/file_path_reparse_point_win.h"
#include "base/test/gtest_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <sys/socket.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <linux/fs.h>
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/test/android/content_uri_test_utils.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "base/test/scoped_dev_zero_fuchsia.h"
#endif

// This macro helps avoid wrapped lines in the test structs.
#define FPL(x) FILE_PATH_LITERAL(x)

namespace base {

namespace {

const size_t kLargeFileSize = (1 << 16) + 3;

#if BUILDFLAG(IS_WIN)
// Method that wraps the win32 GetShortPathName API. Returns an empty path on
// error.
FilePath MakeShortFilePath(const FilePath& input) {
  DWORD path_short_len = ::GetShortPathName(input.value().c_str(), nullptr, 0);
  if (path_short_len == 0UL) {
    return FilePath();
  }

  std::wstring path_short_str;
  path_short_len = ::GetShortPathName(
      input.value().c_str(), WriteInto(&path_short_str, path_short_len),
      path_short_len);
  if (path_short_len == 0UL) {
    return FilePath();
  }

  return FilePath(path_short_str);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
// Provide a simple way to change the permissions bits on |path| in tests.
// ASSERT failures will return, but not stop the test.  Caller should wrap
// calls to this function in ASSERT_NO_FATAL_FAILURE().
void ChangePosixFilePermissions(const FilePath& path,
                                int mode_bits_to_set,
                                int mode_bits_to_clear) {
  ASSERT_FALSE(mode_bits_to_set & mode_bits_to_clear)
      << "Can't set and clear the same bits.";

  int mode = 0;
  ASSERT_TRUE(GetPosixFilePermissions(path, &mode));
  mode |= mode_bits_to_set;
  mode &= ~mode_bits_to_clear;
  ASSERT_TRUE(SetPosixFilePermissions(path, mode));
}
#endif  // BUILDFLAG(IS_MAC)

// Fuchsia doesn't support file permissions.
#if !BUILDFLAG(IS_FUCHSIA)
// Sets the source file to read-only.
void SetReadOnly(const FilePath& path, bool read_only) {
#if BUILDFLAG(IS_WIN)
  // On Windows, it involves setting/removing the 'readonly' bit.
  DWORD attrs = GetFileAttributes(path.value().c_str());
  ASSERT_NE(INVALID_FILE_ATTRIBUTES, attrs);
  ASSERT_TRUE(SetFileAttributes(
      path.value().c_str(), read_only ? (attrs | FILE_ATTRIBUTE_READONLY)
                                      : (attrs & ~FILE_ATTRIBUTE_READONLY)));

  DWORD expected =
      read_only
          ? ((attrs & (FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_DIRECTORY)) |
             FILE_ATTRIBUTE_READONLY)
          : (attrs & (FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_DIRECTORY));

  // Ignore FILE_ATTRIBUTE_NOT_CONTENT_INDEXED and FILE_ATTRIBUTE_COMPRESSED
  // if present. These flags are set by the operating system, depending on
  // local configurations, such as compressing the file system. Not filtering
  // out these flags could cause tests to fail even though they should pass.
  attrs = GetFileAttributes(path.value().c_str()) &
          ~(FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_COMPRESSED);
  ASSERT_EQ(expected, attrs);
#else
  // On all other platforms, it involves removing/setting the write bit.
  mode_t mode = read_only ? S_IRUSR : (S_IRUSR | S_IWUSR);
  EXPECT_TRUE(SetPosixFilePermissions(
      path, DirectoryExists(path) ? (mode | S_IXUSR) : mode));
#endif  // BUILDFLAG(IS_WIN)
}

bool IsReadOnly(const FilePath& path) {
#if BUILDFLAG(IS_WIN)
  DWORD attrs = GetFileAttributes(path.value().c_str());
  EXPECT_NE(INVALID_FILE_ATTRIBUTES, attrs);
  return attrs & FILE_ATTRIBUTE_READONLY;
#else
  int mode = 0;
  EXPECT_TRUE(GetPosixFilePermissions(path, &mode));
  return !(mode & S_IWUSR);
#endif  // BUILDFLAG(IS_WIN)
}

#endif  // BUILDFLAG(IS_FUCHSIA)

const wchar_t bogus_content[] = L"I'm cannon fodder.";

const int FILES_AND_DIRECTORIES =
    FileEnumerator::FILES | FileEnumerator::DIRECTORIES;

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest
class FileUtilTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

#if BUILDFLAG(IS_WIN)
  bool AreShortFilePathsEnabled() {
    static const bool enabled = [this] {
      // AreShortNamesEnabled is only available from Windows 11 onwards.
      using AreShortNamesEnabledFunction = decltype(&::AreShortNamesEnabled);
      AreShortNamesEnabledFunction short_names_func =
          reinterpret_cast<AreShortNamesEnabledFunction>(GetProcAddress(
              ::GetModuleHandleW(L"kernel32.dll"), "AreShortNamesEnabled"));

      if (!short_names_func) {
        // For non-Windows 11, it's highly likely that short name support is
        // present, but possible to be overridden by
        // HKLM\System\CurrentControlSet\Control\FileSystem\NtfsDisable8dot3NameCreation.
        // See
        // https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/fsutil-8dot3name.
        //
        // However, this test never checked this before and this has never
        // caused any issues in the past, so it's simpler to assume that short
        // names are always present if AreShortNamesEnabled API is not
        // available.
        return true;
      }
      base::File temp_dir(temp_dir_.GetPath(),
                          base::File::FLAG_OPEN_ALWAYS |
                              base::File::FLAG_WIN_BACKUP_SEMANTICS |
                              base::File::FLAG_READ);
      BOOL enabled = false;
      if (!short_names_func(temp_dir.GetPlatformFile(), &enabled)) {
        DPLOG(ERROR) << "Call to AreShortNamesEnabled failed.";
        // Assume short names are enabled (the default) if AreShortNamesEnabled
        // fails to return a value.
        return true;
      }
      return !!enabled;
    }();
    return enabled;
  }
#endif  // BUILDFLAG(IS_WIN)

  ScopedTempDir temp_dir_;
};

// Collects all the results from the given file enumerator, and provides an
// interface to query whether a given file is present.
class FindResultCollector {
 public:
  explicit FindResultCollector(FileEnumerator* enumerator) {
    FilePath cur_file;
    while (!(cur_file = enumerator->Next()).value().empty()) {
      FilePath::StringType path = cur_file.value();
      // The file should not be returned twice.
      EXPECT_TRUE(files_.end() == files_.find(path))
          << "Same file returned twice";

      // Save for later.
      files_.insert(path);
    }
  }

  // Returns true if the enumerator found the file.
  bool HasFile(const FilePath& file) const {
    return files_.find(file.value()) != files_.end();
  }

  int size() { return static_cast<int>(files_.size()); }

 private:
  std::set<FilePath::StringType> files_;
};

// Simple function to dump some text into a new file.
void CreateTextFile(const FilePath& filename, const std::wstring& contents) {
  std::wofstream file;
#if BUILDFLAG(IS_WIN)
  file.open(filename.value().c_str());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  file.open(filename.value());
#endif  // BUILDFLAG(IS_WIN)
  ASSERT_TRUE(file.is_open());
  file << contents;
  file.close();
}

// Simple function to take out some text from a file.
std::wstring ReadTextFile(const FilePath& filename) {
  wchar_t contents[64];
  std::wifstream file;
#if BUILDFLAG(IS_WIN)
  file.open(filename.value().c_str());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  file.open(filename.value());
#endif  // BUILDFLAG(IS_WIN)
  EXPECT_TRUE(file.is_open());
  file.getline(contents, std::size(contents));
  file.close();
  return std::wstring(contents);
}

// Sets |is_inheritable| to indicate whether or not |stream| is set up to be
// inerhited into child processes (i.e., HANDLE_FLAG_INHERIT is set on the
// underlying handle on Windows, or FD_CLOEXEC is not set on the underlying file
// descriptor on POSIX). Calls to this function must be wrapped with
// ASSERT_NO_FATAL_FAILURE to properly abort tests in case of fatal failure.
void GetIsInheritable(FILE* stream, bool* is_inheritable) {
#if BUILDFLAG(IS_WIN)
  HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stream)));
  ASSERT_NE(INVALID_HANDLE_VALUE, handle);

  DWORD info = 0;
  ASSERT_EQ(TRUE, ::GetHandleInformation(handle, &info));
  *is_inheritable = ((info & HANDLE_FLAG_INHERIT) != 0);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  int fd = fileno(stream);
  ASSERT_NE(-1, fd);
  int flags = fcntl(fd, F_GETFD, 0);
  ASSERT_NE(-1, flags);
  *is_inheritable = ((flags & FD_CLOEXEC) == 0);
#else
#error Not implemented
#endif
}

#if BUILDFLAG(IS_POSIX)
class ScopedWorkingDirectory {
 public:
  explicit ScopedWorkingDirectory(const FilePath& new_working_dir) {
    CHECK(base::GetCurrentDirectory(&original_working_directory_));
    CHECK(base::SetCurrentDirectory(new_working_dir));
  }

  ~ScopedWorkingDirectory() {
    CHECK(base::SetCurrentDirectory(original_working_directory_));
  }

 private:
  base::FilePath original_working_directory_;
};

TEST_F(FileUtilTest, MakeAbsoluteFilePathNoResolveSymbolicLinks) {
  FilePath cwd;
  ASSERT_TRUE(GetCurrentDirectory(&cwd));
  const std::pair<FilePath, std::optional<FilePath>> kExpectedResults[]{
      {FilePath(), std::nullopt},
      {FilePath("."), cwd},
      {FilePath(".."), cwd.DirName()},
      {FilePath("a/.."), cwd},
      {FilePath("a/b/.."), cwd.Append(FPL("a"))},
      {FilePath("/tmp/../.."), FilePath("/")},
      {FilePath("/tmp/../"), FilePath("/")},
      {FilePath("/tmp/a/b/../c/../.."), FilePath("/tmp")},
      {FilePath("/././tmp/./a/./b/../c/./../.."), FilePath("/tmp")},
      {FilePath("/.././../tmp"), FilePath("/tmp")},
      {FilePath("/..///.////..////tmp"), FilePath("/tmp")},
      {FilePath("//..///.////..////tmp"), FilePath("//tmp")},
      {FilePath("///..///.////..////tmp"), FilePath("/tmp")},
  };

  for (auto& expected_result : kExpectedResults) {
    EXPECT_EQ(MakeAbsoluteFilePathNoResolveSymbolicLinks(expected_result.first),
              expected_result.second);
  }

  // Test that MakeAbsoluteFilePathNoResolveSymbolicLinks() returns an empty
  // path if GetCurrentDirectory() fails.
  const FilePath temp_dir_path = temp_dir_.GetPath();
  ScopedWorkingDirectory scoped_cwd(temp_dir_path);
  // Delete the cwd so that GetCurrentDirectory() fails.
  ASSERT_TRUE(temp_dir_.Delete());
  ASSERT_FALSE(
      MakeAbsoluteFilePathNoResolveSymbolicLinks(FilePath("relative_file_path"))
          .has_value());
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(FileUtilTest, FileAndDirectorySize) {
  // Create three files of 20, 30 and 3 chars (utf8). ComputeDirectorySize
  // should return 53 bytes.
  FilePath file_01 = temp_dir_.GetPath().Append(FPL("The file 01.txt"));
  CreateTextFile(file_01, L"12345678901234567890");

  std::optional<int64_t> size_f1 = GetFileSize(file_01);
  ASSERT_THAT(size_f1, testing::Optional(20));
  int64_t size_f1_out = 0;
  ASSERT_TRUE(GetFileSize(file_01, &size_f1_out));
  EXPECT_EQ(size_f1.value(), size_f1_out);

  FilePath subdir_path = temp_dir_.GetPath().Append(FPL("Level2"));
  CreateDirectory(subdir_path);

  FilePath file_02 = subdir_path.Append(FPL("The file 02.txt"));
  CreateTextFile(file_02, L"123456789012345678901234567890");
  std::optional<int64_t> size_f2 = GetFileSize(file_02);
  ASSERT_THAT(size_f2, testing::Optional(30));
  int64_t size_f2_out = 0;
  ASSERT_TRUE(GetFileSize(file_02, &size_f2_out));
  EXPECT_EQ(size_f2.value(), size_f2_out);

  FilePath subsubdir_path = subdir_path.Append(FPL("Level3"));
  CreateDirectory(subsubdir_path);

  FilePath file_03 = subsubdir_path.Append(FPL("The file 03.txt"));
  CreateTextFile(file_03, L"123");

  int64_t computed_size = ComputeDirectorySize(temp_dir_.GetPath());
  EXPECT_EQ(size_f1.value() + size_f2.value() + 3, computed_size);
}

TEST_F(FileUtilTest, NormalizeFilePathBasic) {
  // Create a directory under the test dir.  Because we create it,
  // we know it is not a link.
  FilePath file_a_path = temp_dir_.GetPath().Append(FPL("file_a"));
  FilePath dir_path = temp_dir_.GetPath().Append(FPL("dir"));
  FilePath file_b_path = dir_path.Append(FPL("file_b"));
  CreateDirectory(dir_path);

  FilePath normalized_file_a_path, normalized_file_b_path;
  ASSERT_FALSE(PathExists(file_a_path));
  ASSERT_FALSE(NormalizeFilePath(file_a_path, &normalized_file_a_path))
      << "NormalizeFilePath() should fail on nonexistent paths.";

  CreateTextFile(file_a_path, bogus_content);
  ASSERT_TRUE(PathExists(file_a_path));
  ASSERT_TRUE(NormalizeFilePath(file_a_path, &normalized_file_a_path));

  CreateTextFile(file_b_path, bogus_content);
  ASSERT_TRUE(PathExists(file_b_path));
  ASSERT_TRUE(NormalizeFilePath(file_b_path, &normalized_file_b_path));

  // Because this test created |dir_path|, we know it is not a link
  // or junction.  So, the real path of the directory holding file a
  // must be the parent of the path holding file b.
  ASSERT_TRUE(normalized_file_a_path.DirName().IsParent(
      normalized_file_b_path.DirName()));
}

#if BUILDFLAG(IS_WIN)

TEST_F(FileUtilTest, NormalizeFileEmptyFile) {
  // Create a directory under the test dir.  Because we create it,
  // we know it is not a link.
  const wchar_t empty_content[] = L"";

  FilePath file_a_path = temp_dir_.GetPath().Append(FPL("file_empty_a"));
  FilePath dir_path = temp_dir_.GetPath().Append(FPL("dir"));
  FilePath file_b_path = dir_path.Append(FPL("file_empty_b"));
  ASSERT_TRUE(CreateDirectory(dir_path));

  FilePath normalized_file_a_path, normalized_file_b_path;
  ASSERT_FALSE(PathExists(file_a_path));
  EXPECT_FALSE(NormalizeFilePath(file_a_path, &normalized_file_a_path))
      << "NormalizeFilePath() should fail on nonexistent paths.";

  CreateTextFile(file_a_path, empty_content);
  ASSERT_TRUE(PathExists(file_a_path));
  EXPECT_TRUE(NormalizeFilePath(file_a_path, &normalized_file_a_path));

  CreateTextFile(file_b_path, empty_content);
  ASSERT_TRUE(PathExists(file_b_path));
  EXPECT_TRUE(NormalizeFilePath(file_b_path, &normalized_file_b_path));

  // Because this test created |dir_path|, we know it is not a link
  // or junction.  So, the real path of the directory holding file a
  // must be the parent of the path holding file b.
  EXPECT_TRUE(normalized_file_a_path.DirName().IsParent(
      normalized_file_b_path.DirName()));
}

TEST_F(FileUtilTest, NormalizeFilePathReparsePoints) {
  // Build the following directory structure:
  //
  // temp_dir
  // |-> base_a
  // |   |-> sub_a
  // |       |-> file.txt
  // |       |-> long_name___... (Very long name.)
  // |           |-> sub_long
  // |              |-> deep.txt
  // |-> base_b
  //     |-> to_sub_a (reparse point to temp_dir\base_a\sub_a)
  //     |-> to_base_b (reparse point to temp_dir\base_b)
  //     |-> to_sub_long (reparse point to temp_dir\sub_a\long_name_\sub_long)

  FilePath base_a = temp_dir_.GetPath().Append(FPL("base_a"));
  // TEMP can have a lower case drive letter.
  std::wstring temp_base_a = base_a.value();
  ASSERT_FALSE(temp_base_a.empty());
  temp_base_a[0] = ToUpperASCII(char16_t{temp_base_a[0]});
  base_a = FilePath(temp_base_a);

  ASSERT_TRUE(CreateDirectory(base_a));
  // TEMP might be a short name which is not normalized.
  base_a = MakeLongFilePath(base_a);

  FilePath sub_a = base_a.Append(FPL("sub_a"));
  ASSERT_TRUE(CreateDirectory(sub_a));

  FilePath file_txt = sub_a.Append(FPL("file.txt"));
  CreateTextFile(file_txt, bogus_content);

  // Want a directory whose name is long enough to make the path to the file
  // inside just under MAX_PATH chars.  This will be used to test that when
  // a junction expands to a path over MAX_PATH chars in length,
  // NormalizeFilePath() fails without crashing.
  FilePath sub_long_rel(FPL("sub_long"));
  FilePath deep_txt(FPL("deepfile.txt"));

  int target_length = MAX_PATH - 1;  // One for the string terminator.
  target_length -= (sub_a.value().length() + 1);  // +1 for the separator '\'.
  target_length -= (sub_long_rel.Append(deep_txt).value().length() + 1);
  FilePath::StringType long_name_str = FPL("long_name_");
  long_name_str.resize(target_length, '_');

  FilePath long_name = sub_a.Append(FilePath(long_name_str));
  FilePath deep_file = long_name.Append(sub_long_rel).Append(deep_txt);
  ASSERT_EQ(static_cast<size_t>(MAX_PATH - 1), deep_file.value().length());

  FilePath sub_long = deep_file.DirName();
  ASSERT_TRUE(CreateDirectory(sub_long));
  CreateTextFile(deep_file, bogus_content);

  FilePath base_b = temp_dir_.GetPath().Append(FPL("base_b"));
  ASSERT_TRUE(CreateDirectory(base_b));
  // TEMP might be a short name which is not normalized.
  base_b = MakeLongFilePath(base_b);

  FilePath to_sub_a = base_b.Append(FPL("to_sub_a"));
  ASSERT_TRUE(CreateDirectory(to_sub_a));
  FilePath normalized_path;
  {
    auto reparse_to_sub_a = test::FilePathReparsePoint::Create(to_sub_a, sub_a);
    ASSERT_TRUE(reparse_to_sub_a.has_value());

    FilePath to_base_b = base_b.Append(FPL("to_base_b"));
    ASSERT_TRUE(CreateDirectory(to_base_b));
    auto reparse_to_base_b =
        test::FilePathReparsePoint::Create(to_base_b, base_b);
    ASSERT_TRUE(reparse_to_base_b.has_value());

    FilePath to_sub_long = base_b.Append(FPL("to_sub_long"));
    ASSERT_TRUE(CreateDirectory(to_sub_long));
    auto reparse_to_sub_long =
        test::FilePathReparsePoint::Create(to_sub_long, sub_long);
    ASSERT_TRUE(reparse_to_sub_long.has_value());

    // Normalize a junction free path: base_a\sub_a\file.txt .
    ASSERT_TRUE(NormalizeFilePath(file_txt, &normalized_path));
    ASSERT_EQ(file_txt.value(), normalized_path.value());

    // Check that the path base_b\to_sub_a\file.txt can be normalized to exclude
    // the junction to_sub_a.
    ASSERT_TRUE(
        NormalizeFilePath(to_sub_a.Append(FPL("file.txt")), &normalized_path));
    ASSERT_EQ(file_txt.value(), normalized_path.value());

    // Check that the path base_b\to_base_b\to_base_b\to_sub_a\file.txt can be
    // normalized to exclude junctions to_base_b and to_sub_a .
    ASSERT_TRUE(NormalizeFilePath(base_b.Append(FPL("to_base_b"))
                                      .Append(FPL("to_base_b"))
                                      .Append(FPL("to_sub_a"))
                                      .Append(FPL("file.txt")),
                                  &normalized_path));
    ASSERT_EQ(file_txt.value(), normalized_path.value());

    // A long enough path will cause NormalizeFilePath() to fail.  Make a long
    // path using to_base_b many times, and check that paths long enough to fail
    // do not cause a crash.
    FilePath long_path = base_b;
    const int kLengthLimit = MAX_PATH + 40;
    while (long_path.value().length() <= kLengthLimit) {
      long_path = long_path.Append(FPL("to_base_b"));
    }
    long_path = long_path.Append(FPL("to_sub_a")).Append(FPL("file.txt"));

    ASSERT_FALSE(NormalizeFilePath(long_path, &normalized_path));

    // Normalizing the junction to deep.txt should pass, because the expanded
    // path to deep.txt is not longer than `MAX_PATH`.
    ASSERT_TRUE(
        NormalizeFilePath(to_sub_long.Append(deep_txt), &normalized_path));
    ASSERT_EQ(normalized_path, deep_file);

    // Delete the reparse points, and see that NormalizeFilePath() fails
    // to traverse them.
  }

  ASSERT_FALSE(
      NormalizeFilePath(to_sub_a.Append(FPL("file.txt")), &normalized_path));
}

TEST_F(FileUtilTest, NormalizeFilePathWithLongPath) {
  // Indicates that the OS should bypass the normal path length limit.
  const FilePath::StringType kPathPrefix(FPL("\\\\?\\"));

  constexpr int kLengthLimit = MAX_PATH + 40;
  FilePath long_path = temp_dir_.GetPath();
  while (long_path.value().length() <= kLengthLimit) {
    long_path = long_path.Append(FPL("to_base_b"));
    const auto path_with_no_check = kPathPrefix + long_path.value();
    ASSERT_TRUE(::CreateDirectoryW(path_with_no_check.c_str(), nullptr));
  }

  auto path_with_no_check = kPathPrefix + long_path.value();
  long_path = FilePath(path_with_no_check);

  // The normalization should fail because the path is too long.
  FilePath normalized_path;
  ASSERT_FALSE(NormalizeFilePath(long_path, &normalized_path));
}

TEST_F(FileUtilTest, DevicePathToDriveLetter) {
  // Get a drive letter.
  std::wstring real_drive_letter = AsWString(
      ToUpperASCII(AsStringPiece16(temp_dir_.GetPath().value().substr(0, 2))));
  if (!IsAsciiAlpha(real_drive_letter[0]) || ':' != real_drive_letter[1]) {
    LOG(ERROR) << "Can't get a drive letter to test with.";
    return;
  }

  // Get the NT style path to that drive.
  wchar_t device_path[MAX_PATH] = {'\0'};
  ASSERT_TRUE(
      ::QueryDosDevice(real_drive_letter.c_str(), device_path, MAX_PATH));
  FilePath actual_device_path(device_path);
  FilePath win32_path;

  // Run DevicePathToDriveLetterPath() on the NT style path we got from
  // QueryDosDevice().  Expect the drive letter we started with.
  ASSERT_TRUE(DevicePathToDriveLetterPath(actual_device_path, &win32_path));
  ASSERT_EQ(real_drive_letter, win32_path.value());

  // Add some directories to the path.  Expect those extra path componenets
  // to be preserved.
  FilePath kRelativePath(FPL("dir1\\dir2\\file.txt"));
  ASSERT_TRUE(DevicePathToDriveLetterPath(
      actual_device_path.Append(kRelativePath), &win32_path));
  EXPECT_EQ(FilePath(real_drive_letter + FILE_PATH_LITERAL("\\"))
                .Append(kRelativePath)
                .value(),
            win32_path.value());

  // Deform the real path so that it is invalid by removing the last four
  // characters.  The way windows names devices that are hard disks
  // (\Device\HardDiskVolume${NUMBER}) guarantees that the string is longer
  // than three characters.  The only way the truncated string could be a
  // real drive is if more than 10^3 disks are mounted:
  // \Device\HardDiskVolume10000 would be truncated to \Device\HardDiskVolume1
  // Check that DevicePathToDriveLetterPath fails.
  size_t path_length = actual_device_path.value().length();
  size_t new_length = path_length - 4;
  ASSERT_GT(new_length, 0u);
  FilePath prefix_of_real_device_path(
      actual_device_path.value().substr(0, new_length));
  ASSERT_FALSE(
      DevicePathToDriveLetterPath(prefix_of_real_device_path, &win32_path));

  ASSERT_FALSE(DevicePathToDriveLetterPath(
      prefix_of_real_device_path.Append(kRelativePath), &win32_path));

  // Deform the real path so that it is invalid by adding some characters. For
  // example, if C: maps to \Device\HardDiskVolume8, then we simulate a
  // request for the drive letter whose native path is
  // \Device\HardDiskVolume812345 .  We assume such a device does not exist,
  // because drives are numbered in order and mounting 112345 hard disks will
  // never happen.
  const FilePath::StringType kExtraChars = FPL("12345");

  FilePath real_device_path_plus_numbers(actual_device_path.value() +
                                         kExtraChars);

  ASSERT_FALSE(
      DevicePathToDriveLetterPath(real_device_path_plus_numbers, &win32_path));

  ASSERT_FALSE(DevicePathToDriveLetterPath(
      real_device_path_plus_numbers.Append(kRelativePath), &win32_path));
}

TEST_F(FileUtilTest, AreShortFilePathsEnabled) {
  constexpr FilePath::CharType kLongDirName[] = FPL("A long path");
  FilePath long_test_dir = temp_dir_.GetPath().Append(kLongDirName);
  ASSERT_TRUE(CreateDirectory(long_test_dir));

  FilePath short_test_dir = MakeShortFilePath(long_test_dir);

  // MakeShortFilePath returns the long file path if short paths are not
  // supported. See
  // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getshortpathnamew.
  ASSERT_EQ(AreShortFilePathsEnabled(), short_test_dir != long_test_dir);
}

TEST_F(FileUtilTest, CreateTemporaryFileInDirLongPathTest) {
  if (!AreShortFilePathsEnabled()) {
    GTEST_SKIP() << "Short filepaths are not supported on this system.";
  }
  // Test that CreateTemporaryFileInDir() creates a path and returns a long path
  // if it is available. This test requires that:
  // - the filesystem at |temp_dir_| supports long filenames.
  // - the account has FILE_LIST_DIRECTORY permission for all ancestor
  //   directories of |temp_dir_|.
  constexpr FilePath::CharType kLongDirName[] = FPL("A long path");
  constexpr FilePath::CharType kTestSubDirName[] = FPL("test");
  FilePath long_test_dir = temp_dir_.GetPath().Append(kLongDirName);
  ASSERT_TRUE(CreateDirectory(long_test_dir));

  // kLongDirName is not a 8.3 component. So ::GetShortPathName() should give us
  // a different short name.
  FilePath short_test_dir = MakeShortFilePath(long_test_dir);
  ASSERT_FALSE(short_test_dir.empty());
  ASSERT_NE(kLongDirName, short_test_dir.BaseName().value());

  FilePath temp_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(short_test_dir, &temp_file));
  EXPECT_EQ(kLongDirName, temp_file.DirName().BaseName().value());
  EXPECT_TRUE(PathExists(temp_file));

  // Create a subdirectory of |long_test_dir| and make |long_test_dir|
  // unreadable. We should still be able to create a temp file in the
  // subdirectory, but we won't be able to determine the long path for it. This
  // mimics the environment that some users run where their user profiles reside
  // in a location where the don't have full access to the higher level
  // directories. (Note that this assumption is true for NTFS, but not for some
  // network file systems. E.g. AFS).
  FilePath access_test_dir = long_test_dir.Append(kTestSubDirName);
  ASSERT_TRUE(CreateDirectory(access_test_dir));
  FilePermissionRestorer long_test_dir_restorer(long_test_dir);
  ASSERT_TRUE(MakeFileUnreadable(long_test_dir));

  // Use the short form of the directory to create a temporary filename.
  ASSERT_TRUE(CreateTemporaryFileInDir(short_test_dir.Append(kTestSubDirName),
                                       &temp_file));
  EXPECT_TRUE(PathExists(temp_file));
  EXPECT_TRUE(short_test_dir.IsParent(temp_file.DirName()));

  // Check that the long path can't be determined for |temp_file|.
  // Helper method base::MakeLongFilePath returns an empty path on error.
  FilePath temp_file_long = MakeLongFilePath(temp_file);
  ASSERT_TRUE(temp_file_long.empty());
}

TEST_F(FileUtilTest, MakeLongFilePathTest) {
  if (!AreShortFilePathsEnabled()) {
    GTEST_SKIP() << "Short filepaths are not supported on this system.";
  }
  // Tests helper function base::MakeLongFilePath

  // If a username isn't a valid 8.3 short file name (even just a
  // lengthy name like "user with long name"), Windows will set the TMP and TEMP
  // environment variables to be 8.3 paths. ::GetTempPath (called in
  // base::GetTempDir) just uses the value specified by TMP or TEMP, and so can
  // return a short path. So from the start need to use MakeLongFilePath
  // to normalize the path for such test environments.
  FilePath temp_dir_long = MakeLongFilePath(temp_dir_.GetPath());
  ASSERT_FALSE(temp_dir_long.empty());

  FilePath long_test_dir = temp_dir_long.Append(FPL("A long directory name"));
  ASSERT_TRUE(CreateDirectory(long_test_dir));

  // Directory name is not a 8.3 component. So ::GetShortPathName() should give
  // us a different short name.
  FilePath short_test_dir = MakeShortFilePath(long_test_dir);
  ASSERT_FALSE(short_test_dir.empty());

  EXPECT_NE(long_test_dir, short_test_dir);
  EXPECT_EQ(long_test_dir, MakeLongFilePath(short_test_dir));

  FilePath long_test_file = long_test_dir.Append(FPL("A long file name.1234"));
  CreateTextFile(long_test_file, bogus_content);
  ASSERT_TRUE(PathExists(long_test_file));

  // File name is not a 8.3 component. So ::GetShortPathName() should give us
  // a different short name.
  FilePath short_test_file = MakeShortFilePath(long_test_file);
  ASSERT_FALSE(short_test_file.empty());

  EXPECT_NE(long_test_file, short_test_file);
  EXPECT_EQ(long_test_file, MakeLongFilePath(short_test_file));

  // MakeLongFilePath should return empty path if file does not exist.
  EXPECT_TRUE(DeleteFile(short_test_file));
  EXPECT_TRUE(MakeLongFilePath(short_test_file).empty());

  // MakeLongFilePath should return empty path if directory does not exist.
  EXPECT_TRUE(DeleteFile(short_test_dir));
  EXPECT_TRUE(MakeLongFilePath(short_test_dir).empty());
}

TEST_F(FileUtilTest, CreateWinHardlinkTest) {
  // Link to a different file name in a sub-directory of |temp_dir_|.
  FilePath test_dir = temp_dir_.GetPath().Append(FPL("test"));
  ASSERT_TRUE(CreateDirectory(test_dir));
  FilePath temp_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file));
  FilePath link_to_file = test_dir.Append(FPL("linked_name"));
  EXPECT_TRUE(CreateWinHardLink(link_to_file, temp_file));
  EXPECT_TRUE(PathExists(link_to_file));

  // Link two directories. This should fail. Verify that failure is returned
  // by CreateWinHardLink.
  EXPECT_FALSE(CreateWinHardLink(temp_dir_.GetPath(), test_dir));
}

TEST_F(FileUtilTest, PreventExecuteMappingNewFile) {
  base::test::ScopedFeatureList enforcement_feature;
  enforcement_feature.InitAndEnableFeature(
      features::kEnforceNoExecutableFileHandles);
  FilePath file = temp_dir_.GetPath().Append(FPL("afile.txt"));

  ASSERT_FALSE(PathExists(file));
  {
    File new_file(file, File::FLAG_WRITE | File::FLAG_WIN_NO_EXECUTE |
                            File::FLAG_CREATE_ALWAYS);
    ASSERT_TRUE(new_file.IsValid());
  }

  {
    File open_file(file, File::FLAG_READ | File::FLAG_WIN_EXECUTE |
                             File::FLAG_OPEN_ALWAYS);
    EXPECT_FALSE(open_file.IsValid());
  }
  // Verify the deny ACL did not prevent deleting the file.
  EXPECT_TRUE(DeleteFile(file));
}

TEST_F(FileUtilTest, PreventExecuteMappingExisting) {
  base::test::ScopedFeatureList enforcement_feature;
  enforcement_feature.InitAndEnableFeature(
      features::kEnforceNoExecutableFileHandles);
  FilePath file = temp_dir_.GetPath().Append(FPL("afile.txt"));
  CreateTextFile(file, bogus_content);
  ASSERT_TRUE(PathExists(file));
  {
    File open_file(file, File::FLAG_READ | File::FLAG_WIN_EXECUTE |
                             File::FLAG_OPEN_ALWAYS);
    EXPECT_TRUE(open_file.IsValid());
  }
  EXPECT_TRUE(PreventExecuteMapping(file));
  {
    File open_file(file, File::FLAG_READ | File::FLAG_WIN_EXECUTE |
                             File::FLAG_OPEN_ALWAYS);
    EXPECT_FALSE(open_file.IsValid());
  }
  // Verify the deny ACL did not prevent deleting the file.
  EXPECT_TRUE(DeleteFile(file));
}

TEST_F(FileUtilTest, PreventExecuteMappingOpenFile) {
  base::test::ScopedFeatureList enforcement_feature;
  enforcement_feature.InitAndEnableFeature(
      features::kEnforceNoExecutableFileHandles);
  FilePath file = temp_dir_.GetPath().Append(FPL("afile.txt"));
  CreateTextFile(file, bogus_content);
  ASSERT_TRUE(PathExists(file));
  File open_file(file, File::FLAG_READ | File::FLAG_WRITE |
                           File::FLAG_WIN_EXECUTE | File::FLAG_OPEN_ALWAYS);
  EXPECT_TRUE(open_file.IsValid());
  // Verify ACE can be set even on an open file.
  EXPECT_TRUE(PreventExecuteMapping(file));
  {
    File second_open_file(
        file, File::FLAG_READ | File::FLAG_WRITE | File::FLAG_OPEN_ALWAYS);
    EXPECT_TRUE(second_open_file.IsValid());
  }
  {
    File third_open_file(file, File::FLAG_READ | File::FLAG_WIN_EXECUTE |
                                   File::FLAG_OPEN_ALWAYS);
    EXPECT_FALSE(third_open_file.IsValid());
  }

  open_file.Close();
  // Verify the deny ACL did not prevent deleting the file.
  EXPECT_TRUE(DeleteFile(file));
}

TEST(FileUtilDeathTest, DisallowNoExecuteOnUnsafeFile) {
  base::test::ScopedFeatureList enforcement_feature;
  enforcement_feature.InitAndEnableFeature(
      features::kEnforceNoExecutableFileHandles);
  base::FilePath local_app_data;
  // This test places a file in %LOCALAPPDATA% to verify that the checks in
  // IsPathSafeToSetAclOn work correctly.
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_LOCAL_APP_DATA, &local_app_data));

  base::FilePath file_path;
  EXPECT_DCHECK_DEATH_WITH(
      {
        {
          base::File temp_file =
              base::CreateAndOpenTemporaryFileInDir(local_app_data, &file_path);
        }
        File reopen_file(file_path, File::FLAG_READ | File::FLAG_WRITE |
                                        File::FLAG_WIN_NO_EXECUTE |
                                        File::FLAG_OPEN_ALWAYS |
                                        File::FLAG_DELETE_ON_CLOSE);
      },
      "Unsafe to deny execute access to path");
}

MULTIPROCESS_TEST_MAIN(NoExecuteOnSafeFileMain) {
  base::FilePath temp_file;
  CHECK(base::CreateTemporaryFile(&temp_file));

  // A file with FLAG_WIN_NO_EXECUTE created in temp dir should always be
  // permitted.
  File reopen_file(temp_file, File::FLAG_READ | File::FLAG_WRITE |
                                  File::FLAG_WIN_NO_EXECUTE |
                                  File::FLAG_OPEN_ALWAYS |
                                  File::FLAG_DELETE_ON_CLOSE);
  return 0;
}

TEST_F(FileUtilTest, NoExecuteOnSafeFile) {
  FilePath new_dir;
  ASSERT_TRUE(CreateTemporaryDirInDir(
      temp_dir_.GetPath(), FILE_PATH_LITERAL("NoExecuteOnSafeFileLongPath"),
      &new_dir));

  FilePath short_dir = base::MakeShortFilePath(new_dir);

  LaunchOptions options;
  options.environment[L"TMP"] = short_dir.value();

  CommandLine child_command_line(GetMultiProcessTestChildBaseCommandLine());

  Process child_process = SpawnMultiProcessTestChild(
      "NoExecuteOnSafeFileMain", child_command_line, options);
  ASSERT_TRUE(child_process.IsValid());
  int rv = -1;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &rv));
  ASSERT_EQ(0, rv);
}

class FileUtilExecuteEnforcementTest
    : public FileUtilTest,
      public ::testing::WithParamInterface<bool> {
 public:
  FileUtilExecuteEnforcementTest() {
    if (IsEnforcementEnabled()) {
      enforcement_feature_.InitAndEnableFeature(
          features::kEnforceNoExecutableFileHandles);
    } else {
      enforcement_feature_.InitAndDisableFeature(
          features::kEnforceNoExecutableFileHandles);
    }
  }

 protected:
  bool IsEnforcementEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList enforcement_feature_;
};

// This test verifies that if a file has been passed to `PreventExecuteMapping`
// and enforcement is enabled, then it cannot be mapped as executable into
// memory.
TEST_P(FileUtilExecuteEnforcementTest, Functional) {
  FilePath dir_exe;
  EXPECT_TRUE(PathService::Get(DIR_EXE, &dir_exe));
  // This DLL is built as part of base_unittests so is guaranteed to be present.
  FilePath test_dll(dir_exe.Append(FPL("scoped_handle_test_dll.dll")));

  EXPECT_TRUE(base::PathExists(test_dll));

  FilePath dll_copy_path = temp_dir_.GetPath().Append(FPL("test.dll"));

  ASSERT_TRUE(CopyFile(test_dll, dll_copy_path));
  ASSERT_TRUE(PreventExecuteMapping(dll_copy_path));
  ScopedNativeLibrary module(dll_copy_path);

  // If enforcement is enabled, then `PreventExecuteMapping` will have prevented
  // the load, and the module will be invalid.
  EXPECT_EQ(IsEnforcementEnabled(), !module.is_valid());
}

INSTANTIATE_TEST_SUITE_P(EnforcementEnabled,
                         FileUtilExecuteEnforcementTest,
                         ::testing::Values(true));
INSTANTIATE_TEST_SUITE_P(EnforcementDisabled,
                         FileUtilExecuteEnforcementTest,
                         ::testing::Values(false));

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX)

TEST_F(FileUtilTest, CreateAndReadSymlinks) {
  FilePath link_from = temp_dir_.GetPath().Append(FPL("from_file"));
  FilePath link_to = temp_dir_.GetPath().Append(FPL("to_file"));
  CreateTextFile(link_to, bogus_content);

  ASSERT_TRUE(CreateSymbolicLink(link_to, link_from))
      << "Failed to create file symlink.";

  // If we created the link properly, we should be able to read the contents
  // through it.
  EXPECT_EQ(bogus_content, ReadTextFile(link_from));

  FilePath result;
  ASSERT_TRUE(ReadSymbolicLink(link_from, &result));
  EXPECT_EQ(link_to.value(), result.value());

  // Link to a directory.
  link_from = temp_dir_.GetPath().Append(FPL("from_dir"));
  link_to = temp_dir_.GetPath().Append(FPL("to_dir"));
  ASSERT_TRUE(CreateDirectory(link_to));
  ASSERT_TRUE(CreateSymbolicLink(link_to, link_from))
      << "Failed to create directory symlink.";

  // Test failures.
  EXPECT_FALSE(CreateSymbolicLink(link_to, link_to));
  EXPECT_FALSE(ReadSymbolicLink(link_to, &result));
  FilePath missing = temp_dir_.GetPath().Append(FPL("missing"));
  EXPECT_FALSE(ReadSymbolicLink(missing, &result));
}

TEST_F(FileUtilTest, CreateAndReadRelativeSymlinks) {
  FilePath link_from = temp_dir_.GetPath().Append(FPL("from_file"));
  FilePath filename_link_to("to_file");
  FilePath link_to = temp_dir_.GetPath().Append(filename_link_to);
  FilePath link_from_in_subdir =
      temp_dir_.GetPath().Append(FPL("subdir")).Append(FPL("from_file"));
  FilePath link_to_in_subdir = FilePath(FPL("..")).Append(filename_link_to);
  CreateTextFile(link_to, bogus_content);

  ASSERT_TRUE(CreateDirectory(link_from_in_subdir.DirName()));
  ASSERT_TRUE(CreateSymbolicLink(link_to_in_subdir, link_from_in_subdir));

  ASSERT_TRUE(CreateSymbolicLink(filename_link_to, link_from))
      << "Failed to create file symlink.";

  // If we created the link properly, we should be able to read the contents
  // through it.
  EXPECT_EQ(bogus_content, ReadTextFile(link_from));
  EXPECT_EQ(bogus_content, ReadTextFile(link_from_in_subdir));

  FilePath result;
  ASSERT_TRUE(ReadSymbolicLink(link_from, &result));
  EXPECT_EQ(filename_link_to.value(), result.value());

  std::optional<FilePath> absolute_link = ReadSymbolicLinkAbsolute(link_from);
  ASSERT_TRUE(absolute_link);
  EXPECT_EQ(link_to.value(), absolute_link->value());

  absolute_link = ReadSymbolicLinkAbsolute(link_from_in_subdir);
  ASSERT_TRUE(absolute_link);
  EXPECT_EQ(link_to.value(), absolute_link->value());

  // Link to a directory.
  link_from = temp_dir_.GetPath().Append(FPL("from_dir"));
  filename_link_to = FilePath("to_dir");
  link_to = temp_dir_.GetPath().Append(filename_link_to);
  ASSERT_TRUE(CreateDirectory(link_to));
  ASSERT_TRUE(CreateSymbolicLink(filename_link_to, link_from))
      << "Failed to create relative directory symlink.";

  ASSERT_TRUE(ReadSymbolicLink(link_from, &result));
  EXPECT_EQ(filename_link_to.value(), result.value());

  absolute_link = ReadSymbolicLinkAbsolute(link_from);
  ASSERT_TRUE(absolute_link);
  EXPECT_EQ(link_to.value(), absolute_link->value());

  // Test failures.
  EXPECT_FALSE(CreateSymbolicLink(link_to, link_to));
  EXPECT_FALSE(ReadSymbolicLink(link_to, &result));
}

// The following test of NormalizeFilePath() require that we create a symlink.
// This can not be done on Windows before Vista.  On Vista, creating a symlink
// requires privilege "SeCreateSymbolicLinkPrivilege".
// TODO(skerner): Investigate the possibility of giving base_unittests the
// privileges required to create a symlink.
TEST_F(FileUtilTest, NormalizeFilePathSymlinks) {
  // Link one file to another.
  FilePath link_from = temp_dir_.GetPath().Append(FPL("from_file"));
  FilePath link_to = temp_dir_.GetPath().Append(FPL("to_file"));
  CreateTextFile(link_to, bogus_content);

  ASSERT_TRUE(CreateSymbolicLink(link_to, link_from))
      << "Failed to create file symlink.";

  // Check that NormalizeFilePath sees the link.
  FilePath normalized_path;
  ASSERT_TRUE(NormalizeFilePath(link_from, &normalized_path));
  EXPECT_NE(link_from, link_to);
  EXPECT_EQ(link_to.BaseName().value(), normalized_path.BaseName().value());
  EXPECT_EQ(link_to.BaseName().value(), normalized_path.BaseName().value());

  // Link to a directory.
  link_from = temp_dir_.GetPath().Append(FPL("from_dir"));
  link_to = temp_dir_.GetPath().Append(FPL("to_dir"));
  ASSERT_TRUE(CreateDirectory(link_to));
  ASSERT_TRUE(CreateSymbolicLink(link_to, link_from))
      << "Failed to create directory symlink.";

  EXPECT_TRUE(NormalizeFilePath(link_from, &normalized_path))
      << "Links to directories should return true.";

  // Test that a loop in the links causes NormalizeFilePath() to return false.
  link_from = temp_dir_.GetPath().Append(FPL("link_a"));
  link_to = temp_dir_.GetPath().Append(FPL("link_b"));
  ASSERT_TRUE(CreateSymbolicLink(link_to, link_from))
      << "Failed to create loop symlink a.";
  ASSERT_TRUE(CreateSymbolicLink(link_from, link_to))
      << "Failed to create loop symlink b.";

  // Infinite loop!
  EXPECT_FALSE(NormalizeFilePath(link_from, &normalized_path));
}

TEST_F(FileUtilTest, DeleteSymlinkToExistentFile) {
  // Create a file.
  FilePath file_name = temp_dir_.GetPath().Append(FPL("Test DeleteFile 2.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(PathExists(file_name));

  // Create a symlink to the file.
  FilePath file_link = temp_dir_.GetPath().Append("file_link_2");
  ASSERT_TRUE(CreateSymbolicLink(file_name, file_link))
      << "Failed to create symlink.";

  // Delete the symbolic link.
  EXPECT_TRUE(DeleteFile(file_link));

  // Make sure original file is not deleted.
  EXPECT_FALSE(PathExists(file_link));
  EXPECT_TRUE(PathExists(file_name));
}

TEST_F(FileUtilTest, DeleteSymlinkToNonExistentFile) {
  // Create a non-existent file path.
  FilePath non_existent =
      temp_dir_.GetPath().Append(FPL("Test DeleteFile 3.txt"));
  EXPECT_FALSE(PathExists(non_existent));

  // Create a symlink to the non-existent file.
  FilePath file_link = temp_dir_.GetPath().Append("file_link_3");
  ASSERT_TRUE(CreateSymbolicLink(non_existent, file_link))
      << "Failed to create symlink.";

  // Make sure the symbolic link is exist.
  EXPECT_TRUE(IsLink(file_link));
  EXPECT_FALSE(PathExists(file_link));

  // Delete the symbolic link.
  EXPECT_TRUE(DeleteFile(file_link));

  // Make sure the symbolic link is deleted.
  EXPECT_FALSE(IsLink(file_link));
}

TEST_F(FileUtilTest, CopyFileFollowsSymlinks) {
  FilePath link_from = temp_dir_.GetPath().Append(FPL("from_file"));
  FilePath link_to = temp_dir_.GetPath().Append(FPL("to_file"));
  CreateTextFile(link_to, bogus_content);

  ASSERT_TRUE(CreateSymbolicLink(link_to, link_from));

  // If we created the link properly, we should be able to read the contents
  // through it.
  EXPECT_EQ(bogus_content, ReadTextFile(link_from));

  FilePath result;
  ASSERT_TRUE(ReadSymbolicLink(link_from, &result));
  EXPECT_EQ(link_to.value(), result.value());

  // Create another file and copy it to |link_from|.
  FilePath src_file = temp_dir_.GetPath().Append(FPL("src.txt"));
  const std::wstring file_contents(L"Gooooooooooooooooooooogle");
  CreateTextFile(src_file, file_contents);
  ASSERT_TRUE(CopyFile(src_file, link_from));

  // Make sure |link_from| is still a symlink, and |link_to| has been written to
  // by CopyFile().
  EXPECT_TRUE(IsLink(link_from));
  EXPECT_EQ(file_contents, ReadTextFile(link_from));
  EXPECT_EQ(file_contents, ReadTextFile(link_to));
}

TEST_F(FileUtilTest, ChangeFilePermissionsAndRead) {
  // Create a file path.
  FilePath file_name =
      temp_dir_.GetPath().Append(FPL("Test Readable File.txt"));
  EXPECT_FALSE(PathExists(file_name));
  EXPECT_FALSE(PathIsReadable(file_name));

  static constexpr char kData[] = "hello";
  static constexpr int kDataSize = sizeof(kData) - 1;
  char buffer[kDataSize];

  // Write file.
  EXPECT_TRUE(WriteFile(file_name, kData));
  EXPECT_TRUE(PathExists(file_name));

  // Make sure the file is readable.
  int32_t mode = 0;
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & FILE_PERMISSION_READ_BY_USER);
  EXPECT_TRUE(PathIsReadable(file_name));

  // Get rid of the read permission.
  EXPECT_TRUE(SetPosixFilePermissions(file_name, 0u));
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_FALSE(mode & FILE_PERMISSION_READ_BY_USER);
  EXPECT_FALSE(PathIsReadable(file_name));
  // Make sure the file can't be read.
  EXPECT_EQ(ReadFile(file_name, buffer), std::nullopt);

  // Give the read permission.
  EXPECT_TRUE(SetPosixFilePermissions(file_name, FILE_PERMISSION_READ_BY_USER));
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & FILE_PERMISSION_READ_BY_USER);
  EXPECT_TRUE(PathIsReadable(file_name));
  // Make sure the file can be read.
  EXPECT_EQ(ReadFile(file_name, buffer), kDataSize);

  // Delete the file.
  EXPECT_TRUE(DeleteFile(file_name));
  EXPECT_FALSE(PathExists(file_name));
}

TEST_F(FileUtilTest, ChangeFilePermissionsAndWrite) {
  // Create a file path.
  FilePath file_name =
      temp_dir_.GetPath().Append(FPL("Test Readable File.txt"));
  EXPECT_FALSE(PathExists(file_name));

  const std::string kData("hello");

  // Write file.
  EXPECT_TRUE(WriteFile(file_name, kData));
  EXPECT_TRUE(PathExists(file_name));

  // Make sure the file is writable.
  int mode = 0;
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & FILE_PERMISSION_WRITE_BY_USER);
  EXPECT_TRUE(PathIsWritable(file_name));

  // Get rid of the write permission.
  EXPECT_TRUE(SetPosixFilePermissions(file_name, 0u));
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_FALSE(mode & FILE_PERMISSION_WRITE_BY_USER);
  // Make sure the file can't be write.
  EXPECT_FALSE(WriteFile(file_name, kData));
  EXPECT_FALSE(PathIsWritable(file_name));

  // Give read permission.
  EXPECT_TRUE(
      SetPosixFilePermissions(file_name, FILE_PERMISSION_WRITE_BY_USER));
  EXPECT_TRUE(GetPosixFilePermissions(file_name, &mode));
  EXPECT_TRUE(mode & FILE_PERMISSION_WRITE_BY_USER);
  // Make sure the file can be write.
  EXPECT_TRUE(WriteFile(file_name, kData));
  EXPECT_TRUE(PathIsWritable(file_name));

  // Delete the file.
  EXPECT_TRUE(DeleteFile(file_name));
  EXPECT_FALSE(PathExists(file_name));
}

TEST_F(FileUtilTest, ChangeDirectoryPermissionsAndEnumerate) {
  // Create a directory path.
  FilePath subdir_path = temp_dir_.GetPath().Append(FPL("PermissionTest1"));
  CreateDirectory(subdir_path);
  ASSERT_TRUE(PathExists(subdir_path));

  // Create a dummy file to enumerate.
  FilePath file_name = subdir_path.Append(FPL("Test Readable File.txt"));
  EXPECT_FALSE(PathExists(file_name));
  const std::string kData("hello");
  EXPECT_TRUE(WriteFile(file_name, kData));
  EXPECT_TRUE(PathExists(file_name));

  // Make sure the directory has the all permissions.
  int mode = 0;
  EXPECT_TRUE(GetPosixFilePermissions(subdir_path, &mode));
  EXPECT_EQ(FILE_PERMISSION_USER_MASK, mode & FILE_PERMISSION_USER_MASK);

  // Get rid of the permissions from the directory.
  EXPECT_TRUE(SetPosixFilePermissions(subdir_path, 0u));
  EXPECT_TRUE(GetPosixFilePermissions(subdir_path, &mode));
  EXPECT_FALSE(mode & FILE_PERMISSION_USER_MASK);

  // Make sure the file in the directory can't be enumerated.
  FileEnumerator f1(subdir_path, true, FileEnumerator::FILES);
  EXPECT_TRUE(PathExists(subdir_path));
  FindResultCollector c1(&f1);
  EXPECT_EQ(0, c1.size());
  EXPECT_FALSE(GetPosixFilePermissions(file_name, &mode));

  // Give the permissions to the directory.
  EXPECT_TRUE(SetPosixFilePermissions(subdir_path, FILE_PERMISSION_USER_MASK));
  EXPECT_TRUE(GetPosixFilePermissions(subdir_path, &mode));
  EXPECT_EQ(FILE_PERMISSION_USER_MASK, mode & FILE_PERMISSION_USER_MASK);

  // Make sure the file in the directory can be enumerated.
  FileEnumerator f2(subdir_path, true, FileEnumerator::FILES);
  FindResultCollector c2(&f2);
  EXPECT_TRUE(c2.HasFile(file_name));
  EXPECT_EQ(1, c2.size());

  // Delete the file.
  EXPECT_TRUE(DeletePathRecursively(subdir_path));
  EXPECT_FALSE(PathExists(subdir_path));
}

TEST_F(FileUtilTest, ExecutableExistsInPath) {
  // Create two directories that we will put in our PATH
  const FilePath::CharType kDir1[] = FPL("dir1");
  const FilePath::CharType kDir2[] = FPL("dir2");

  FilePath dir1 = temp_dir_.GetPath().Append(kDir1);
  FilePath dir2 = temp_dir_.GetPath().Append(kDir2);
  ASSERT_TRUE(CreateDirectory(dir1));
  ASSERT_TRUE(CreateDirectory(dir2));

  ScopedEnvironmentVariableOverride scoped_env(
      "PATH", dir1.value() + ":" + dir2.value());
  ASSERT_TRUE(scoped_env.IsOverridden());

  const FilePath::CharType kRegularFileName[] = FPL("regular_file");
  const FilePath::CharType kExeFileName[] = FPL("exe");
  const FilePath::CharType kDneFileName[] = FPL("does_not_exist");

  const FilePath kExePath = dir1.Append(kExeFileName);
  const FilePath kRegularFilePath = dir2.Append(kRegularFileName);

  // Write file.
  const std::string kData("hello");
  ASSERT_TRUE(WriteFile(kExePath, kData));
  ASSERT_TRUE(PathExists(kExePath));
  ASSERT_TRUE(WriteFile(kRegularFilePath, kData));
  ASSERT_TRUE(PathExists(kRegularFilePath));

  ASSERT_TRUE(SetPosixFilePermissions(dir1.Append(kExeFileName),
                                      FILE_PERMISSION_EXECUTE_BY_USER));

  EXPECT_TRUE(ExecutableExistsInPath(scoped_env.GetEnv(), kExeFileName));
  EXPECT_FALSE(ExecutableExistsInPath(scoped_env.GetEnv(), kRegularFileName));
  EXPECT_FALSE(ExecutableExistsInPath(scoped_env.GetEnv(), kDneFileName));
}

TEST_F(FileUtilTest, CopyDirectoryPermissions) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create some regular files under the directory with various permissions.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Reggy-1.txt"));
  CreateTextFile(file_name_from, L"Mordecai");
  ASSERT_TRUE(PathExists(file_name_from));
  ASSERT_TRUE(SetPosixFilePermissions(file_name_from, 0755));

  FilePath file2_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Reggy-2.txt"));
  CreateTextFile(file2_name_from, L"Rigby");
  ASSERT_TRUE(PathExists(file2_name_from));
  ASSERT_TRUE(SetPosixFilePermissions(file2_name_from, 0777));

  FilePath file3_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Reggy-3.txt"));
  CreateTextFile(file3_name_from, L"Benson");
  ASSERT_TRUE(PathExists(file3_name_from));
  ASSERT_TRUE(SetPosixFilePermissions(file3_name_from, 0400));

  // Copy the directory recursively.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Reggy-1.txt"));
  FilePath file2_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Reggy-2.txt"));
  FilePath file3_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Reggy-3.txt"));

  ASSERT_FALSE(PathExists(dir_name_to));

  EXPECT_TRUE(CopyDirectory(dir_name_from, dir_name_to, true));
  ASSERT_TRUE(PathExists(file_name_to));
  ASSERT_TRUE(PathExists(file2_name_to));
  ASSERT_TRUE(PathExists(file3_name_to));

  int mode = 0;
  int expected_mode;
  ASSERT_TRUE(GetPosixFilePermissions(file_name_to, &mode));
#if BUILDFLAG(IS_APPLE)
  expected_mode = 0755;
#elif BUILDFLAG(IS_CHROMEOS)
  expected_mode = 0644;
#else
  expected_mode = 0600;
#endif
  EXPECT_EQ(expected_mode, mode);

  ASSERT_TRUE(GetPosixFilePermissions(file2_name_to, &mode));
#if BUILDFLAG(IS_APPLE)
  expected_mode = 0755;
#elif BUILDFLAG(IS_CHROMEOS)
  expected_mode = 0644;
#else
  expected_mode = 0600;
#endif
  EXPECT_EQ(expected_mode, mode);

  ASSERT_TRUE(GetPosixFilePermissions(file3_name_to, &mode));
#if BUILDFLAG(IS_APPLE)
  expected_mode = 0600;
#elif BUILDFLAG(IS_CHROMEOS)
  expected_mode = 0644;
#else
  expected_mode = 0600;
#endif
  EXPECT_EQ(expected_mode, mode);
}

TEST_F(FileUtilTest, CopyDirectoryPermissionsOverExistingFile) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Reggy-1.txt"));
  CreateTextFile(file_name_from, L"Mordecai");
  ASSERT_TRUE(PathExists(file_name_from));
  ASSERT_TRUE(SetPosixFilePermissions(file_name_from, 0644));

  // Create a directory.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  CreateDirectory(dir_name_to);
  ASSERT_TRUE(PathExists(dir_name_to));

  // Create a file under the directory with wider permissions.
  FilePath file_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Reggy-1.txt"));
  CreateTextFile(file_name_to, L"Rigby");
  ASSERT_TRUE(PathExists(file_name_to));
  ASSERT_TRUE(SetPosixFilePermissions(file_name_to, 0777));

  // Ensure that when we copy the directory, the file contents are copied
  // but the permissions on the destination are left alone.
  EXPECT_TRUE(CopyDirectory(dir_name_from, dir_name_to, false));
  ASSERT_TRUE(PathExists(file_name_to));
  ASSERT_EQ(L"Mordecai", ReadTextFile(file_name_to));

  int mode = 0;
  ASSERT_TRUE(GetPosixFilePermissions(file_name_to, &mode));
  EXPECT_EQ(0777, mode);
}

TEST_F(FileUtilTest, CopyDirectoryExclDoesNotOverwrite) {
  // Create source directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Reggy-1.txt"));
  CreateTextFile(file_name_from, L"Mordecai");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create destination directory.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  CreateDirectory(dir_name_to);
  ASSERT_TRUE(PathExists(dir_name_to));

  // Create a file under the directory with the same name.
  FilePath file_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Reggy-1.txt"));
  CreateTextFile(file_name_to, L"Rigby");
  ASSERT_TRUE(PathExists(file_name_to));

  // Ensure that copying failed and the file was not overwritten.
  EXPECT_FALSE(CopyDirectoryExcl(dir_name_from, dir_name_to, false));
  ASSERT_TRUE(PathExists(file_name_to));
  ASSERT_EQ(L"Rigby", ReadTextFile(file_name_to));
}

TEST_F(FileUtilTest, CopyDirectoryExclDirectoryOverExistingFile) {
  // Create source directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from = dir_name_from.Append(FILE_PATH_LITERAL("Subsub"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create destination directory.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  CreateDirectory(dir_name_to);
  ASSERT_TRUE(PathExists(dir_name_to));

  // Create a regular file under the directory with the same name.
  FilePath file_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Subsub"));
  CreateTextFile(file_name_to, L"Rigby");
  ASSERT_TRUE(PathExists(file_name_to));

  // Ensure that copying failed and the file was not overwritten.
  EXPECT_FALSE(CopyDirectoryExcl(dir_name_from, dir_name_to, false));
  ASSERT_TRUE(PathExists(file_name_to));
  ASSERT_EQ(L"Rigby", ReadTextFile(file_name_to));
}

TEST_F(FileUtilTest, CopyDirectoryExclDirectoryOverExistingDirectory) {
  // Create source directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from = dir_name_from.Append(FILE_PATH_LITERAL("Subsub"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create destination directory.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  CreateDirectory(dir_name_to);
  ASSERT_TRUE(PathExists(dir_name_to));

  // Create a subdirectory under the directory with the same name.
  FilePath subdir_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Subsub"));
  CreateDirectory(subdir_name_to);
  ASSERT_TRUE(PathExists(subdir_name_to));

  // Ensure that copying failed and the file was not overwritten.
  EXPECT_FALSE(CopyDirectoryExcl(dir_name_from, dir_name_to, false));
}

TEST_F(FileUtilTest, CopyFileExecutablePermission) {
  FilePath src = temp_dir_.GetPath().Append(FPL("src.txt"));
  const std::wstring file_contents(L"Gooooooooooooooooooooogle");
  CreateTextFile(src, file_contents);

  ASSERT_TRUE(SetPosixFilePermissions(src, 0755));
  int mode = 0;
  ASSERT_TRUE(GetPosixFilePermissions(src, &mode));
  EXPECT_EQ(0755, mode);

  FilePath dst = temp_dir_.GetPath().Append(FPL("dst.txt"));
  ASSERT_TRUE(CopyFile(src, dst));
  EXPECT_EQ(file_contents, ReadTextFile(dst));

  ASSERT_TRUE(GetPosixFilePermissions(dst, &mode));
  int expected_mode;
#if BUILDFLAG(IS_APPLE)
  expected_mode = 0755;
#elif BUILDFLAG(IS_CHROMEOS)
  expected_mode = 0644;
#else
  expected_mode = 0600;
#endif
  EXPECT_EQ(expected_mode, mode);
  ASSERT_TRUE(DeleteFile(dst));

  ASSERT_TRUE(SetPosixFilePermissions(src, 0777));
  ASSERT_TRUE(GetPosixFilePermissions(src, &mode));
  EXPECT_EQ(0777, mode);

  ASSERT_TRUE(CopyFile(src, dst));
  EXPECT_EQ(file_contents, ReadTextFile(dst));

  ASSERT_TRUE(GetPosixFilePermissions(dst, &mode));
#if BUILDFLAG(IS_APPLE)
  expected_mode = 0755;
#elif BUILDFLAG(IS_CHROMEOS)
  expected_mode = 0644;
#else
  expected_mode = 0600;
#endif
  EXPECT_EQ(expected_mode, mode);
  ASSERT_TRUE(DeleteFile(dst));

  ASSERT_TRUE(SetPosixFilePermissions(src, 0400));
  ASSERT_TRUE(GetPosixFilePermissions(src, &mode));
  EXPECT_EQ(0400, mode);

  ASSERT_TRUE(CopyFile(src, dst));
  EXPECT_EQ(file_contents, ReadTextFile(dst));

  ASSERT_TRUE(GetPosixFilePermissions(dst, &mode));
#if BUILDFLAG(IS_APPLE)
  expected_mode = 0600;
#elif BUILDFLAG(IS_CHROMEOS)
  expected_mode = 0644;
#else
  expected_mode = 0600;
#endif
  EXPECT_EQ(expected_mode, mode);

  // This time, do not delete |dst|. Instead set its permissions to 0777.
  ASSERT_TRUE(SetPosixFilePermissions(dst, 0777));
  ASSERT_TRUE(GetPosixFilePermissions(dst, &mode));
  EXPECT_EQ(0777, mode);

  // Overwrite it and check the permissions again.
  ASSERT_TRUE(CopyFile(src, dst));
  EXPECT_EQ(file_contents, ReadTextFile(dst));
  ASSERT_TRUE(GetPosixFilePermissions(dst, &mode));
  EXPECT_EQ(0777, mode);
}

#endif  // BUILDFLAG(IS_POSIX)

#if !BUILDFLAG(IS_FUCHSIA)

TEST_F(FileUtilTest, CopyFileACL) {
  // While FileUtilTest.CopyFile asserts the content is correctly copied over,
  // this test case asserts the access control bits are meeting expectations in
  // CopyFile().
  FilePath src = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("src.txt"));
  const std::wstring file_contents(L"Gooooooooooooooooooooogle");
  CreateTextFile(src, file_contents);

  // Set the source file to read-only.
  ASSERT_FALSE(IsReadOnly(src));
  SetReadOnly(src, true);
  ASSERT_TRUE(IsReadOnly(src));

  // Copy the file.
  FilePath dst = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("dst.txt"));
  ASSERT_TRUE(CopyFile(src, dst));
  EXPECT_EQ(file_contents, ReadTextFile(dst));

  ASSERT_FALSE(IsReadOnly(dst));
}

TEST_F(FileUtilTest, CopyDirectoryACL) {
  // Create source directories.
  FilePath src = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("src"));
  FilePath src_subdir = src.Append(FILE_PATH_LITERAL("subdir"));
  CreateDirectory(src_subdir);
  ASSERT_TRUE(PathExists(src_subdir));

  // Create a file under the directory.
  FilePath src_file = src.Append(FILE_PATH_LITERAL("src.txt"));
  CreateTextFile(src_file, L"Gooooooooooooooooooooogle");
  SetReadOnly(src_file, true);
  ASSERT_TRUE(IsReadOnly(src_file));

  // Make directory read-only.
  SetReadOnly(src_subdir, true);
  ASSERT_TRUE(IsReadOnly(src_subdir));

  // Copy the directory recursively.
  FilePath dst = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("dst"));
  FilePath dst_file = dst.Append(FILE_PATH_LITERAL("src.txt"));
  EXPECT_TRUE(CopyDirectory(src, dst, true));

  FilePath dst_subdir = dst.Append(FILE_PATH_LITERAL("subdir"));
  ASSERT_FALSE(IsReadOnly(dst_subdir));
  ASSERT_FALSE(IsReadOnly(dst_file));

  // Give write permissions to allow deletion.
  SetReadOnly(src_subdir, false);
  ASSERT_FALSE(IsReadOnly(src_subdir));
}

#endif  // !BUILDFLAG(IS_FUCHSIA)

TEST_F(FileUtilTest, DeleteNonExistent) {
  FilePath non_existent =
      temp_dir_.GetPath().AppendASCII("bogus_file_dne.foobar");
  ASSERT_FALSE(PathExists(non_existent));

  EXPECT_TRUE(DeleteFile(non_existent));
  ASSERT_FALSE(PathExists(non_existent));
  EXPECT_TRUE(DeletePathRecursively(non_existent));
  ASSERT_FALSE(PathExists(non_existent));
}

TEST_F(FileUtilTest, DeleteNonExistentWithNonExistentParent) {
  FilePath non_existent = temp_dir_.GetPath().AppendASCII("bogus_topdir");
  non_existent = non_existent.AppendASCII("bogus_subdir");
  ASSERT_FALSE(PathExists(non_existent));

  EXPECT_TRUE(DeleteFile(non_existent));
  ASSERT_FALSE(PathExists(non_existent));
  EXPECT_TRUE(DeletePathRecursively(non_existent));
  ASSERT_FALSE(PathExists(non_existent));
}

TEST_F(FileUtilTest, DeleteFile) {
  // Create a file
  FilePath file_name = temp_dir_.GetPath().Append(FPL("Test DeleteFile 1.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(PathExists(file_name));

  // Make sure it's deleted
  EXPECT_TRUE(DeleteFile(file_name));
  EXPECT_FALSE(PathExists(file_name));

  // Test recursive case, create a new file
  file_name = temp_dir_.GetPath().Append(FPL("Test DeleteFile 2.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(PathExists(file_name));

  // Make sure it's deleted
  EXPECT_TRUE(DeletePathRecursively(file_name));
  EXPECT_FALSE(PathExists(file_name));
}

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
TEST_F(FileUtilTest, DeleteDeep) {
  // Create deeply nested directories.
  const FilePath dir_path = temp_dir_.GetPath().AppendASCII("deep");
  ASSERT_EQ(mkdir(dir_path.value().c_str(), 0777), 0);

  {
    ScopedFD fd(
        HANDLE_EINTR(open(dir_path.value().c_str(), O_DIRECTORY | O_CLOEXEC)));
    ASSERT_TRUE(fd.is_valid()) << strerror(errno);

    for (char c = 'a'; c <= 'z'; ++c) {
      const std::string name(NAME_MAX, c);
      ASSERT_EQ(HANDLE_EINTR(mkdirat(fd.get(), name.c_str(), 0777)), 0)
          << strerror(errno);

      fd = ScopedFD(HANDLE_EINTR(
          openat(fd.get(), name.c_str(), O_DIRECTORY | O_CLOEXEC)));
      ASSERT_TRUE(fd.is_valid()) << strerror(errno);
    }

#if !BUILDFLAG(IS_FUCHSIA)
    // Create a symlink at the bottom of the deep tree.
    ASSERT_EQ(HANDLE_EINTR(symlinkat("..", fd.get(), "up")), 0)
        << strerror(errno);
#endif  // !BUILDFLAG(IS_FUCHSIA)

    // Create a file at the bottom of the deep tree.
    fd = ScopedFD(HANDLE_EINTR(openat(
        fd.get(), "file.txt", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666)));
    ASSERT_TRUE(fd.is_valid()) << strerror(errno);

    const std::string_view s = "This is a deep file";
    ASSERT_EQ(HANDLE_EINTR(write(fd.get(), s.data(), s.size())),
              static_cast<ssize_t>(s.size()));
  }

  // Delete the deep tree.
  EXPECT_TRUE(PathExists(dir_path));
  EXPECT_TRUE(DeletePathRecursively(dir_path));

  // Check if the deep tree is deleted.
  EXPECT_FALSE(PathExists(dir_path));
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_ANDROID)
TEST_F(FileUtilTest, ContentUriGetInfo) {
  FilePath file = temp_dir_.GetPath().Append("file.txt");
  FilePath dir = temp_dir_.GetPath().Append("dir");
  WriteFile(file, "file-content");
  CreateDirectory(dir);

  FilePath content_uri_file =
      *test::android::GetContentUriFromCacheDirFilePath(file);
  FilePath content_uri_dir =
      *test::android::GetContentUriFromCacheDirFilePath(dir);

  // GetInfo() should work the same for files and content-URIs.
  File::Info info;
  File::Info content_uri_info;
  EXPECT_TRUE(GetFileInfo(file, &info));
  EXPECT_TRUE(GetFileInfo(content_uri_file, &content_uri_info));
  EXPECT_EQ(12u, info.size);
  EXPECT_EQ(12u, content_uri_info.size);
  EXPECT_EQ(info.last_modified, content_uri_info.last_modified);
  EXPECT_FALSE(info.is_directory);
  EXPECT_FALSE(content_uri_info.is_directory);

  // GetInfo() should work the same for dirs and content-URIs.
  EXPECT_TRUE(GetFileInfo(dir, &info));
  EXPECT_TRUE(GetFileInfo(content_uri_dir, &content_uri_info));
  EXPECT_EQ(info.last_modified, content_uri_info.last_modified);
  EXPECT_TRUE(info.is_directory);
  EXPECT_TRUE(content_uri_info.is_directory);

  // GetPosixFilePermissions() should fail for content URIs.
  int mode = 0;
  EXPECT_TRUE(GetPosixFilePermissions(file, &mode));
  EXPECT_TRUE(GetPosixFilePermissions(dir, &mode));
  EXPECT_FALSE(GetPosixFilePermissions(content_uri_file, &mode));
  EXPECT_FALSE(GetPosixFilePermissions(content_uri_dir, &mode));
}

TEST_F(FileUtilTest, DeleteContentUri) {
  // Get the path to the test file.
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &data_dir));
  data_dir = data_dir.Append(FPL("file_util"));
  ASSERT_TRUE(PathExists(data_dir));
  FilePath image_file = data_dir.Append(FPL("red.png"));
  ASSERT_TRUE(PathExists(image_file));

  // Make a copy (we don't want to delete the original red.png when deleting the
  // content URI).
  FilePath image_copy = data_dir.Append(FPL("redcopy.png"));
  ASSERT_TRUE(CopyFile(image_file, image_copy));

  // Insert the image into MediaStore and get a content URI.
  FilePath uri_path = InsertImageIntoMediaStore(image_copy);
  ASSERT_TRUE(uri_path.IsContentUri());
  ASSERT_TRUE(PathExists(uri_path));

  // Try deleting the content URI.
  EXPECT_TRUE(DeleteFile(uri_path));
  EXPECT_FALSE(PathExists(image_copy));
  EXPECT_FALSE(PathExists(uri_path));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
// Tests that the Delete function works for wild cards, especially
// with the recursion flag.  Also coincidentally tests PathExists.
// TODO(erikkay): see if anyone's actually using this feature of the API
TEST_F(FileUtilTest, DeleteWildCard) {
  // Create a file and a directory
  FilePath file_name =
      temp_dir_.GetPath().Append(FPL("Test DeleteWildCard.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(PathExists(file_name));

  FilePath subdir_path = temp_dir_.GetPath().Append(FPL("DeleteWildCardDir"));
  CreateDirectory(subdir_path);
  ASSERT_TRUE(PathExists(subdir_path));

  // Create the wildcard path
  FilePath directory_contents = temp_dir_.GetPath();
  directory_contents = directory_contents.Append(FPL("*"));

  // Delete non-recursively and check that only the file is deleted
  EXPECT_TRUE(DeleteFile(directory_contents));
  EXPECT_FALSE(PathExists(file_name));
  EXPECT_TRUE(PathExists(subdir_path));

  // Delete recursively and make sure all contents are deleted
  EXPECT_TRUE(DeletePathRecursively(directory_contents));
  EXPECT_FALSE(PathExists(file_name));
  EXPECT_FALSE(PathExists(subdir_path));
}

// TODO(erikkay): see if anyone's actually using this feature of the API
TEST_F(FileUtilTest, DeleteNonExistantWildCard) {
  // Create a file and a directory
  FilePath subdir_path =
      temp_dir_.GetPath().Append(FPL("DeleteNonExistantWildCard"));
  CreateDirectory(subdir_path);
  ASSERT_TRUE(PathExists(subdir_path));

  // Create the wildcard path
  FilePath directory_contents = subdir_path;
  directory_contents = directory_contents.Append(FPL("*"));

  // Delete non-recursively and check nothing got deleted
  EXPECT_TRUE(DeleteFile(directory_contents));
  EXPECT_TRUE(PathExists(subdir_path));

  // Delete recursively and check nothing got deleted
  EXPECT_TRUE(DeletePathRecursively(directory_contents));
  EXPECT_TRUE(PathExists(subdir_path));
}
#endif

// Tests non-recursive Delete() for a directory.
TEST_F(FileUtilTest, DeleteDirNonRecursive) {
  // Create a subdirectory and put a file and two directories inside.
  FilePath test_subdir =
      temp_dir_.GetPath().Append(FPL("DeleteDirNonRecursive"));
  CreateDirectory(test_subdir);
  ASSERT_TRUE(PathExists(test_subdir));

  FilePath file_name = test_subdir.Append(FPL("Test DeleteDir.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(PathExists(file_name));

  FilePath subdir_path1 = test_subdir.Append(FPL("TestSubDir1"));
  CreateDirectory(subdir_path1);
  ASSERT_TRUE(PathExists(subdir_path1));

  FilePath subdir_path2 = test_subdir.Append(FPL("TestSubDir2"));
  CreateDirectory(subdir_path2);
  ASSERT_TRUE(PathExists(subdir_path2));

  // Delete non-recursively and check that the empty dir got deleted
  EXPECT_TRUE(DeleteFile(subdir_path2));
  EXPECT_FALSE(PathExists(subdir_path2));

  // Delete non-recursively and check that nothing got deleted
  EXPECT_FALSE(DeleteFile(test_subdir));
  EXPECT_TRUE(PathExists(test_subdir));
  EXPECT_TRUE(PathExists(file_name));
  EXPECT_TRUE(PathExists(subdir_path1));
}

// Tests recursive Delete() for a directory.
TEST_F(FileUtilTest, DeleteDirRecursive) {
  // Create a subdirectory and put a file and two directories inside.
  FilePath test_subdir = temp_dir_.GetPath().Append(FPL("DeleteDirRecursive"));
  CreateDirectory(test_subdir);
  ASSERT_TRUE(PathExists(test_subdir));

  FilePath file_name = test_subdir.Append(FPL("Test DeleteDirRecursive.txt"));
  CreateTextFile(file_name, bogus_content);
  ASSERT_TRUE(PathExists(file_name));

  FilePath subdir_path1 = test_subdir.Append(FPL("TestSubDir1"));
  CreateDirectory(subdir_path1);
  ASSERT_TRUE(PathExists(subdir_path1));

  FilePath subdir_path2 = test_subdir.Append(FPL("TestSubDir2"));
  CreateDirectory(subdir_path2);
  ASSERT_TRUE(PathExists(subdir_path2));

  // Delete recursively and check that the empty dir got deleted
  EXPECT_TRUE(DeletePathRecursively(subdir_path2));
  EXPECT_FALSE(PathExists(subdir_path2));

  // Delete recursively and check that everything got deleted
  EXPECT_TRUE(DeletePathRecursively(test_subdir));
  EXPECT_FALSE(PathExists(file_name));
  EXPECT_FALSE(PathExists(subdir_path1));
  EXPECT_FALSE(PathExists(test_subdir));
}

// Tests recursive Delete() for a directory.
TEST_F(FileUtilTest, DeleteDirRecursiveWithOpenFile) {
  // Create a subdirectory and put a file and two directories inside.
  FilePath test_subdir = temp_dir_.GetPath().Append(FPL("DeleteWithOpenFile"));
  CreateDirectory(test_subdir);
  ASSERT_TRUE(PathExists(test_subdir));

  FilePath file_name1 = test_subdir.Append(FPL("Undeletebable File1.txt"));
  File file1(file_name1,
             File::FLAG_CREATE | File::FLAG_READ | File::FLAG_WRITE);
  ASSERT_TRUE(PathExists(file_name1));

  FilePath file_name2 = test_subdir.Append(FPL("Deleteable File2.txt"));
  CreateTextFile(file_name2, bogus_content);
  ASSERT_TRUE(PathExists(file_name2));

  FilePath file_name3 = test_subdir.Append(FPL("Undeletebable File3.txt"));
  File file3(file_name3,
             File::FLAG_CREATE | File::FLAG_READ | File::FLAG_WRITE);
  ASSERT_TRUE(PathExists(file_name3));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // On Windows, holding the file open in sufficient to make it un-deletable.
  // The POSIX code is verifiable on Linux by creating an "immutable" file but
  // this is best-effort because it's not supported by all file systems. Both
  // files will have the same flags so no need to get them individually.
  int flags;
  bool file_attrs_supported =
      ioctl(file1.GetPlatformFile(), FS_IOC_GETFLAGS, &flags) == 0;
  // Some filesystems (e.g. tmpfs) don't support file attributes.
  if (file_attrs_supported) {
    flags |= FS_IMMUTABLE_FL;
    ioctl(file1.GetPlatformFile(), FS_IOC_SETFLAGS, &flags);
    ioctl(file3.GetPlatformFile(), FS_IOC_SETFLAGS, &flags);
  }
#endif

  // Delete recursively and check that at least the second file got deleted.
  // This ensures that un-deletable files don't impact those that can be.
  DeletePathRecursively(test_subdir);
  EXPECT_FALSE(PathExists(file_name2));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Make sure that the test can clean up after itself.
  if (file_attrs_supported) {
    flags &= ~FS_IMMUTABLE_FL;
    ioctl(file1.GetPlatformFile(), FS_IOC_SETFLAGS, &flags);
    ioctl(file3.GetPlatformFile(), FS_IOC_SETFLAGS, &flags);
  }
#endif
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// This test will validate that files which would block when read result in a
// failure on a call to ReadFileToStringNonBlocking. To accomplish this we will
// use a named pipe because it appears as a file on disk and we can control how
// much data is available to read. This allows us to simulate a file which would
// block.
TEST_F(FileUtilTest, TestNonBlockingFileReadLinux) {
  FilePath fifo_path = temp_dir_.GetPath().Append(FPL("fifo"));
  int res = mkfifo(fifo_path.MaybeAsASCII().c_str(),
                   S_IWUSR | S_IRUSR | S_IWGRP | S_IWGRP);
  ASSERT_NE(res, -1);

  base::ScopedFD fd(open(fifo_path.MaybeAsASCII().c_str(), O_RDWR));
  ASSERT_TRUE(fd.is_valid());

  std::string result;
  // We will try to read when nothing is available on the fifo, the output
  // string will be unmodified and it will fail with EWOULDBLOCK.
  ASSERT_FALSE(ReadFileToStringNonBlocking(fifo_path, &result));
  EXPECT_EQ(errno, EWOULDBLOCK);
  EXPECT_TRUE(result.empty());

  // Make a single byte available to read on the FIFO.
  ASSERT_EQ(write(fd.get(), "a", 1), 1);

  // Now the key part of the test we will call ReadFromFileNonBlocking which
  // should fail, errno will be EWOULDBLOCK and the output string will contain
  // the single 'a' byte.
  ASSERT_FALSE(ReadFileToStringNonBlocking(fifo_path, &result));
  EXPECT_EQ(errno, EWOULDBLOCK);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], 'a');
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

TEST_F(FileUtilTest, MoveFileNew) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination.
  FilePath file_name_to = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("Move_Test_File_Destination.txt"));
  ASSERT_FALSE(PathExists(file_name_to));

  EXPECT_TRUE(Move(file_name_from, file_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(file_name_to));
}

TEST_F(FileUtilTest, MoveFileExists) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination name.
  FilePath file_name_to = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("Move_Test_File_Destination.txt"));
  CreateTextFile(file_name_to, L"Old file content");
  ASSERT_TRUE(PathExists(file_name_to));

  EXPECT_TRUE(Move(file_name_from, file_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_TRUE(L"Gooooooooooooooooooooogle" == ReadTextFile(file_name_to));
}

TEST_F(FileUtilTest, MoveFileDirExists) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination directory
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Destination"));
  CreateDirectory(dir_name_to);
  ASSERT_TRUE(PathExists(dir_name_to));

  EXPECT_FALSE(Move(file_name_from, dir_name_to));
}

TEST_F(FileUtilTest, MoveNew) {
  // Create a directory
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Move_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory
  FilePath txt_file_name(FILE_PATH_LITERAL("Move_Test_File.txt"));
  FilePath file_name_from = dir_name_from.Append(txt_file_name);
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Move the directory.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Move_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));

  ASSERT_FALSE(PathExists(dir_name_to));

  EXPECT_TRUE(Move(dir_name_from, dir_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(PathExists(dir_name_from));
  EXPECT_FALSE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));

  // Test path traversal.
  file_name_from = dir_name_to.Append(txt_file_name);
  file_name_to = dir_name_to.Append(FILE_PATH_LITERAL(".."));
  file_name_to = file_name_to.Append(txt_file_name);
  EXPECT_FALSE(Move(file_name_from, file_name_to));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_FALSE(PathExists(file_name_to));
  EXPECT_TRUE(internal::MoveUnsafe(file_name_from, file_name_to));
  EXPECT_FALSE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(file_name_to));
}

TEST_F(FileUtilTest, MoveExist) {
  // Create a directory
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Move_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Move the directory
  FilePath dir_name_exists =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Destination"));

  FilePath dir_name_to =
      dir_name_exists.Append(FILE_PATH_LITERAL("Move_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));

  // Create the destination directory.
  CreateDirectory(dir_name_exists);
  ASSERT_TRUE(PathExists(dir_name_exists));

  EXPECT_TRUE(Move(dir_name_from, dir_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(PathExists(dir_name_from));
  EXPECT_FALSE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
}

TEST_F(FileUtilTest, CopyDirectoryRecursivelyNew) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from = dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name2_from));

  // Copy the directory recursively.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));
  FilePath file_name2_to =
      subdir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  ASSERT_FALSE(PathExists(dir_name_to));

  EXPECT_TRUE(CopyDirectory(dir_name_from, dir_name_to, true));

  // Check everything has been copied.
  EXPECT_TRUE(PathExists(dir_name_from));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(subdir_name_from));
  EXPECT_TRUE(PathExists(file_name2_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_TRUE(PathExists(subdir_name_to));
  EXPECT_TRUE(PathExists(file_name2_to));
}

TEST_F(FileUtilTest, CopyDirectoryRecursivelyExists) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from = dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name2_from));

  // Copy the directory recursively.
  FilePath dir_name_exists =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Destination"));

  FilePath dir_name_to =
      dir_name_exists.Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));
  FilePath file_name2_to =
      subdir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  // Create the destination directory.
  CreateDirectory(dir_name_exists);
  ASSERT_TRUE(PathExists(dir_name_exists));

  EXPECT_TRUE(CopyDirectory(dir_name_from, dir_name_exists, true));

  // Check everything has been copied.
  EXPECT_TRUE(PathExists(dir_name_from));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(subdir_name_from));
  EXPECT_TRUE(PathExists(file_name2_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_TRUE(PathExists(subdir_name_to));
  EXPECT_TRUE(PathExists(file_name2_to));
}

TEST_F(FileUtilTest, CopyDirectoryNew) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from = dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name2_from));

  // Copy the directory not recursively.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));

  ASSERT_FALSE(PathExists(dir_name_to));

  EXPECT_TRUE(CopyDirectory(dir_name_from, dir_name_to, false));

  // Check everything has been copied.
  EXPECT_TRUE(PathExists(dir_name_from));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(subdir_name_from));
  EXPECT_TRUE(PathExists(file_name2_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_FALSE(PathExists(subdir_name_to));
}

TEST_F(FileUtilTest, CopyDirectoryExists) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from = dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name2_from));

  // Copy the directory not recursively.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));

  // Create the destination directory.
  CreateDirectory(dir_name_to);
  ASSERT_TRUE(PathExists(dir_name_to));

  EXPECT_TRUE(CopyDirectory(dir_name_from, dir_name_to, false));

  // Check everything has been copied.
  EXPECT_TRUE(PathExists(dir_name_from));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(subdir_name_from));
  EXPECT_TRUE(PathExists(file_name2_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_FALSE(PathExists(subdir_name_to));
}

TEST_F(FileUtilTest, CopyFileWithCopyDirectoryRecursiveToNew) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination name
  FilePath file_name_to = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("Copy_Test_File_Destination.txt"));
  ASSERT_FALSE(PathExists(file_name_to));

  EXPECT_TRUE(CopyDirectory(file_name_from, file_name_to, true));

  // Check the has been copied
  EXPECT_TRUE(PathExists(file_name_to));
}

TEST_F(FileUtilTest, CopyFileWithCopyDirectoryRecursiveToExisting) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination name
  FilePath file_name_to = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("Copy_Test_File_Destination.txt"));
  CreateTextFile(file_name_to, L"Old file content");
  ASSERT_TRUE(PathExists(file_name_to));

  EXPECT_TRUE(CopyDirectory(file_name_from, file_name_to, true));

  // Check the has been copied
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_TRUE(L"Gooooooooooooooooooooogle" == ReadTextFile(file_name_to));
}

TEST_F(FileUtilTest, CopyFileWithCopyDirectoryRecursiveToExistingDirectory) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // The destination
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Destination"));
  CreateDirectory(dir_name_to);
  ASSERT_TRUE(PathExists(dir_name_to));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  EXPECT_TRUE(CopyDirectory(file_name_from, dir_name_to, true));

  // Check the has been copied
  EXPECT_TRUE(PathExists(file_name_to));
}

TEST_F(FileUtilTest, CopyFileFailureWithCopyDirectoryExcl) {
  // Create a file
  FilePath file_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Make a destination file.
  FilePath file_name_to = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("Copy_Test_File_Destination.txt"));
  CreateTextFile(file_name_to, L"Old file content");
  ASSERT_TRUE(PathExists(file_name_to));

  // Overwriting the destination should fail.
  EXPECT_FALSE(CopyDirectoryExcl(file_name_from, file_name_to, true));
  EXPECT_EQ(L"Old file content", ReadTextFile(file_name_to));
}

TEST_F(FileUtilTest, CopyDirectoryWithTrailingSeparators) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Copy the directory recursively.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  // Create from path with trailing separators.
#if BUILDFLAG(IS_WIN)
  FilePath from_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir\\\\\\"));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  FilePath from_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir///"));
#endif

  EXPECT_TRUE(CopyDirectory(from_path, dir_name_to, true));

  // Check everything has been copied.
  EXPECT_TRUE(PathExists(dir_name_from));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
}

#if BUILDFLAG(IS_POSIX)
TEST_F(FileUtilTest, CopyDirectoryWithNonRegularFiles) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_from));
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a symbolic link under the directory pointing to that file.
  FilePath symlink_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Symlink"));
  ASSERT_TRUE(CreateSymbolicLink(file_name_from, symlink_name_from));
  ASSERT_TRUE(PathExists(symlink_name_from));

  // Create a fifo under the directory.
  FilePath fifo_name_from = dir_name_from.Append(FILE_PATH_LITERAL("Fifo"));
  ASSERT_EQ(0, mkfifo(fifo_name_from.value().c_str(), 0644));
  ASSERT_TRUE(PathExists(fifo_name_from));

  // Copy the directory.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath symlink_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Symlink"));
  FilePath fifo_name_to = dir_name_to.Append(FILE_PATH_LITERAL("Fifo"));

  ASSERT_FALSE(PathExists(dir_name_to));

  EXPECT_TRUE(CopyDirectory(dir_name_from, dir_name_to, false));

  // Check that only directories and regular files are copied.
  EXPECT_TRUE(PathExists(dir_name_from));
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(symlink_name_from));
  EXPECT_TRUE(PathExists(fifo_name_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
  EXPECT_FALSE(PathExists(symlink_name_to));
  EXPECT_FALSE(PathExists(fifo_name_to));
}

TEST_F(FileUtilTest, CopyDirectoryExclFileOverSymlink) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_from));
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a destination directory with a symlink of the same name.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_to));
  ASSERT_TRUE(PathExists(dir_name_to));

  FilePath symlink_target =
      dir_name_to.Append(FILE_PATH_LITERAL("Symlink_Target.txt"));
  CreateTextFile(symlink_target, L"asdf");
  ASSERT_TRUE(PathExists(symlink_target));

  FilePath symlink_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  ASSERT_TRUE(CreateSymbolicLink(symlink_target, symlink_name_to));
  ASSERT_TRUE(PathExists(symlink_name_to));

  // Check that copying fails.
  EXPECT_FALSE(CopyDirectoryExcl(dir_name_from, dir_name_to, false));
}

TEST_F(FileUtilTest, CopyDirectoryExclDirectoryOverSymlink) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_from));
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from = dir_name_from.Append(FILE_PATH_LITERAL("Subsub"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create a destination directory with a symlink of the same name.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_to));
  ASSERT_TRUE(PathExists(dir_name_to));

  FilePath symlink_target = dir_name_to.Append(FILE_PATH_LITERAL("Subsub"));
  CreateTextFile(symlink_target, L"asdf");
  ASSERT_TRUE(PathExists(symlink_target));

  FilePath symlink_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  ASSERT_TRUE(CreateSymbolicLink(symlink_target, symlink_name_to));
  ASSERT_TRUE(PathExists(symlink_name_to));

  // Check that copying fails.
  EXPECT_FALSE(CopyDirectoryExcl(dir_name_from, dir_name_to, false));
}

TEST_F(FileUtilTest, CopyDirectoryExclFileOverDanglingSymlink) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_from));
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a destination directory with a dangling symlink of the same name.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_to));
  ASSERT_TRUE(PathExists(dir_name_to));

  FilePath symlink_target =
      dir_name_to.Append(FILE_PATH_LITERAL("Symlink_Target.txt"));
  CreateTextFile(symlink_target, L"asdf");
  ASSERT_TRUE(PathExists(symlink_target));

  FilePath symlink_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  ASSERT_TRUE(CreateSymbolicLink(symlink_target, symlink_name_to));
  ASSERT_TRUE(PathExists(symlink_name_to));
  ASSERT_TRUE(DeleteFile(symlink_target));

  // Check that copying fails and that no file was created for the symlink's
  // referent.
  EXPECT_FALSE(CopyDirectoryExcl(dir_name_from, dir_name_to, false));
  EXPECT_FALSE(PathExists(symlink_target));
}

TEST_F(FileUtilTest, CopyDirectoryExclDirectoryOverDanglingSymlink) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_from));
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from = dir_name_from.Append(FILE_PATH_LITERAL("Subsub"));
  CreateDirectory(subdir_name_from);
  ASSERT_TRUE(PathExists(subdir_name_from));

  // Create a destination directory with a dangling symlink of the same name.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_to));
  ASSERT_TRUE(PathExists(dir_name_to));

  FilePath symlink_target =
      dir_name_to.Append(FILE_PATH_LITERAL("Symlink_Target.txt"));
  CreateTextFile(symlink_target, L"asdf");
  ASSERT_TRUE(PathExists(symlink_target));

  FilePath symlink_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  ASSERT_TRUE(CreateSymbolicLink(symlink_target, symlink_name_to));
  ASSERT_TRUE(PathExists(symlink_name_to));
  ASSERT_TRUE(DeleteFile(symlink_target));

  // Check that copying fails and that no directory was created for the
  // symlink's referent.
  EXPECT_FALSE(CopyDirectoryExcl(dir_name_from, dir_name_to, false));
  EXPECT_FALSE(PathExists(symlink_target));
}

TEST_F(FileUtilTest, CopyDirectoryExclFileOverFifo) {
  // Create a directory.
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_from));
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Create a destination directory with a fifo of the same name.
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_to));
  ASSERT_TRUE(PathExists(dir_name_to));

  FilePath fifo_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  ASSERT_EQ(0, mkfifo(fifo_name_to.value().c_str(), 0644));
  ASSERT_TRUE(PathExists(fifo_name_to));

  // Check that copying fails.
  EXPECT_FALSE(CopyDirectoryExcl(dir_name_from, dir_name_to, false));
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(FileUtilTest, CopyFile) {
  // Create a directory
  FilePath dir_name_from =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  ASSERT_TRUE(CreateDirectory(dir_name_from));
  ASSERT_TRUE(DirectoryExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  const std::wstring file_contents(L"Gooooooooooooooooooooogle");
  CreateTextFile(file_name_from, file_contents);
  ASSERT_TRUE(PathExists(file_name_from));

  // Copy the file.
  FilePath dest_file = dir_name_from.Append(FILE_PATH_LITERAL("DestFile.txt"));
  ASSERT_TRUE(CopyFile(file_name_from, dest_file));

  // Try to copy the file to another location using '..' in the path.
  FilePath dest_file2(dir_name_from);
  dest_file2 = dest_file2.AppendASCII("..");
  dest_file2 = dest_file2.AppendASCII("DestFile.txt");
  ASSERT_FALSE(CopyFile(file_name_from, dest_file2));

  FilePath dest_file2_test(dir_name_from);
  dest_file2_test = dest_file2_test.DirName();
  dest_file2_test = dest_file2_test.AppendASCII("DestFile.txt");

  // Check expected copy results.
  EXPECT_TRUE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(dest_file));
  EXPECT_EQ(file_contents, ReadTextFile(dest_file));
  EXPECT_FALSE(PathExists(dest_file2_test));
  EXPECT_FALSE(PathExists(dest_file2));

  // Change |file_name_from| contents.
  const std::wstring new_file_contents(L"Moogle");
  CreateTextFile(file_name_from, new_file_contents);
  ASSERT_TRUE(PathExists(file_name_from));
  EXPECT_EQ(new_file_contents, ReadTextFile(file_name_from));

  // Overwrite |dest_file|.
  ASSERT_TRUE(CopyFile(file_name_from, dest_file));
  EXPECT_TRUE(PathExists(dest_file));
  EXPECT_EQ(new_file_contents, ReadTextFile(dest_file));

  // Create another directory.
  FilePath dest_dir = temp_dir_.GetPath().Append(FPL("dest_dir"));
  ASSERT_TRUE(CreateDirectory(dest_dir));
  EXPECT_TRUE(DirectoryExists(dest_dir));
  EXPECT_TRUE(IsDirectoryEmpty(dest_dir));

  // Make sure CopyFile() cannot overwrite a directory.
  ASSERT_FALSE(CopyFile(file_name_from, dest_dir));
  EXPECT_TRUE(DirectoryExists(dest_dir));
  EXPECT_TRUE(IsDirectoryEmpty(dest_dir));
}

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest.
typedef PlatformTest ReadOnlyFileUtilTest;

TEST_F(ReadOnlyFileUtilTest, ContentsEqual) {
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &data_dir));
  data_dir = data_dir.AppendASCII("file_util");
  ASSERT_TRUE(PathExists(data_dir));

  FilePath original_file = data_dir.Append(FILE_PATH_LITERAL("original.txt"));
  FilePath same_file = data_dir.Append(FILE_PATH_LITERAL("same.txt"));
  FilePath same_length_file =
      data_dir.Append(FILE_PATH_LITERAL("same_length.txt"));
  FilePath different_file = data_dir.Append(FILE_PATH_LITERAL("different.txt"));
  FilePath different_first_file =
      data_dir.Append(FILE_PATH_LITERAL("different_first.txt"));
  FilePath different_last_file =
      data_dir.Append(FILE_PATH_LITERAL("different_last.txt"));
  FilePath empty1_file = data_dir.Append(FILE_PATH_LITERAL("empty1.txt"));
  FilePath empty2_file = data_dir.Append(FILE_PATH_LITERAL("empty2.txt"));
  FilePath shortened_file = data_dir.Append(FILE_PATH_LITERAL("shortened.txt"));
  FilePath binary_file = data_dir.Append(FILE_PATH_LITERAL("binary_file.bin"));
  FilePath binary_file_same =
      data_dir.Append(FILE_PATH_LITERAL("binary_file_same.bin"));
  FilePath binary_file_diff =
      data_dir.Append(FILE_PATH_LITERAL("binary_file_diff.bin"));

  EXPECT_TRUE(ContentsEqual(original_file, original_file));
  EXPECT_TRUE(ContentsEqual(original_file, same_file));
  EXPECT_FALSE(ContentsEqual(original_file, same_length_file));
  EXPECT_FALSE(ContentsEqual(original_file, different_file));
  EXPECT_FALSE(ContentsEqual(FilePath(FILE_PATH_LITERAL("bogusname")),
                             FilePath(FILE_PATH_LITERAL("bogusname"))));
  EXPECT_FALSE(ContentsEqual(original_file, different_first_file));
  EXPECT_FALSE(ContentsEqual(original_file, different_last_file));
  EXPECT_TRUE(ContentsEqual(empty1_file, empty2_file));
  EXPECT_FALSE(ContentsEqual(original_file, shortened_file));
  EXPECT_FALSE(ContentsEqual(shortened_file, original_file));
  EXPECT_TRUE(ContentsEqual(binary_file, binary_file_same));
  EXPECT_FALSE(ContentsEqual(binary_file, binary_file_diff));
}

TEST_F(ReadOnlyFileUtilTest, TextContentsEqual) {
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &data_dir));
  data_dir = data_dir.AppendASCII("file_util");
  ASSERT_TRUE(PathExists(data_dir));

  FilePath original_file = data_dir.Append(FILE_PATH_LITERAL("original.txt"));
  FilePath same_file = data_dir.Append(FILE_PATH_LITERAL("same.txt"));
  FilePath crlf_file = data_dir.Append(FILE_PATH_LITERAL("crlf.txt"));
  FilePath shortened_file = data_dir.Append(FILE_PATH_LITERAL("shortened.txt"));
  FilePath different_file = data_dir.Append(FILE_PATH_LITERAL("different.txt"));
  FilePath different_first_file =
      data_dir.Append(FILE_PATH_LITERAL("different_first.txt"));
  FilePath different_last_file =
      data_dir.Append(FILE_PATH_LITERAL("different_last.txt"));
  FilePath first1_file = data_dir.Append(FILE_PATH_LITERAL("first1.txt"));
  FilePath first2_file = data_dir.Append(FILE_PATH_LITERAL("first2.txt"));
  FilePath empty1_file = data_dir.Append(FILE_PATH_LITERAL("empty1.txt"));
  FilePath empty2_file = data_dir.Append(FILE_PATH_LITERAL("empty2.txt"));
  FilePath blank_line_file =
      data_dir.Append(FILE_PATH_LITERAL("blank_line.txt"));
  FilePath blank_line_crlf_file =
      data_dir.Append(FILE_PATH_LITERAL("blank_line_crlf.txt"));

  EXPECT_TRUE(TextContentsEqual(original_file, same_file));
  EXPECT_TRUE(TextContentsEqual(original_file, crlf_file));
  EXPECT_FALSE(TextContentsEqual(original_file, shortened_file));
  EXPECT_FALSE(TextContentsEqual(original_file, different_file));
  EXPECT_FALSE(TextContentsEqual(original_file, different_first_file));
  EXPECT_FALSE(TextContentsEqual(original_file, different_last_file));
  EXPECT_FALSE(TextContentsEqual(first1_file, first2_file));
  EXPECT_TRUE(TextContentsEqual(empty1_file, empty2_file));
  EXPECT_FALSE(TextContentsEqual(original_file, empty1_file));
  EXPECT_TRUE(TextContentsEqual(blank_line_file, blank_line_crlf_file));
}

// We don't need equivalent functionality outside of Windows.
#if BUILDFLAG(IS_WIN)
TEST_F(FileUtilTest, CopyAndDeleteDirectoryTest) {
  // Create a directory
  FilePath dir_name_from = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("CopyAndDelete_From_Subdir"));
  CreateDirectory(dir_name_from);
  ASSERT_TRUE(PathExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("CopyAndDelete_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(PathExists(file_name_from));

  // Move the directory by using CopyAndDeleteDirectory
  FilePath dir_name_to =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("CopyAndDelete_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("CopyAndDelete_Test_File.txt"));

  ASSERT_FALSE(PathExists(dir_name_to));

  EXPECT_TRUE(internal::CopyAndDeleteDirectory(dir_name_from, dir_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(PathExists(dir_name_from));
  EXPECT_FALSE(PathExists(file_name_from));
  EXPECT_TRUE(PathExists(dir_name_to));
  EXPECT_TRUE(PathExists(file_name_to));
}

TEST_F(FileUtilTest, GetTempDirTest) {
  const TCHAR* kTmpKey = _T("TMP");
  std::array<const TCHAR*, 5> kTmpValues = {_T(""), _T("C:"), _T("C:\\"),
                                            _T("C:\\tmp"), _T("C:\\tmp\\")};
  // Save the original $TMP.
  size_t original_tmp_size;
  TCHAR* original_tmp;
  ASSERT_EQ(0, ::_tdupenv_s(&original_tmp, &original_tmp_size, kTmpKey));
  // original_tmp may be NULL.

  for (const TCHAR* val : kTmpValues) {
    FilePath path;
    ::_tputenv_s(kTmpKey, val);
    GetTempDir(&path);
    EXPECT_TRUE(path.IsAbsolute())
        << "$TMP=" << val << " result=" << path.value();
  }

  // Restore the original $TMP.
  if (original_tmp) {
    ::_tputenv_s(kTmpKey, original_tmp);
    free(original_tmp);
  } else {
    ::_tputenv_s(kTmpKey, _T(""));
  }
}
#endif  // BUILDFLAG(IS_WIN)

// Test that files opened by OpenFile are not set up for inheritance into child
// procs.
TEST_F(FileUtilTest, OpenFileNoInheritance) {
  FilePath file_path(temp_dir_.GetPath().Append(FPL("a_file")));

// Character set handling is leaking according to ASAN. http://crbug.com/883698
#if defined(ADDRESS_SANITIZER)
  static constexpr const char* modes[] = {"wb", "r"};
#else
  static constexpr const char* modes[] = {"wb", "r,ccs=UTF-8"};
#endif

  for (const char* mode : modes) {
    SCOPED_TRACE(mode);
    ASSERT_NO_FATAL_FAILURE(CreateTextFile(file_path, L"Geepers"));
    FILE* file = OpenFile(file_path, mode);
    ASSERT_NE(nullptr, file);
    {
      absl::Cleanup file_closer = [file] { CloseFile(file); };
      bool is_inheritable = true;
      ASSERT_NO_FATAL_FAILURE(GetIsInheritable(file, &is_inheritable));
      EXPECT_FALSE(is_inheritable);
    }
    ASSERT_TRUE(DeleteFile(file_path));
  }
}

TEST_F(FileUtilTest, CreateAndOpenTemporaryFileInDir) {
  // Create a temporary file.
  FilePath path;
  File file = CreateAndOpenTemporaryFileInDir(temp_dir_.GetPath(), &path);
  ASSERT_TRUE(file.IsValid());
  EXPECT_FALSE(path.empty());

  // Try to open another handle to it.
  File file2(path,
             File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WIN_SHARE_DELETE);
#if BUILDFLAG(IS_WIN)
  // The file cannot be opened again on account of the exclusive access.
  EXPECT_FALSE(file2.IsValid());
#else
  // Exclusive access isn't a thing on non-Windows platforms.
  EXPECT_TRUE(file2.IsValid());
#endif
}

TEST_F(FileUtilTest, CreateTemporaryFileTest) {
  std::array<FilePath, 3> temp_files;
  for (auto& i : temp_files) {
    ASSERT_TRUE(CreateTemporaryFile(&i));
    EXPECT_TRUE(PathExists(i));
    EXPECT_FALSE(DirectoryExists(i));
  }
  for (size_t i = 0u; i < 3u; i++) {
    EXPECT_NE(temp_files[i], temp_files[(i + 1u) % 3u]);
  }
  for (const auto& i : temp_files) {
    EXPECT_TRUE(DeleteFile(i));
  }
}

TEST_F(FileUtilTest, CreateAndOpenTemporaryStreamTest) {
  std::array<FilePath, 3> names;
  std::array<ScopedFILE, 3> fps;
  size_t i;

  // Create; make sure they are open and exist.
  for (i = 0u; i < 3u; ++i) {
    fps[i] = CreateAndOpenTemporaryStream(&(names[i]));
    ASSERT_TRUE(fps[i]);
    EXPECT_TRUE(PathExists(names[i]));
  }

  // Make sure all names are unique.
  for (i = 0u; i < 3u; ++i) {
    EXPECT_NE(names[i], names[(i + 1u) % 3u]);
  }

  // Close and delete.
  for (i = 0u; i < 3u; ++i) {
    fps[i].reset();
    EXPECT_TRUE(DeleteFile(names[i]));
  }
}

TEST_F(FileUtilTest, GetUniquePath) {
  FilePath base_name(FPL("Unique_Base_Name.txt"));
  FilePath base_path = temp_dir_.GetPath().Append(base_name);
  EXPECT_FALSE(PathExists(base_path));

  // GetUniquePath() should return unchanged path if file does not exist.
  EXPECT_EQ(base_path, GetUniquePath(base_path));

  // Create the file.
  {
    File file(base_path,
              File::FLAG_CREATE | File::FLAG_READ | File::FLAG_WRITE);
    EXPECT_TRUE(PathExists(base_path));
  }

  static const FilePath::CharType* const kExpectedNames[] = {
      FPL("Unique_Base_Name (1).txt"),
      FPL("Unique_Base_Name (2).txt"),
      FPL("Unique_Base_Name (3).txt"),
  };

  // Call GetUniquePath() three times against this existing file name.
  for (const FilePath::CharType* expected_name : kExpectedNames) {
    FilePath expected_path = temp_dir_.GetPath().Append(expected_name);
    FilePath path = GetUniquePath(base_path);
    EXPECT_EQ(expected_path, path);

    // Verify that a file with this path indeed does not exist on the file
    // system.
    EXPECT_FALSE(PathExists(path));

    // Create the file so it exists for the next call to GetUniquePath() in the
    // loop.
    File file(path, File::FLAG_CREATE | File::FLAG_READ | File::FLAG_WRITE);
    EXPECT_TRUE(PathExists(path));
  }
}

TEST_F(FileUtilTest, GetUniquePathTooManyFiles) {
  // Create a file with the desired path.
  const FilePath some_file = temp_dir_.GetPath().Append(FPL("SomeFile.txt"));
  ASSERT_TRUE(File(some_file, File::FLAG_CREATE | File::FLAG_WRITE).IsValid());

  // Now create 100 collisions.
  for (int i = 1; i <= kMaxUniqueFiles; ++i) {
    FilePath path =
        temp_dir_.GetPath().AppendASCII(StringPrintf("SomeFile (%d).txt", i));
    ASSERT_EQ(GetUniquePath(some_file), path);
    ASSERT_TRUE(File(path, File::FLAG_CREATE | File::FLAG_WRITE).IsValid());
  }

  // Verify that the limit has been reached.
  EXPECT_EQ(GetUniquePath(some_file), base::FilePath());
}

TEST_F(FileUtilTest, GetUniquePathWithSuffixFormat) {
  const char kSuffix[] = "_%d";
  FilePath base_name(FPL("Unique_Base_Name.txt"));
  FilePath base_path = temp_dir_.GetPath().Append(base_name);
  EXPECT_FALSE(PathExists(base_path));

  // GetUniquePathWithSuffixFormat() should return unchanged path if file does
  // not exist.
  EXPECT_EQ(base_path, GetUniquePathWithSuffixFormat(base_path, kSuffix));

  // Create the file.
  {
    File file(base_path,
              File::FLAG_CREATE | File::FLAG_READ | File::FLAG_WRITE);
    EXPECT_TRUE(PathExists(base_path));
  }

  static const FilePath::CharType* const kExpectedNames[] = {
      FPL("Unique_Base_Name_1.txt"),
      FPL("Unique_Base_Name_2.txt"),
      FPL("Unique_Base_Name_3.txt"),
  };

  // Call GetUniquePathWithSuffixFormat() three times against this existing file
  // name.
  for (const FilePath::CharType* expected_name : kExpectedNames) {
    FilePath expected_path = temp_dir_.GetPath().Append(expected_name);
    FilePath path = GetUniquePathWithSuffixFormat(base_path, kSuffix);
    EXPECT_EQ(expected_path, path);

    // Verify that a file with this path indeed does not exist on the file
    // system.
    EXPECT_FALSE(PathExists(path));

    // Create the file so it exists for the next call to
    // GetUniquePathWithSuffixFormat() in the loop.
    File file(path, File::FLAG_CREATE | File::FLAG_READ | File::FLAG_WRITE);
    EXPECT_TRUE(PathExists(path));
  }

  // Verify that a different suffix still ends up with number 1.
  EXPECT_EQ(temp_dir_.GetPath().Append(FPL("Unique_Base_Name (1).txt")),
            GetUniquePathWithSuffixFormat(base_path, " (%d)"));
}

TEST_F(FileUtilTest, FileToFILE) {
  File file;
  FILE* stream = FileToFILE(std::move(file), "w");
  EXPECT_FALSE(stream);

  FilePath file_name = temp_dir_.GetPath().Append(FPL("The file.txt"));
  file = File(file_name, File::FLAG_CREATE | File::FLAG_WRITE);
  EXPECT_TRUE(file.IsValid());

  stream = FileToFILE(std::move(file), "w");
  EXPECT_TRUE(stream);
  EXPECT_FALSE(file.IsValid());
  EXPECT_TRUE(CloseFile(stream));
}

TEST_F(FileUtilTest, FILEToFile) {
  ScopedFILE stream;
  EXPECT_FALSE(FILEToFile(stream.get()).IsValid());

  stream.reset(OpenFile(temp_dir_.GetPath().Append(FPL("hello.txt")), "wb+"));
  ASSERT_TRUE(stream);
  File file = FILEToFile(stream.get());
  EXPECT_TRUE(file.IsValid());
  ASSERT_EQ(fprintf(stream.get(), "there"), 5);
  ASSERT_EQ(fflush(stream.get()), 0);
  EXPECT_EQ(file.GetLength(), 5L);
}

TEST_F(FileUtilTest, CreateNewTempDirectoryTest) {
  FilePath temp_dir;
  ASSERT_TRUE(CreateNewTempDirectory(FilePath::StringType(), &temp_dir));
  EXPECT_TRUE(PathExists(temp_dir));
  EXPECT_TRUE(DeleteFile(temp_dir));
}

#if BUILDFLAG(IS_WIN)
TEST_F(FileUtilTest, TempDirectoryParentTest) {
  if (!::IsUserAnAdmin()) {
    GTEST_SKIP() << "This test must be run by an admin user";
  }
  FilePath temp_dir;
  ASSERT_TRUE(CreateNewTempDirectory(FilePath::StringType(), &temp_dir));
  EXPECT_TRUE(PathExists(temp_dir));

  FilePath expected_parent_dir;
  if (!::IsUserAnAdmin() ||
      !PathService::Get(DIR_SYSTEM_TEMP, &expected_parent_dir)) {
    EXPECT_TRUE(PathService::Get(DIR_TEMP, &expected_parent_dir));
  }
  EXPECT_TRUE(expected_parent_dir.IsParent(temp_dir));
  EXPECT_TRUE(DeleteFile(temp_dir));
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(FileUtilTest, CreateNewTemporaryDirInDirTest) {
  FilePath new_dir;
  ASSERT_TRUE(CreateTemporaryDirInDir(
      temp_dir_.GetPath(), FILE_PATH_LITERAL("CreateNewTemporaryDirInDirTest"),
      &new_dir));
  EXPECT_TRUE(PathExists(new_dir));
  EXPECT_TRUE(temp_dir_.GetPath().IsParent(new_dir));
  EXPECT_TRUE(DeleteFile(new_dir));
}

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
TEST_F(FileUtilTest, GetShmemTempDirTest) {
  FilePath dir;
  EXPECT_TRUE(GetShmemTempDir(false, &dir));
  EXPECT_TRUE(DirectoryExists(dir));
}

TEST_F(FileUtilTest, AllocateFileRegionTest_ZeroOffset) {
  std::string_view test_data = "test_data";
  FilePath file_path = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("allocate_file_region_test_zero_offset"));
  WriteFile(file_path, test_data);

  File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                           base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  ASSERT_GE(file.GetLength(), 0);
  ASSERT_EQ(checked_cast<size_t>(file.GetLength()), test_data.size());

  const int kExtendedFileLength = 23;
  ASSERT_TRUE(AllocateFileRegion(&file, 0, kExtendedFileLength));
  EXPECT_EQ(file.GetLength(), kExtendedFileLength);

  char data_read[32] = {};
  int bytes_read = UNSAFE_TODO(file.Read(0, data_read, kExtendedFileLength));
  EXPECT_EQ(bytes_read, kExtendedFileLength);
  auto [front, back] = base::span(data_read).split_at(test_data.size());
  EXPECT_EQ(front, test_data);
  EXPECT_THAT(back, testing::Each('\0'));
}

TEST_F(FileUtilTest, AllocateFileRegionTest_NonZeroOffset) {
  std::string_view test_data = "test_data";
  FilePath file_path = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("allocate_file_region_test_non_zero_offset"));
  WriteFile(file_path, test_data);

  File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                           base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  ASSERT_GE(file.GetLength(), 0);
  ASSERT_EQ(checked_cast<size_t>(file.GetLength()), test_data.size());

  const int kExtensionOffset = 5;
  const int kExtensionSize = 10;
  ASSERT_TRUE(AllocateFileRegion(&file, kExtensionOffset, kExtensionSize));
  const int kExtendedFileLength = kExtensionOffset + kExtensionSize;
  EXPECT_EQ(file.GetLength(), kExtendedFileLength);

  char data_read[32] = {};
  int bytes_read = UNSAFE_TODO(file.Read(0, data_read, kExtendedFileLength));
  EXPECT_EQ(bytes_read, kExtendedFileLength);
  auto [front, back] = base::span(data_read).split_at(test_data.size());
  EXPECT_EQ(front, test_data);
  EXPECT_THAT(back, testing::Each('\0'));
}

TEST_F(FileUtilTest, AllocateFileRegionTest_DontTruncate) {
  std::string_view test_data = "test_data";
  FilePath file_path = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("allocate_file_region_test_dont_truncate"));
  WriteFile(file_path, test_data);

  File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                           base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  ASSERT_GE(file.GetLength(), 0);
  ASSERT_EQ(checked_cast<size_t>(file.GetLength()), test_data.size());

  const int kTruncatedFileLength = 4;
  ASSERT_TRUE(AllocateFileRegion(&file, 0, kTruncatedFileLength));
  ASSERT_GE(file.GetLength(), 0);
  EXPECT_EQ(checked_cast<size_t>(file.GetLength()), test_data.size());
}
#endif

TEST_F(FileUtilTest, GetHomeDirTest) {
#if !BUILDFLAG(IS_ANDROID)  // Not implemented on Android.
  // We don't actually know what the home directory is supposed to be without
  // calling some OS functions which would just duplicate the implementation.
  // So here we just test that it returns something "reasonable".
  FilePath home = GetHomeDir();
  ASSERT_FALSE(home.empty());
  ASSERT_TRUE(home.IsAbsolute());
#endif
}

TEST_F(FileUtilTest, CreateDirectoryTest) {
  FilePath test_root =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("create_directory_test"));
#if BUILDFLAG(IS_WIN)
  FilePath test_path =
      test_root.Append(FILE_PATH_LITERAL("dir\\tree\\likely\\doesnt\\exist\\"));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  FilePath test_path =
      test_root.Append(FILE_PATH_LITERAL("dir/tree/likely/doesnt/exist/"));
#endif

  EXPECT_FALSE(PathExists(test_path));
  EXPECT_TRUE(CreateDirectory(test_path));
  EXPECT_TRUE(PathExists(test_path));
  // CreateDirectory returns true if the DirectoryExists returns true.
  EXPECT_TRUE(CreateDirectory(test_path));

  // Doesn't work to create it on top of a non-dir
  test_path = test_path.Append(FILE_PATH_LITERAL("foobar.txt"));
  EXPECT_FALSE(PathExists(test_path));
  CreateTextFile(test_path, L"test file");
  EXPECT_TRUE(PathExists(test_path));
  EXPECT_FALSE(CreateDirectory(test_path));

  EXPECT_TRUE(DeletePathRecursively(test_root));
  EXPECT_FALSE(PathExists(test_root));
  EXPECT_FALSE(PathExists(test_path));

  // Verify assumptions made by the Windows implementation:
  // 1. The current directory always exists.
  // 2. The root directory always exists.
  ASSERT_TRUE(DirectoryExists(FilePath(FilePath::kCurrentDirectory)));
  FilePath top_level = test_root;
  while (top_level != top_level.DirName()) {
    top_level = top_level.DirName();
  }
  ASSERT_TRUE(DirectoryExists(top_level));

  // Given these assumptions hold, it should be safe to
  // test that "creating" these directories succeeds.
  EXPECT_TRUE(CreateDirectory(FilePath(FilePath::kCurrentDirectory)));
  EXPECT_TRUE(CreateDirectory(top_level));

#if BUILDFLAG(IS_WIN)
  FilePath invalid_drive(FILE_PATH_LITERAL("o:\\"));
  FilePath invalid_path =
      invalid_drive.Append(FILE_PATH_LITERAL("some\\inaccessible\\dir"));
  if (!PathExists(invalid_drive)) {
    EXPECT_FALSE(CreateDirectory(invalid_path));
  }
#endif
}

TEST_F(FileUtilTest, DetectDirectoryTest) {
  // Check a directory
  FilePath test_root =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("detect_directory_test"));
  EXPECT_FALSE(PathExists(test_root));
  EXPECT_TRUE(CreateDirectory(test_root));
  EXPECT_TRUE(PathExists(test_root));
  EXPECT_TRUE(DirectoryExists(test_root));
  // Check a file
  FilePath test_path = test_root.Append(FILE_PATH_LITERAL("foobar.txt"));
  EXPECT_FALSE(PathExists(test_path));
  CreateTextFile(test_path, L"test file");
  EXPECT_TRUE(PathExists(test_path));
  EXPECT_FALSE(DirectoryExists(test_path));
  EXPECT_TRUE(DeleteFile(test_path));

  EXPECT_TRUE(DeletePathRecursively(test_root));
}

TEST_F(FileUtilTest, FileEnumeratorTest) {
  // Test an empty directory.
  FileEnumerator f0(temp_dir_.GetPath(), true, FILES_AND_DIRECTORIES);
  EXPECT_EQ(FPL(""), f0.Next().value());
  EXPECT_EQ(FPL(""), f0.Next().value());

  // Test an empty directory, non-recursively, including "..".
  FileEnumerator f0_dotdot(
      temp_dir_.GetPath(), false,
      FILES_AND_DIRECTORIES | FileEnumerator::INCLUDE_DOT_DOT);
  EXPECT_EQ(temp_dir_.GetPath().Append(FPL("..")).value(),
            f0_dotdot.Next().value());
  EXPECT_EQ(FPL(""), f0_dotdot.Next().value());

  // create the directories
  FilePath dir1 = temp_dir_.GetPath().Append(FPL("dir1"));
  EXPECT_TRUE(CreateDirectory(dir1));
  FilePath dir2 = temp_dir_.GetPath().Append(FPL("dir2"));
  EXPECT_TRUE(CreateDirectory(dir2));
  FilePath dir2inner = dir2.Append(FPL("inner"));
  EXPECT_TRUE(CreateDirectory(dir2inner));

  // create the files
  FilePath dir2file = dir2.Append(FPL("dir2file.txt"));
  CreateTextFile(dir2file, std::wstring());
  FilePath dir2innerfile = dir2inner.Append(FPL("innerfile.txt"));
  CreateTextFile(dir2innerfile, std::wstring());
  FilePath file1 = temp_dir_.GetPath().Append(FPL("file1.txt"));
  CreateTextFile(file1, std::wstring());
  FilePath file2_rel =
      dir2.Append(FilePath::kParentDirectory).Append(FPL("file2.txt"));
  CreateTextFile(file2_rel, std::wstring());
  FilePath file2_abs = temp_dir_.GetPath().Append(FPL("file2.txt"));

  // Only enumerate files.
  FileEnumerator f1(temp_dir_.GetPath(), true, FileEnumerator::FILES);
  FindResultCollector c1(&f1);
  EXPECT_TRUE(c1.HasFile(file1));
  EXPECT_TRUE(c1.HasFile(file2_abs));
  EXPECT_TRUE(c1.HasFile(dir2file));
  EXPECT_TRUE(c1.HasFile(dir2innerfile));
  EXPECT_EQ(4, c1.size());

  // Only enumerate directories.
  FileEnumerator f2(temp_dir_.GetPath(), true, FileEnumerator::DIRECTORIES);
  FindResultCollector c2(&f2);
  EXPECT_TRUE(c2.HasFile(dir1));
  EXPECT_TRUE(c2.HasFile(dir2));
  EXPECT_TRUE(c2.HasFile(dir2inner));
  EXPECT_EQ(3, c2.size());

  // Only enumerate directories non-recursively.
  FileEnumerator f2_non_recursive(temp_dir_.GetPath(), false,
                                  FileEnumerator::DIRECTORIES);
  FindResultCollector c2_non_recursive(&f2_non_recursive);
  EXPECT_TRUE(c2_non_recursive.HasFile(dir1));
  EXPECT_TRUE(c2_non_recursive.HasFile(dir2));
  EXPECT_EQ(2, c2_non_recursive.size());

  // Only enumerate directories, non-recursively, including "..".
  FileEnumerator f2_dotdot(
      temp_dir_.GetPath(), false,
      FileEnumerator::DIRECTORIES | FileEnumerator::INCLUDE_DOT_DOT);
  FindResultCollector c2_dotdot(&f2_dotdot);
  EXPECT_TRUE(c2_dotdot.HasFile(dir1));
  EXPECT_TRUE(c2_dotdot.HasFile(dir2));
  EXPECT_TRUE(c2_dotdot.HasFile(temp_dir_.GetPath().Append(FPL(".."))));
  EXPECT_EQ(3, c2_dotdot.size());

  // Enumerate files and directories.
  FileEnumerator f3(temp_dir_.GetPath(), true, FILES_AND_DIRECTORIES);
  FindResultCollector c3(&f3);
  EXPECT_TRUE(c3.HasFile(dir1));
  EXPECT_TRUE(c3.HasFile(dir2));
  EXPECT_TRUE(c3.HasFile(file1));
  EXPECT_TRUE(c3.HasFile(file2_abs));
  EXPECT_TRUE(c3.HasFile(dir2file));
  EXPECT_TRUE(c3.HasFile(dir2inner));
  EXPECT_TRUE(c3.HasFile(dir2innerfile));
  EXPECT_EQ(7, c3.size());

  // Non-recursive operation.
  FileEnumerator f4(temp_dir_.GetPath(), false, FILES_AND_DIRECTORIES);
  FindResultCollector c4(&f4);
  EXPECT_TRUE(c4.HasFile(dir2));
  EXPECT_TRUE(c4.HasFile(dir2));
  EXPECT_TRUE(c4.HasFile(file1));
  EXPECT_TRUE(c4.HasFile(file2_abs));
  EXPECT_EQ(4, c4.size());

  // Enumerate with a pattern.
  FileEnumerator f5(temp_dir_.GetPath(), true, FILES_AND_DIRECTORIES,
                    FPL("dir*"));
  FindResultCollector c5(&f5);
  EXPECT_TRUE(c5.HasFile(dir1));
  EXPECT_TRUE(c5.HasFile(dir2));
  EXPECT_TRUE(c5.HasFile(dir2file));
  EXPECT_TRUE(c5.HasFile(dir2inner));
  EXPECT_TRUE(c5.HasFile(dir2innerfile));
  EXPECT_EQ(5, c5.size());

#if BUILDFLAG(IS_WIN)
  {
    // Make dir1 point to dir2.
    auto reparse_point = test::FilePathReparsePoint::Create(dir1, dir2);
    EXPECT_TRUE(reparse_point.has_value());

    // There can be a delay for the enumeration code to see the change on
    // the file system so skip this test for XP.
    // Enumerate the reparse point.
    FileEnumerator f6(dir1, true, FILES_AND_DIRECTORIES);
    FindResultCollector c6(&f6);
    FilePath inner2 = dir1.Append(FPL("inner"));
    EXPECT_TRUE(c6.HasFile(inner2));
    EXPECT_TRUE(c6.HasFile(inner2.Append(FPL("innerfile.txt"))));
    EXPECT_TRUE(c6.HasFile(dir1.Append(FPL("dir2file.txt"))));
    EXPECT_EQ(3, c6.size());

    // No changes for non recursive operation.
    FileEnumerator f7(temp_dir_.GetPath(), false, FILES_AND_DIRECTORIES);
    FindResultCollector c7(&f7);
    EXPECT_TRUE(c7.HasFile(dir2));
    EXPECT_TRUE(c7.HasFile(dir2));
    EXPECT_TRUE(c7.HasFile(file1));
    EXPECT_TRUE(c7.HasFile(file2_abs));
    EXPECT_EQ(4, c7.size());

    // Should not enumerate inside dir1 when using recursion.
    FileEnumerator f8(temp_dir_.GetPath(), true, FILES_AND_DIRECTORIES);
    FindResultCollector c8(&f8);
    EXPECT_TRUE(c8.HasFile(dir1));
    EXPECT_TRUE(c8.HasFile(dir2));
    EXPECT_TRUE(c8.HasFile(file1));
    EXPECT_TRUE(c8.HasFile(file2_abs));
    EXPECT_TRUE(c8.HasFile(dir2file));
    EXPECT_TRUE(c8.HasFile(dir2inner));
    EXPECT_TRUE(c8.HasFile(dir2innerfile));
    EXPECT_EQ(7, c8.size());
  }
#endif

  // Make sure the destructor closes the find handle while in the middle of a
  // query to allow TearDown to delete the directory.
  FileEnumerator f9(temp_dir_.GetPath(), true, FILES_AND_DIRECTORIES);
  EXPECT_FALSE(f9.Next().value().empty());  // Should have found something
                                            // (we don't care what).
}

TEST_F(FileUtilTest, AppendToFile) {
  FilePath data_dir =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("FilePathTest"));

  // Create a fresh, empty copy of this directory.
  if (PathExists(data_dir)) {
    ASSERT_TRUE(DeletePathRecursively(data_dir));
  }
  ASSERT_TRUE(CreateDirectory(data_dir));

  // Create a fresh, empty copy of this directory.
  if (PathExists(data_dir)) {
    ASSERT_TRUE(DeletePathRecursively(data_dir));
  }
  ASSERT_TRUE(CreateDirectory(data_dir));
  FilePath foobar(data_dir.Append(FILE_PATH_LITERAL("foobar.txt")));

  std::string data("hello");
  EXPECT_FALSE(AppendToFile(foobar, data));
  EXPECT_TRUE(WriteFile(foobar, data));
  EXPECT_TRUE(AppendToFile(foobar, data));

  const std::wstring read_content = ReadTextFile(foobar);
  EXPECT_EQ(L"hellohello", read_content);
}

TEST_F(FileUtilTest, ReadFile) {
  // Create a test file to be read.
  const std::string kTestData("The quick brown fox jumps over the lazy dog.");
  FilePath file_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("ReadFileTest"));

  ASSERT_TRUE(WriteFile(file_path, kTestData));

  // Make buffers with various size.
  std::vector<char> small_buffer(kTestData.size() / 2);
  std::vector<char> exact_buffer(kTestData.size());
  std::vector<char> large_buffer(kTestData.size() * 2);

  // Read the file with smaller buffer.
  EXPECT_EQ(ReadFile(file_path, small_buffer), small_buffer.size());
  EXPECT_EQ(kTestData.substr(0, small_buffer.size()),
            std::string(small_buffer.begin(), small_buffer.end()));

  // Read the file with buffer which have exactly same size.
  EXPECT_EQ(ReadFile(file_path, exact_buffer), kTestData.size());
  EXPECT_EQ(kTestData, std::string(exact_buffer.begin(), exact_buffer.end()));

  // Read the file with larger buffer.
  EXPECT_EQ(ReadFile(file_path, large_buffer), kTestData.size());
  EXPECT_EQ(kTestData, std::string(large_buffer.begin(),
                                   large_buffer.begin() + kTestData.size()));

  // Make sure the read fails if the file doesn't exist.
  FilePath file_path_not_exist =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("ReadFileNotExistTest"));
  EXPECT_EQ(ReadFile(file_path_not_exist, exact_buffer), std::nullopt);
}

TEST_F(FileUtilTest, ReadFileToBytes) {
  const std::vector<uint8_t> kTestData = {'0', '1', '2', '3'};

  FilePath file_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("ReadFileToStringTest"));
  FilePath file_path_dangerous =
      temp_dir_.GetPath()
          .Append(FILE_PATH_LITERAL(".."))
          .Append(temp_dir_.GetPath().BaseName())
          .Append(FILE_PATH_LITERAL("ReadFileToStringTest"));

  // Create test file.
  ASSERT_TRUE(WriteFile(file_path, kTestData));

  std::optional<std::vector<uint8_t>> bytes = ReadFileToBytes(file_path);
  ASSERT_TRUE(bytes.has_value());
  EXPECT_EQ(kTestData, bytes);

  // Write empty file.
  ASSERT_TRUE(WriteFile(file_path, ""));
  bytes = ReadFileToBytes(file_path);
  ASSERT_TRUE(bytes.has_value());
  EXPECT_TRUE(bytes->empty());

  ASSERT_FALSE(ReadFileToBytes(file_path_dangerous));
}

TEST_F(FileUtilTest, ReadFileToString) {
  const char kTestData[] = "0123";
  std::string data;

  FilePath file_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("ReadFileToStringTest"));
  FilePath file_path_dangerous =
      temp_dir_.GetPath()
          .Append(FILE_PATH_LITERAL(".."))
          .Append(temp_dir_.GetPath().BaseName())
          .Append(FILE_PATH_LITERAL("ReadFileToStringTest"));

  // Create test file.
  ASSERT_TRUE(WriteFile(file_path, kTestData));

  EXPECT_TRUE(ReadFileToString(file_path, &data));
  EXPECT_EQ(kTestData, data);

  data = "temp";
  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, &data, 0));
  EXPECT_EQ(0u, data.length());

  data = "temp";
  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, &data, 2));
  EXPECT_EQ("01", data);

  data = "temp";
  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, &data, 3));
  EXPECT_EQ("012", data);

  data = "temp";
  EXPECT_TRUE(ReadFileToStringWithMaxSize(file_path, &data, 4));
  EXPECT_EQ("0123", data);

  data = "temp";
  EXPECT_TRUE(ReadFileToStringWithMaxSize(file_path, &data, 6));
  EXPECT_EQ("0123", data);

  EXPECT_TRUE(ReadFileToStringWithMaxSize(file_path, nullptr, 6));

  EXPECT_TRUE(ReadFileToString(file_path, nullptr));

  data = "temp";
  EXPECT_FALSE(ReadFileToString(file_path_dangerous, &data));
  EXPECT_EQ(0u, data.length());

  // Delete test file.
  EXPECT_TRUE(DeleteFile(file_path));

  data = "temp";
  EXPECT_FALSE(ReadFileToString(file_path, &data));
  EXPECT_EQ(0u, data.length());

  data = "temp";
  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, &data, 6));
  EXPECT_EQ(0u, data.length());
}

#if !BUILDFLAG(IS_WIN)
TEST_F(FileUtilTest, ReadFileToStringWithUnknownFileSize) {
#if BUILDFLAG(IS_FUCHSIA)
  test::TaskEnvironment task_environment;
  auto dev_zero = ScopedDevZero::Get();
  ASSERT_TRUE(dev_zero);
#endif
  FilePath file_path("/dev/zero");
  std::string data = "temp";

  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, &data, 0));
  EXPECT_EQ(0u, data.length());

  data = "temp";
  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, &data, 2));
  EXPECT_EQ(std::string(2, '\0'), data);

  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, nullptr, 6));

  // Read more than buffer size.
  data = "temp";
  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, &data, kLargeFileSize));
  EXPECT_EQ(kLargeFileSize, data.length());
  EXPECT_EQ(std::string(kLargeFileSize, '\0'), data);

  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, nullptr, kLargeFileSize));
}
#endif  // !BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA) && \
    !BUILDFLAG(IS_IOS)
#define ChildMain WriteToPipeChildMain
#define ChildMainString "WriteToPipeChildMain"

MULTIPROCESS_TEST_MAIN(ChildMain) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const FilePath pipe_path = command_line->GetSwitchValuePath("pipe-path");

  int fd = open(pipe_path.value().c_str(), O_WRONLY);
  CHECK_NE(-1, fd);

  base::span<const char> to_write = base::span_from_cstring("0123");
  while (!to_write.empty()) {
    ssize_t res = write(fd, to_write.data(), to_write.size());
    if (res == -1) {
      break;
    }
    to_write = to_write.subspan(checked_cast<size_t>(res));
  }
  CHECK_EQ(to_write.size(), 0u);
  CHECK_EQ(0, close(fd));
  return 0;
}

#define MoreThanBufferSizeChildMain WriteToPipeMoreThanBufferSizeChildMain
#define MoreThanBufferSizeChildMainString \
  "WriteToPipeMoreThanBufferSizeChildMain"

MULTIPROCESS_TEST_MAIN(MoreThanBufferSizeChildMain) {
  std::string data(kLargeFileSize, 'c');
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const FilePath pipe_path = command_line->GetSwitchValuePath("pipe-path");

  int fd = open(pipe_path.value().c_str(), O_WRONLY);
  CHECK_NE(-1, fd);

  base::span<const char> to_write = base::span(data);
  while (!to_write.empty()) {
    ssize_t res = write(fd, to_write.data(), to_write.size());
    if (res == -1) {
      // We are unable to write because reading process has already read
      // requested number of bytes and closed pipe.
      break;
    }
    to_write = to_write.subspan(checked_cast<size_t>(res));
  }
  CHECK_EQ(0, close(fd));
  return 0;
}

TEST_F(FileUtilTest, ReadFileToStringWithNamedPipe) {
  FilePath pipe_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("test_pipe"));
  ASSERT_EQ(0, mkfifo(pipe_path.value().c_str(), 0600));

  CommandLine child_command_line(GetMultiProcessTestChildBaseCommandLine());
  child_command_line.AppendSwitchPath("pipe-path", pipe_path);

  {
    Process child_process = SpawnMultiProcessTestChild(
        ChildMainString, child_command_line, LaunchOptions());
    ASSERT_TRUE(child_process.IsValid());

    std::string data = "temp";
    EXPECT_FALSE(ReadFileToStringWithMaxSize(pipe_path, &data, 2));
    EXPECT_EQ("01", data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }
  {
    Process child_process = SpawnMultiProcessTestChild(
        ChildMainString, child_command_line, LaunchOptions());
    ASSERT_TRUE(child_process.IsValid());

    std::string data = "temp";
    EXPECT_TRUE(ReadFileToStringWithMaxSize(pipe_path, &data, 6));
    EXPECT_EQ("0123", data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }
  {
    Process child_process = SpawnMultiProcessTestChild(
        MoreThanBufferSizeChildMainString, child_command_line, LaunchOptions());
    ASSERT_TRUE(child_process.IsValid());

    std::string data = "temp";
    EXPECT_FALSE(ReadFileToStringWithMaxSize(pipe_path, &data, 6));
    EXPECT_EQ("cccccc", data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }
  {
    Process child_process = SpawnMultiProcessTestChild(
        MoreThanBufferSizeChildMainString, child_command_line, LaunchOptions());
    ASSERT_TRUE(child_process.IsValid());

    std::string data = "temp";
    EXPECT_FALSE(
        ReadFileToStringWithMaxSize(pipe_path, &data, kLargeFileSize - 1));
    EXPECT_EQ(std::string(kLargeFileSize - 1, 'c'), data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }
  {
    Process child_process = SpawnMultiProcessTestChild(
        MoreThanBufferSizeChildMainString, child_command_line, LaunchOptions());
    ASSERT_TRUE(child_process.IsValid());

    std::string data = "temp";
    EXPECT_TRUE(ReadFileToStringWithMaxSize(pipe_path, &data, kLargeFileSize));
    EXPECT_EQ(std::string(kLargeFileSize, 'c'), data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }
  {
    Process child_process = SpawnMultiProcessTestChild(
        MoreThanBufferSizeChildMainString, child_command_line, LaunchOptions());
    ASSERT_TRUE(child_process.IsValid());

    std::string data = "temp";
    EXPECT_TRUE(
        ReadFileToStringWithMaxSize(pipe_path, &data, kLargeFileSize * 5));
    EXPECT_EQ(std::string(kLargeFileSize, 'c'), data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }

  ASSERT_EQ(0, unlink(pipe_path.value().c_str()));
}
#endif  // !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_FUCHSIA)
        // && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN)
#define ChildMain WriteToPipeChildMain
#define ChildMainString "WriteToPipeChildMain"

MULTIPROCESS_TEST_MAIN(ChildMain) {
  const char kTestData[] = "0123";
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const FilePath pipe_path = command_line->GetSwitchValuePath("pipe-path");
  std::string switch_string = command_line->GetSwitchValueASCII("sync_event");
  EXPECT_FALSE(switch_string.empty());
  unsigned int switch_uint = 0;
  EXPECT_TRUE(StringToUint(switch_string, &switch_uint));
  win::ScopedHandle sync_event(win::Uint32ToHandle(switch_uint));

  HANDLE ph = CreateNamedPipe(pipe_path.value().c_str(), PIPE_ACCESS_OUTBOUND,
                              PIPE_WAIT, 1, 0, 0, 0, NULL);
  EXPECT_NE(ph, INVALID_HANDLE_VALUE);
  EXPECT_TRUE(SetEvent(sync_event.get()));
  if (!::ConnectNamedPipe(ph, /*lpOverlapped=*/nullptr)) {
    // ERROR_PIPE_CONNECTED means that the other side has already connected.
    auto error = ::GetLastError();
    EXPECT_EQ(error, DWORD{ERROR_PIPE_CONNECTED});
  }

  DWORD written;
  EXPECT_TRUE(::WriteFile(ph, kTestData, strlen(kTestData), &written, NULL));
  EXPECT_EQ(strlen(kTestData), written);
  CloseHandle(ph);
  return 0;
}

#define MoreThanBufferSizeChildMain WriteToPipeMoreThanBufferSizeChildMain
#define MoreThanBufferSizeChildMainString \
  "WriteToPipeMoreThanBufferSizeChildMain"

MULTIPROCESS_TEST_MAIN(MoreThanBufferSizeChildMain) {
  std::string data(kLargeFileSize, 'c');
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const FilePath pipe_path = command_line->GetSwitchValuePath("pipe-path");
  std::string switch_string = command_line->GetSwitchValueASCII("sync_event");
  EXPECT_FALSE(switch_string.empty());
  unsigned int switch_uint = 0;
  EXPECT_TRUE(StringToUint(switch_string, &switch_uint));
  win::ScopedHandle sync_event(win::Uint32ToHandle(switch_uint));

  HANDLE ph = CreateNamedPipe(pipe_path.value().c_str(), PIPE_ACCESS_OUTBOUND,
                              PIPE_WAIT, 1, data.size(), data.size(), 0, NULL);
  EXPECT_NE(ph, INVALID_HANDLE_VALUE);
  EXPECT_TRUE(SetEvent(sync_event.get()));
  if (!::ConnectNamedPipe(ph, /*lpOverlapped=*/nullptr)) {
    // ERROR_PIPE_CONNECTED means that the other side has already connected.
    auto error = ::GetLastError();
    EXPECT_EQ(error, DWORD{ERROR_PIPE_CONNECTED});
  }

  DWORD written;
  EXPECT_TRUE(::WriteFile(ph, data.c_str(), data.size(), &written, NULL));
  EXPECT_EQ(data.size(), written);
  CloseHandle(ph);
  return 0;
}

TEST_F(FileUtilTest, ReadFileToStringWithNamedPipe) {
  FilePath pipe_path(FILE_PATH_LITERAL("\\\\.\\pipe\\test_pipe"));
  win::ScopedHandle sync_event(CreateEvent(0, false, false, nullptr));

  CommandLine child_command_line(GetMultiProcessTestChildBaseCommandLine());
  child_command_line.AppendSwitchPath("pipe-path", pipe_path);
  child_command_line.AppendSwitchASCII(
      "sync_event", NumberToString(win::HandleToUint32(sync_event.get())));

  LaunchOptions options;
  options.handles_to_inherit.push_back(sync_event.get());

  {
    Process child_process = SpawnMultiProcessTestChild(
        ChildMainString, child_command_line, options);
    ASSERT_TRUE(child_process.IsValid());
    // Wait for pipe creation in child process.
    EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(sync_event.get(), INFINITE));

    std::string data = "temp";
    EXPECT_FALSE(ReadFileToStringWithMaxSize(pipe_path, &data, 2));
    EXPECT_EQ("01", data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }
  {
    Process child_process = SpawnMultiProcessTestChild(
        ChildMainString, child_command_line, options);
    ASSERT_TRUE(child_process.IsValid());
    // Wait for pipe creation in child process.
    EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(sync_event.get(), INFINITE));

    std::string data = "temp";
    EXPECT_TRUE(ReadFileToStringWithMaxSize(pipe_path, &data, 6));
    EXPECT_EQ("0123", data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }
  {
    Process child_process = SpawnMultiProcessTestChild(
        MoreThanBufferSizeChildMainString, child_command_line, options);
    ASSERT_TRUE(child_process.IsValid());
    // Wait for pipe creation in child process.
    EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(sync_event.get(), INFINITE));

    std::string data = "temp";
    EXPECT_FALSE(ReadFileToStringWithMaxSize(pipe_path, &data, 6));
    EXPECT_EQ("cccccc", data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }
  {
    Process child_process = SpawnMultiProcessTestChild(
        MoreThanBufferSizeChildMainString, child_command_line, options);
    ASSERT_TRUE(child_process.IsValid());
    // Wait for pipe creation in child process.
    EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(sync_event.get(), INFINITE));

    std::string data = "temp";
    EXPECT_FALSE(
        ReadFileToStringWithMaxSize(pipe_path, &data, kLargeFileSize - 1));
    EXPECT_EQ(std::string(kLargeFileSize - 1, 'c'), data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }
  {
    Process child_process = SpawnMultiProcessTestChild(
        MoreThanBufferSizeChildMainString, child_command_line, options);
    ASSERT_TRUE(child_process.IsValid());
    // Wait for pipe creation in child process.
    EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(sync_event.get(), INFINITE));

    std::string data = "temp";
    EXPECT_TRUE(ReadFileToStringWithMaxSize(pipe_path, &data, kLargeFileSize));
    EXPECT_EQ(std::string(kLargeFileSize, 'c'), data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }
  {
    Process child_process = SpawnMultiProcessTestChild(
        MoreThanBufferSizeChildMainString, child_command_line, options);
    ASSERT_TRUE(child_process.IsValid());
    // Wait for pipe creation in child process.
    EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(sync_event.get(), INFINITE));

    std::string data = "temp";
    EXPECT_TRUE(
        ReadFileToStringWithMaxSize(pipe_path, &data, kLargeFileSize * 5));
    EXPECT_EQ(std::string(kLargeFileSize, 'c'), data);

    int rv = -1;
    ASSERT_TRUE(WaitForMultiprocessTestChildExit(
        child_process, TestTimeouts::action_timeout(), &rv));
    ASSERT_EQ(0, rv);
  }
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
TEST_F(FileUtilTest, ReadFileToStringWithProcFileSystem) {
  FilePath file_path("/proc/cpuinfo");
  std::string data = "temp";

  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, &data, 0));
  EXPECT_EQ(0u, data.length());

  data = "temp";
  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, &data, 2));
  EXPECT_TRUE(EqualsCaseInsensitiveASCII("pr", data));

  data = "temp";
  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, &data, 4));
  EXPECT_TRUE(EqualsCaseInsensitiveASCII("proc", data));

  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, nullptr, 4));
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)

TEST_F(FileUtilTest, ReadFileToStringWithLargeFile) {
  std::string data(kLargeFileSize, 'c');

  FilePath file_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("ReadFileToStringTest"));

  // Create test file.
  ASSERT_TRUE(WriteFile(file_path, data));

  std::string actual_data = "temp";
  EXPECT_TRUE(ReadFileToString(file_path, &actual_data));
  EXPECT_EQ(data, actual_data);

  actual_data = "temp";
  EXPECT_FALSE(ReadFileToStringWithMaxSize(file_path, &actual_data, 0));
  EXPECT_EQ(0u, actual_data.length());

  // Read more than buffer size.
  actual_data = "temp";
  EXPECT_FALSE(
      ReadFileToStringWithMaxSize(file_path, &actual_data, kLargeFileSize - 1));
  EXPECT_EQ(std::string(kLargeFileSize - 1, 'c'), actual_data);
}

TEST_F(FileUtilTest, ReadStreamToString) {
  ScopedFILE stream(
      OpenFile(temp_dir_.GetPath().Append(FPL("hello.txt")), "wb+"));
  ASSERT_TRUE(stream);
  File file = FILEToFile(stream.get());
  ASSERT_TRUE(file.IsValid());
  ASSERT_EQ(fprintf(stream.get(), "there"), 5);
  ASSERT_EQ(fflush(stream.get()), 0);

  std::string contents;
  EXPECT_TRUE(ReadStreamToString(stream.get(), &contents));
  EXPECT_EQ(contents, std::string("there"));
}

#if BUILDFLAG(IS_POSIX)
TEST_F(FileUtilTest, ReadStreamToString_ZeroLengthFile) {
  Thread write_thread("write thread");
  ASSERT_TRUE(write_thread.Start());

  const size_t kSizes[] = {0, 1, 4095, 4096, 4097, 65535, 65536, 65537};

  for (size_t size : kSizes) {
    ScopedFD read_fd, write_fd;
    // Pipes have a length of zero when stat()'d.
    ASSERT_TRUE(CreatePipe(&read_fd, &write_fd, false /* non_blocking */));

    std::string random_data;
    if (size > 0) {
      random_data = RandBytesAsString(size);
    }
    EXPECT_EQ(size, random_data.size());
    write_thread.task_runner()->PostTask(
        FROM_HERE,
        BindLambdaForTesting([random_data, write_fd = std::move(write_fd)]() {
          ASSERT_TRUE(WriteFileDescriptor(write_fd.get(), random_data));
        }));

    ScopedFILE read_file(fdopen(read_fd.release(), "r"));
    ASSERT_TRUE(read_file);

    std::string contents;
    EXPECT_TRUE(ReadStreamToString(read_file.get(), &contents));
    EXPECT_EQ(contents, random_data);
  }
}
#endif

TEST_F(FileUtilTest, ReadStreamToStringWithMaxSize) {
  ScopedFILE stream(
      OpenFile(temp_dir_.GetPath().Append(FPL("hello.txt")), "wb+"));
  ASSERT_TRUE(stream);
  File file = FILEToFile(stream.get());
  ASSERT_TRUE(file.IsValid());
  ASSERT_EQ(fprintf(stream.get(), "there"), 5);
  ASSERT_EQ(fflush(stream.get()), 0);

  std::string contents;
  EXPECT_FALSE(ReadStreamToStringWithMaxSize(stream.get(), 2, &contents));
}

TEST_F(FileUtilTest, ReadStreamToStringNullStream) {
  std::string contents;
  EXPECT_FALSE(ReadStreamToString(nullptr, &contents));
}

TEST_F(FileUtilTest, TouchFile) {
  FilePath data_dir =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("FilePathTest"));

  // Create a fresh, empty copy of this directory.
  if (PathExists(data_dir)) {
    ASSERT_TRUE(DeletePathRecursively(data_dir));
  }
  ASSERT_TRUE(CreateDirectory(data_dir));

  FilePath foobar(data_dir.Append(FILE_PATH_LITERAL("foobar.txt")));
  std::string data("hello");
  ASSERT_TRUE(WriteFile(foobar, data));

  Time access_time;
  // This timestamp is divisible by one day (in local timezone),
  // to make it work on FAT too.
  ASSERT_TRUE(Time::FromString("Wed, 16 Nov 1994, 00:00:00", &access_time));

  Time modification_time;
  // Note that this timestamp is divisible by two (seconds) - FAT stores
  // modification times with 2s resolution.
  ASSERT_TRUE(
      Time::FromString("Tue, 15 Nov 1994, 12:45:26 GMT", &modification_time));

  ASSERT_TRUE(TouchFile(foobar, access_time, modification_time));
  File::Info file_info;
  ASSERT_TRUE(GetFileInfo(foobar, &file_info));
#if !BUILDFLAG(IS_FUCHSIA)
  // Access time is not supported on Fuchsia, see https://crbug.com/735233.
  EXPECT_EQ(access_time.ToInternalValue(),
            file_info.last_accessed.ToInternalValue());
#endif
  EXPECT_EQ(modification_time.ToInternalValue(),
            file_info.last_modified.ToInternalValue());
}

TEST_F(FileUtilTest, WriteFileSpanVariant) {
  FilePath empty_file =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("empty_file"));
  ASSERT_FALSE(PathExists(empty_file));
  EXPECT_TRUE(WriteFile(empty_file, base::span<const uint8_t>()));
  EXPECT_TRUE(PathExists(empty_file));

  std::string data = "not empty";
  EXPECT_TRUE(ReadFileToString(empty_file, &data));
  EXPECT_TRUE(data.empty());

  FilePath write_span_file =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("write_span_file"));
  ASSERT_FALSE(PathExists(write_span_file));
  static constexpr uint8_t kInput[] = {'h', 'e', 'l', 'l', 'o'};
  EXPECT_TRUE(WriteFile(write_span_file, kInput));
  EXPECT_TRUE(PathExists(write_span_file));

  data.clear();
  EXPECT_TRUE(ReadFileToString(write_span_file, &data));
  EXPECT_EQ("hello", data);
}

TEST_F(FileUtilTest, WriteFileStringVariant) {
  FilePath empty_file =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("empty_file"));
  ASSERT_FALSE(PathExists(empty_file));
  EXPECT_TRUE(WriteFile(empty_file, ""));
  EXPECT_TRUE(PathExists(empty_file));

  std::string data = "not empty";
  EXPECT_TRUE(ReadFileToString(empty_file, &data));
  EXPECT_TRUE(data.empty());

  FilePath write_span_file =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("write_string_file"));
  ASSERT_FALSE(PathExists(write_span_file));
  EXPECT_TRUE(WriteFile(write_span_file, "world"));
  EXPECT_TRUE(PathExists(write_span_file));

  data.clear();
  EXPECT_TRUE(ReadFileToString(write_span_file, &data));
  EXPECT_EQ("world", data);
}

TEST_F(FileUtilTest, IsDirectoryEmpty) {
  FilePath empty_dir =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("EmptyDir"));

  ASSERT_FALSE(PathExists(empty_dir));

  ASSERT_TRUE(CreateDirectory(empty_dir));

  EXPECT_TRUE(IsDirectoryEmpty(empty_dir));

  FilePath foo(empty_dir.Append(FILE_PATH_LITERAL("foo.txt")));
  std::string bar("baz");
  ASSERT_TRUE(WriteFile(foo, bar));

  EXPECT_FALSE(IsDirectoryEmpty(empty_dir));
}

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

TEST_F(FileUtilTest, SetNonBlocking) {
  const int kBogusFd = 99999;
  EXPECT_FALSE(SetNonBlocking(kBogusFd));

  FilePath path;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &path));
  path = path.Append(FPL("file_util")).Append(FPL("original.txt"));
  ScopedFD fd(open(path.value().c_str(), O_RDONLY));
  ASSERT_GE(fd.get(), 0);
  EXPECT_TRUE(SetNonBlocking(fd.get()));
}

TEST_F(FileUtilTest, SetCloseOnExec) {
  const int kBogusFd = 99999;
  EXPECT_FALSE(SetCloseOnExec(kBogusFd));

  FilePath path;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &path));
  path = path.Append(FPL("file_util")).Append(FPL("original.txt"));
  ScopedFD fd(open(path.value().c_str(), O_RDONLY));
  ASSERT_GE(fd.get(), 0);
  EXPECT_TRUE(SetCloseOnExec(fd.get()));
}

#endif

#if BUILDFLAG(IS_MAC)

// Testing VerifyPathControlledByAdmin() is hard, because there is no
// way a test can make a file owned by root, or change file paths
// at the root of the file system.  VerifyPathControlledByAdmin()
// is implemented as a call to VerifyPathControlledByUser, which gives
// us the ability to test with paths under the test's temp directory,
// using a user id we control.
// Pull tests of VerifyPathControlledByUserTest() into a separate test class
// with a common SetUp() method.
class VerifyPathControlledByUserTest : public FileUtilTest {
 protected:
  void SetUp() override {
    FileUtilTest::SetUp();

    // Create a basic structure used by each test.
    // base_dir_
    //  |-> sub_dir_
    //       |-> text_file_

    base_dir_ = temp_dir_.GetPath().AppendASCII("base_dir");
    ASSERT_TRUE(CreateDirectory(base_dir_));

    sub_dir_ = base_dir_.AppendASCII("sub_dir");
    ASSERT_TRUE(CreateDirectory(sub_dir_));

    text_file_ = sub_dir_.AppendASCII("file.txt");
    CreateTextFile(text_file_, L"This text file has some text in it.");

    // Get the user and group files are created with from |base_dir_|.
    stat_wrapper_t stat_buf;
    ASSERT_EQ(0, File::Stat(base_dir_, &stat_buf));
    uid_ = stat_buf.st_uid;
    ok_gids_.insert(stat_buf.st_gid);
    bad_gids_.insert(stat_buf.st_gid + 1);

    ASSERT_EQ(uid_, getuid());  // This process should be the owner.

    // To ensure that umask settings do not cause the initial state
    // of permissions to be different from what we expect, explicitly
    // set permissions on the directories we create.
    // Make all files and directories non-world-writable.

    // Users and group can read, write, traverse
    int enabled_permissions =
        FILE_PERMISSION_USER_MASK | FILE_PERMISSION_GROUP_MASK;
    // Other users can't read, write, traverse
    int disabled_permissions = FILE_PERMISSION_OTHERS_MASK;

    ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(
        base_dir_, enabled_permissions, disabled_permissions));
    ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(
        sub_dir_, enabled_permissions, disabled_permissions));
  }

  FilePath base_dir_;
  FilePath sub_dir_;
  FilePath text_file_;
  uid_t uid_;

  std::set<gid_t> ok_gids_;
  std::set<gid_t> bad_gids_;
};

TEST_F(VerifyPathControlledByUserTest, BadPaths) {
  // File does not exist.
  FilePath does_not_exist =
      base_dir_.AppendASCII("does").AppendASCII("not").AppendASCII("exist");
  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, does_not_exist, uid_, ok_gids_));

  // |base| not a subpath of |path|.
  EXPECT_FALSE(VerifyPathControlledByUser(sub_dir_, base_dir_, uid_, ok_gids_));

  // An empty base path will fail to be a prefix for any path.
  FilePath empty;
  EXPECT_FALSE(VerifyPathControlledByUser(empty, base_dir_, uid_, ok_gids_));

  // Finding that a bad call fails proves nothing unless a good call succeeds.
  EXPECT_TRUE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, ok_gids_));
}

TEST_F(VerifyPathControlledByUserTest, Symlinks) {
  // Symlinks in the path should cause failure.

  // Symlink to the file at the end of the path.
  FilePath file_link = base_dir_.AppendASCII("file_link");
  ASSERT_TRUE(CreateSymbolicLink(text_file_, file_link))
      << "Failed to create symlink.";

  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, file_link, uid_, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(file_link, file_link, uid_, ok_gids_));

  // Symlink from one directory to another within the path.
  FilePath link_to_sub_dir = base_dir_.AppendASCII("link_to_sub_dir");
  ASSERT_TRUE(CreateSymbolicLink(sub_dir_, link_to_sub_dir))
      << "Failed to create symlink.";

  FilePath file_path_with_link = link_to_sub_dir.AppendASCII("file.txt");
  ASSERT_TRUE(PathExists(file_path_with_link));

  EXPECT_FALSE(VerifyPathControlledByUser(base_dir_, file_path_with_link, uid_,
                                          ok_gids_));

  EXPECT_FALSE(VerifyPathControlledByUser(link_to_sub_dir, file_path_with_link,
                                          uid_, ok_gids_));

  // Symlinks in parents of base path are allowed.
  EXPECT_TRUE(VerifyPathControlledByUser(file_path_with_link,
                                         file_path_with_link, uid_, ok_gids_));
}

TEST_F(VerifyPathControlledByUserTest, OwnershipChecks) {
  // Get a uid that is not the uid of files we create.
  uid_t bad_uid = uid_ + 1;

  // Make all files and directories non-world-writable.
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(base_dir_, 0u, S_IWOTH));
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(sub_dir_, 0u, S_IWOTH));
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(text_file_, 0u, S_IWOTH));

  // We control these paths.
  EXPECT_TRUE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_TRUE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(VerifyPathControlledByUser(sub_dir_, text_file_, uid_, ok_gids_));

  // Another user does not control these paths.
  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, sub_dir_, bad_uid, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, text_file_, bad_uid, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(sub_dir_, text_file_, bad_uid, ok_gids_));

  // Another group does not control the paths.
  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, bad_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, bad_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(sub_dir_, text_file_, uid_, bad_gids_));
}

TEST_F(VerifyPathControlledByUserTest, GroupWriteTest) {
  // Make all files and directories writable only by their owner.
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(base_dir_, 0u, S_IWOTH | S_IWGRP));
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(sub_dir_, 0u, S_IWOTH | S_IWGRP));
  ASSERT_NO_FATAL_FAILURE(
      ChangePosixFilePermissions(text_file_, 0u, S_IWOTH | S_IWGRP));

  // Any group is okay because the path is not group-writable.
  EXPECT_TRUE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_TRUE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(VerifyPathControlledByUser(sub_dir_, text_file_, uid_, ok_gids_));

  EXPECT_TRUE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, bad_gids_));
  EXPECT_TRUE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, bad_gids_));
  EXPECT_TRUE(
      VerifyPathControlledByUser(sub_dir_, text_file_, uid_, bad_gids_));

  // No group is okay, because we don't check the group
  // if no group can write.
  std::set<gid_t> no_gids;  // Empty set of gids.
  EXPECT_TRUE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, no_gids));
  EXPECT_TRUE(VerifyPathControlledByUser(base_dir_, text_file_, uid_, no_gids));
  EXPECT_TRUE(VerifyPathControlledByUser(sub_dir_, text_file_, uid_, no_gids));

  // Make all files and directories writable by their group.
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(base_dir_, S_IWGRP, 0u));
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(sub_dir_, S_IWGRP, 0u));
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(text_file_, S_IWGRP, 0u));

  // Now |ok_gids_| works, but |bad_gids_| fails.
  EXPECT_TRUE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_TRUE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(VerifyPathControlledByUser(sub_dir_, text_file_, uid_, ok_gids_));

  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, bad_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, bad_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(sub_dir_, text_file_, uid_, bad_gids_));

  // Because any group in the group set is allowed,
  // the union of good and bad gids passes.

  std::set<gid_t> multiple_gids;
  std::set_union(ok_gids_.begin(), ok_gids_.end(), bad_gids_.begin(),
                 bad_gids_.end(),
                 std::inserter(multiple_gids, multiple_gids.begin()));

  EXPECT_TRUE(
      VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, multiple_gids));
  EXPECT_TRUE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, multiple_gids));
  EXPECT_TRUE(
      VerifyPathControlledByUser(sub_dir_, text_file_, uid_, multiple_gids));
}

TEST_F(VerifyPathControlledByUserTest, WriteBitChecks) {
  // Make all files and directories non-world-writable.
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(base_dir_, 0u, S_IWOTH));
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(sub_dir_, 0u, S_IWOTH));
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(text_file_, 0u, S_IWOTH));

  // Initialy, we control all parts of the path.
  EXPECT_TRUE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_TRUE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(VerifyPathControlledByUser(sub_dir_, text_file_, uid_, ok_gids_));

  // Make base_dir_ world-writable.
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(base_dir_, S_IWOTH, 0u));
  EXPECT_FALSE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(VerifyPathControlledByUser(sub_dir_, text_file_, uid_, ok_gids_));

  // Make sub_dir_ world writable.
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(sub_dir_, S_IWOTH, 0u));
  EXPECT_FALSE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(sub_dir_, text_file_, uid_, ok_gids_));

  // Make text_file_ world writable.
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(text_file_, S_IWOTH, 0u));
  EXPECT_FALSE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(sub_dir_, text_file_, uid_, ok_gids_));

  // Make sub_dir_ non-world writable.
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(sub_dir_, 0u, S_IWOTH));
  EXPECT_FALSE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(sub_dir_, text_file_, uid_, ok_gids_));

  // Make base_dir_ non-world-writable.
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(base_dir_, 0u, S_IWOTH));
  EXPECT_TRUE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_FALSE(
      VerifyPathControlledByUser(sub_dir_, text_file_, uid_, ok_gids_));

  // Back to the initial state: Nothing is writable, so every path
  // should pass.
  ASSERT_NO_FATAL_FAILURE(ChangePosixFilePermissions(text_file_, 0u, S_IWOTH));
  EXPECT_TRUE(VerifyPathControlledByUser(base_dir_, sub_dir_, uid_, ok_gids_));
  EXPECT_TRUE(
      VerifyPathControlledByUser(base_dir_, text_file_, uid_, ok_gids_));
  EXPECT_TRUE(VerifyPathControlledByUser(sub_dir_, text_file_, uid_, ok_gids_));
}

#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
TEST_F(FileUtilTest, ValidContentUriTest) {
  // Get the test image path.
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &data_dir));
  data_dir = data_dir.AppendASCII("file_util");
  ASSERT_TRUE(PathExists(data_dir));
  FilePath image_file = data_dir.Append(FILE_PATH_LITERAL("red.png"));
  int64_t image_size;
  GetFileSize(image_file, &image_size);
  ASSERT_GT(image_size, 0);

  // Insert the image into MediaStore. MediaStore will do some conversions, and
  // return the content URI.
  FilePath path = InsertImageIntoMediaStore(image_file);
  EXPECT_TRUE(path.IsContentUri());
  EXPECT_TRUE(PathExists(path));
  // The file size may not equal to the input image as MediaStore may convert
  // the image.
  int64_t content_uri_size;
  GetFileSize(path, &content_uri_size);
  EXPECT_EQ(image_size, content_uri_size);

  // We should be able to read the file.
  File file(path, File::FLAG_OPEN | File::FLAG_READ);
  EXPECT_TRUE(file.IsValid());
  auto buffer = std::make_unique<char[]>(image_size);
  // SAFETY: required for test.
  EXPECT_TRUE(UNSAFE_BUFFERS(file.ReadAtCurrentPos(buffer.get(), image_size)));
}

TEST_F(FileUtilTest, WriteContentUri) {
  // `path` and `content_uri` are the same file.
  FilePath path = temp_dir_.GetPath().Append("file.txt");
  ASSERT_TRUE(WriteFile(path, "file-content"));
  FilePath content_uri =
      *test::android::GetContentUriFromCacheDirFilePath(path);

  // We should be able to open the file as writable which truncates the file.
  File file = File(content_uri, File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE);
  EXPECT_TRUE(file.IsValid());
  int64_t size;
  GetFileSize(path, &size);
  EXPECT_EQ(size, 0);

  EXPECT_EQ(*file.WriteAtCurrentPos(byte_span_from_cstring("123")), 3u);
  EXPECT_TRUE(file.Flush());
  GetFileSize(path, &size);
  EXPECT_EQ(size, 3);
}

TEST_F(FileUtilTest, NonExistentContentUriTest) {
  FilePath path("content://foo.bar");
  EXPECT_TRUE(path.IsContentUri());
  EXPECT_FALSE(PathExists(path));
  // Size should be smaller than 0.
  int64_t size;
  EXPECT_FALSE(GetFileSize(path, &size));

  // We should not be able to read the file.
  File file(path, File::FLAG_OPEN | File::FLAG_READ);
  EXPECT_FALSE(file.IsValid());
}
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    defined(ARCH_CPU_32_BITS)
// TODO(crbug.com/327582285): Re-enable these tests. They may be failing due to
// prefetching failing under memory pressure.
#define FLAKY_327582285 1
#endif

#if defined(FLAKY_327582285)
#define MAYBE_PreReadFileExistingFileNoSize \
  DISABLED_PreReadFileExistingFileNoSize
#else
#define MAYBE_PreReadFileExistingFileNoSize PreReadFileExistingFileNoSize
#endif
TEST_F(FileUtilTest, MAYBE_PreReadFileExistingFileNoSize) {
  FilePath text_file = temp_dir_.GetPath().Append(FPL("text_file"));
  CreateTextFile(text_file, bogus_content);

  EXPECT_TRUE(
      PreReadFile(text_file, /*is_executable=*/false, /*sequential=*/false));
}

#if defined(FLAKY_327582285)
#define MAYBE_PreReadFileExistingFileExactSize \
  DISABLED_PreReadFileExistingFileExactSize
#else
#define MAYBE_PreReadFileExistingFileExactSize PreReadFileExistingFileExactSize
#endif
TEST_F(FileUtilTest, MAYBE_PreReadFileExistingFileExactSize) {
  FilePath text_file = temp_dir_.GetPath().Append(FPL("text_file"));
  CreateTextFile(text_file, bogus_content);

  EXPECT_TRUE(PreReadFile(text_file, /*is_executable=*/false,
                          /*sequential=*/false, std::size(bogus_content)));
}

#if defined(FLAKY_327582285)
#define MAYBE_PreReadFileExistingFileOverSized \
  DISABLED_PreReadFileExistingFileOverSized
#else
#define MAYBE_PreReadFileExistingFileOverSized PreReadFileExistingFileOverSized
#endif
TEST_F(FileUtilTest, MAYBE_PreReadFileExistingFileOverSized) {
  FilePath text_file = temp_dir_.GetPath().Append(FPL("text_file"));
  CreateTextFile(text_file, bogus_content);

  EXPECT_TRUE(PreReadFile(text_file, /*is_executable=*/false,
                          /*sequential=*/false, std::size(bogus_content) * 2));
}

#if defined(FLAKY_327582285)
#define MAYBE_PreReadFileExistingFileUnderSized \
  DISABLED_PreReadFileExistingFileUnderSized
#else
#define MAYBE_PreReadFileExistingFileUnderSized \
  PreReadFileExistingFileUnderSized
#endif
TEST_F(FileUtilTest, MAYBE_PreReadFileExistingFileUnderSized) {
  FilePath text_file = temp_dir_.GetPath().Append(FPL("text_file"));
  CreateTextFile(text_file, bogus_content);

  EXPECT_TRUE(PreReadFile(text_file, /*is_executable=*/false,
                          /*sequential=*/false, std::size(bogus_content) / 2));
}

TEST_F(FileUtilTest, PreReadFileExistingFileZeroSize) {
  FilePath text_file = temp_dir_.GetPath().Append(FPL("text_file"));
  CreateTextFile(text_file, bogus_content);

  EXPECT_TRUE(PreReadFile(text_file, /*is_executable=*/false,
                          /*sequential=*/false, /*max_bytes=*/0));
}

TEST_F(FileUtilTest, PreReadFileExistingEmptyFileNoSize) {
  FilePath text_file = temp_dir_.GetPath().Append(FPL("text_file"));
  CreateTextFile(text_file, L"");
  // The test just asserts that this doesn't crash. The Windows implementation
  // fails in this case, due to the base::MemoryMappedFile implementation and
  // the limitations of ::MapViewOfFile().
  PreReadFile(text_file, /*is_executable=*/false, /*sequential=*/false);
}

TEST_F(FileUtilTest, PreReadFileExistingEmptyFileZeroSize) {
  FilePath text_file = temp_dir_.GetPath().Append(FPL("text_file"));
  CreateTextFile(text_file, L"");
  EXPECT_TRUE(PreReadFile(text_file, /*is_executable=*/false,
                          /*sequential=*/false, /*max_bytes=*/0));
}

TEST_F(FileUtilTest, PreReadFileInexistentFile) {
  FilePath inexistent_file = temp_dir_.GetPath().Append(FPL("inexistent_file"));
  EXPECT_FALSE(PreReadFile(inexistent_file, /*is_executable=*/false,
                           /*sequential=*/false));
}

#if defined(FLAKY_327582285)
#define MAYBE_PreReadFileExecutable DISABLED_PreReadFileExecutable
#else
#define MAYBE_PreReadFileExecutable PreReadFileExecutable
#endif
TEST_F(FileUtilTest, MAYBE_PreReadFileExecutable) {
  FilePath exe_data_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &exe_data_dir));
  exe_data_dir = exe_data_dir.Append(FPL("pe_image_reader"));
  ASSERT_TRUE(PathExists(exe_data_dir));

  // Load a sample executable and confirm that it was successfully prefetched.
  // `test_exe` is a Windows binary, which is fine in this case because only the
  // Windows implementation treats binaries differently from other files.
  const FilePath test_exe = exe_data_dir.Append(FPL("signed.exe"));
  EXPECT_TRUE(
      PreReadFile(test_exe, /*is_executable=*/true, /*sequential=*/false));
}

#if defined(FLAKY_327582285)
#define MAYBE_PreReadFileWithSequentialAccess \
  DISABLED_PreReadFileWithSequentialAccess
#else
#define MAYBE_PreReadFileWithSequentialAccess PreReadFileWithSequentialAccess
#endif
TEST_F(FileUtilTest, MAYBE_PreReadFileWithSequentialAccess) {
  FilePath text_file = temp_dir_.GetPath().Append(FPL("text_file"));
  CreateTextFile(text_file, bogus_content);

  EXPECT_TRUE(
      PreReadFile(text_file, /*is_executable=*/false, /*sequential=*/true));
}

#undef FLAKY_327582285

// Test that temp files obtained racily are all unique (no interference between
// threads). Mimics file operations in DoLaunchChildTestProcess() to rule out
// thread-safety issues @ https://crbug.com/826408#c17.
TEST(FileUtilMultiThreadedTest, MultiThreadedTempFiles) {
#if BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40577019): Too slow to run on infra due to QEMU overhead.
  constexpr int kNumThreads = 8;
#else
  constexpr int kNumThreads = 64;
#endif
  constexpr int kNumWritesPerThread = 32;

  std::unique_ptr<Thread> threads[kNumThreads];
  for (auto& thread : threads) {
    thread = std::make_unique<Thread>("test worker");
    thread->Start();
  }

  // Wait until all threads are started for max parallelism.
  for (auto& thread : threads) {
    thread->WaitUntilThreadStarted();
  }

  const RepeatingClosure open_write_close_read = BindRepeating([] {
    FilePath output_filename;
    ScopedFILE output_file(CreateAndOpenTemporaryStream(&output_filename));
    EXPECT_TRUE(output_file);

    const std::string content = Uuid::GenerateRandomV4().AsLowercaseString();
#if BUILDFLAG(IS_WIN)
    HANDLE handle =
        reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(output_file.get())));
    DWORD bytes_written = 0;
    ::WriteFile(handle, content.c_str(), content.length(), &bytes_written,
                NULL);
#else
    size_t bytes_written =
        ::write(::fileno(output_file.get()), content.c_str(), content.length());
#endif
    EXPECT_EQ(content.length(), bytes_written);
    ::fflush(output_file.get());
    output_file.reset();

    std::string output_file_contents;
    EXPECT_TRUE(ReadFileToString(output_filename, &output_file_contents))
        << output_filename;

    EXPECT_EQ(content, output_file_contents);

    DeleteFile(output_filename);
  });

  // Post tasks to each thread in a round-robin fashion to ensure as much
  // parallelism as possible.
  for (int i = 0; i < kNumWritesPerThread; ++i) {
    for (auto& thread : threads) {
      thread->task_runner()->PostTask(FROM_HERE, open_write_close_read);
    }
  }

  for (auto& thread : threads) {
    thread->Stop();
  }
}

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

TEST(ScopedFD, ScopedFDDoesClose) {
  int fds[2];
  char c = 0;
  ASSERT_EQ(0, pipe(fds));
  const int write_end = fds[1];
  ScopedFD read_end_closer(fds[0]);
  { ScopedFD write_end_closer(fds[1]); }
  // This is the only thread. This file descriptor should no longer be valid.
  int ret = close(write_end);
  EXPECT_EQ(-1, ret);
  EXPECT_EQ(EBADF, errno);
  // Make sure read(2) won't block.
  ASSERT_EQ(0, fcntl(fds[0], F_SETFL, O_NONBLOCK));
  // Reading the pipe should EOF.
  EXPECT_EQ(0, read(fds[0], &c, 1));
}

#if defined(GTEST_HAS_DEATH_TEST)
void CloseWithScopedFD(int fd) {
  ScopedFD fd_closer(fd);
}
#endif

TEST(ScopedFD, ScopedFDCrashesOnCloseFailure) {
  int fds[2];
  ASSERT_EQ(0, pipe(fds));
  ScopedFD read_end_closer(fds[0]);
  EXPECT_EQ(0, IGNORE_EINTR(close(fds[1])));
#if defined(GTEST_HAS_DEATH_TEST)
  // This is the only thread. This file descriptor should no longer be valid.
  // Trying to close it should crash. This is important for security.
  EXPECT_DEATH(CloseWithScopedFD(fds[1]), "");
#endif
}

#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
TEST_F(FileUtilTest, CopyFileContentsWithSendfile) {
  // This test validates that sendfile(2) can be used to copy a file contents
  // and that it will honor the file offsets as CopyFileContents does.
  FilePath file_name_from = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("copy_contents_file_in.txt"));
  FilePath file_name_to = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("copy_contents_file_out.txt"));

  const std::wstring from_contents(L"0123456789ABCDEF");
  CreateTextFile(file_name_from, from_contents);
  ASSERT_TRUE(PathExists(file_name_from));

  const std::wstring to_contents(L"GHIJKL");
  CreateTextFile(file_name_to, to_contents);
  ASSERT_TRUE(PathExists(file_name_to));

  File from(file_name_from, File::FLAG_OPEN | File::FLAG_READ);
  ASSERT_TRUE(from.IsValid());

  File to(file_name_to, File::FLAG_OPEN | File::FLAG_WRITE);
  ASSERT_TRUE(to.IsValid());

  // See to the 1st byte in each file.
  ASSERT_EQ(from.Seek(File::Whence::FROM_BEGIN, 1), 1);
  ASSERT_EQ(to.Seek(File::Whence::FROM_BEGIN, 1), 1);

  bool retry_slow = false;

  // Given the test setup there should never be a sendfile(2) failure.
  ASSERT_TRUE(internal::CopyFileContentsWithSendfile(from, to, retry_slow));
  from.Close();
  to.Close();

  // Expect the output file contents to be: G123456789ABCDEF because both
  // file positions when we copied the file contents were at 1.
  EXPECT_EQ(L"G123456789ABCDEF", ReadTextFile(file_name_to));
}

TEST_F(FileUtilTest, CopyFileContentsWithSendfileEmpty) {
  FilePath file_name_from = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("copy_contents_file_in.txt"));
  FilePath file_name_to = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("copy_contents_file_out.txt"));

  const std::wstring from_contents(L"");
  CreateTextFile(file_name_from, from_contents);
  ASSERT_TRUE(PathExists(file_name_from));

  const std::wstring to_contents(L"");
  CreateTextFile(file_name_to, to_contents);
  ASSERT_TRUE(PathExists(file_name_to));

  File from(file_name_from, File::FLAG_OPEN | File::FLAG_READ);
  ASSERT_TRUE(from.IsValid());

  File to(file_name_to, File::FLAG_OPEN | File::FLAG_WRITE);
  ASSERT_TRUE(to.IsValid());

  bool retry_slow = false;

  ASSERT_FALSE(internal::CopyFileContentsWithSendfile(from, to, retry_slow));
  ASSERT_TRUE(retry_slow);

  from.Close();
  to.Close();

  EXPECT_EQ(L"", ReadTextFile(file_name_to));
}

TEST_F(FileUtilTest, CopyFileContentsWithSendfilePipe) {
  FilePath file_name_to = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("copy_contents_file_out.txt"));

  File to(file_name_to,
          File::FLAG_OPEN | File::FLAG_WRITE | File::FLAG_CREATE_ALWAYS);
  ASSERT_TRUE(to.IsValid());

  // This test validates that CopyFileContentsWithSendfile fails with a pipe and
  // retry_slow is set.
  int fd[2];
  ASSERT_EQ(pipe2(fd, O_CLOEXEC), 0);

  // For good measure write some data into the pipe.
  const char* buf = "hello world";
  ASSERT_EQ(write(fd[1], buf, sizeof(buf)), static_cast<int>(sizeof(buf)));

  // fd[0] refers to the read end of the pipe.
  bool retry_slow = false;
  base::PlatformFile pipe_read_end(fd[0]);
  base::File pipe_read(pipe_read_end);
  ASSERT_FALSE(
      internal::CopyFileContentsWithSendfile(pipe_read, to, retry_slow));
  ASSERT_TRUE(retry_slow);
}

TEST_F(FileUtilTest, CopyFileContentsWithSendfileSocket) {
  // This test validates that CopyFileContentsWithSendfile fails with a socket
  // and retry_slow is set.
  int sock[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sock), 0);

  FilePath file_name_from = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("copy_contents_file_in.txt"));
  FilePath file_name_to = temp_dir_.GetPath().Append(
      FILE_PATH_LITERAL("copy_contents_file_out.txt"));
  const std::wstring from_contents(L"0123456789ABCDEF");
  CreateTextFile(file_name_from, from_contents);
  ASSERT_TRUE(PathExists(file_name_from));

  File from(file_name_from, File::FLAG_OPEN | File::FLAG_READ);
  ASSERT_TRUE(from.IsValid());

  base::PlatformFile to_file(sock[0]);
  base::File to_sock(to_file);

  // Copying from a file to a socket will work.
  bool retry_slow = false;
  ASSERT_TRUE(
      internal::CopyFileContentsWithSendfile(from, to_sock, retry_slow));

  // But copying for a socket to a file will not.
  base::PlatformFile from_sock_file(sock[1]);
  base::File from_sock(from_sock_file);

  File to(file_name_to,
          File::FLAG_OPEN | File::FLAG_WRITE | File::FLAG_CREATE_ALWAYS);
  ASSERT_TRUE(to.IsValid());
  ASSERT_FALSE(
      internal::CopyFileContentsWithSendfile(from_sock, to, retry_slow));
  ASSERT_TRUE(retry_slow);
}

TEST_F(FileUtilTest, CopyFileContentsWithSendfileSeqFile) {
  // This test verifies the special case where we have a regular file with zero
  // length that might actually have contents (such as a seq_file).
  for (auto* const file : {"/proc/meminfo", "/proc/self/cmdline",
                           "/proc/self/environ", "/proc/self/auxv"}) {
    FilePath proc_file_from(file);
    File from(proc_file_from, File::FLAG_OPEN | File::FLAG_READ);
    ASSERT_TRUE(from.IsValid()) << "could not open " << file;

    FilePath file_name_to = temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("copy_contents_file_out.txt"));
    File to(file_name_to,
            File::FLAG_OPEN | File::FLAG_WRITE | File::FLAG_CREATE_ALWAYS);
    ASSERT_TRUE(to.IsValid());

    bool retry_slow = false;
    ASSERT_FALSE(internal::CopyFileContentsWithSendfile(from, to, retry_slow))
        << proc_file_from << " should have failed";
    ASSERT_TRUE(retry_slow)
        << "retry slow for " << proc_file_from << " should be set";

    // Now let's make sure we can copy it the "slow" way.
    ASSERT_TRUE(base::CopyFileContents(from, to));
    ASSERT_GT(to.GetLength(), 0);
    ASSERT_TRUE(base::DeleteFile(file_name_to));
  }
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

}  // namespace

}  // namespace base
