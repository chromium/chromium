// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <shlobj.h>

#include <iterator>
#include <memory>
#include <string>
#include <tuple>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL FILE_PATH_LITERAL

namespace base {

// A basic test harness that creates a temporary directory during test case
// setup and deletes it during teardown.
class OsValidationTest : public ::testing::Test {
 protected:
  // ::testing::Test:
  static void SetUpTestCase() {
    temp_dir_ = std::make_unique<ScopedTempDir>().release();
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());
  }

  static void TearDownTestCase() {
    // Explicitly delete the dir to catch any deletion errors.
    ASSERT_TRUE(temp_dir_->Delete());
    auto temp_dir = base::WrapUnique(temp_dir_);
    temp_dir_ = nullptr;
  }

  // Returns the path to the test's temporary directory.
  static const FilePath& temp_path() { return temp_dir_->GetPath(); }

 private:
  static ScopedTempDir* temp_dir_;
};

// static
ScopedTempDir* OsValidationTest::temp_dir_ = nullptr;

// A test harness for exhaustively evaluating the conditions under which an open
// file may be operated on. Template parameters are used to turn off or on
// various bits in the access rights and sharing mode bitfields. These template
// parameters are:
// - The standard access right bits (except for WRITE_OWNER, which requires
//   admin rights): SYNCHRONIZE, WRITE_DAC, READ_CONTROL, DELETE.
// - Generic file access rights: FILE_GENERIC_READ, FILE_GENERIC_WRITE,
//                               FILE_EXECUTE.
// - The sharing bits: FILE_SHARE_READ, FILE_SHARE_WRITE, FILE_SHARE_DELETE.
class OpenFileTest : public OsValidationTest,
                     public ::testing::WithParamInterface<
                         std::tuple<std::tuple<DWORD, DWORD, DWORD, DWORD>,
                                    std::tuple<DWORD, DWORD, DWORD>,
                                    std::tuple<DWORD, DWORD, DWORD>>> {
 protected:
  OpenFileTest() = default;
  OpenFileTest(const OpenFileTest&) = delete;
  OpenFileTest& operator=(const OpenFileTest&) = delete;

  // Returns a dwDesiredAccess bitmask for use with CreateFileW containing the
  // test's access right bits.
  static DWORD GetAccess() {
    // Extract the two tuples of standard and generic file rights.
    std::tuple<DWORD, DWORD, DWORD, DWORD> standard_rights;
    std::tuple<DWORD, DWORD, DWORD> generic_rights;
    std::tie(standard_rights, generic_rights, std::ignore) = GetParam();

    // Extract the five standard rights bits.
    DWORD synchronize_bit;
    DWORD write_dac_bit;
    DWORD read_control_bit;
    DWORD delete_bit;
    std::tie(synchronize_bit, write_dac_bit, read_control_bit, delete_bit) =
        standard_rights;

    // Extract the three generic file rights masks.
    DWORD file_generic_read_bits;
    DWORD file_generic_write_bits;
    DWORD file_generic_execute_bits;
    std::tie(file_generic_read_bits, file_generic_write_bits,
             file_generic_execute_bits) = generic_rights;

    // Combine and return the desired access rights.
    return synchronize_bit | write_dac_bit | read_control_bit | delete_bit |
           file_generic_read_bits | file_generic_write_bits |
           file_generic_execute_bits;
  }

  // Returns a dwShareMode bitmask for use with CreateFileW containing the
  // tests's share mode bits.
  static DWORD GetShareMode() {
    // Extract the tuple of sharing mode bits.
    std::tuple<DWORD, DWORD, DWORD> sharing_bits;
    std::tie(std::ignore, std::ignore, sharing_bits) = GetParam();

    // Extract the sharing mode bits.
    DWORD share_read_bit;
    DWORD share_write_bit;
    DWORD share_delete_bit;
    std::tie(share_read_bit, share_write_bit, share_delete_bit) = sharing_bits;

    // Combine and return the sharing mode.
    return share_read_bit | share_write_bit | share_delete_bit;
  }

  // Appends string representation of the access rights bits present in |access|
  // to |result|.
  static void AppendAccessString(DWORD access, std::string* result) {
#define ENTRY(a) \
  { a, #a }
    static constexpr BitAndName kBitNames[] = {
        // The standard access rights:
        ENTRY(SYNCHRONIZE),
        ENTRY(WRITE_OWNER),
        ENTRY(WRITE_DAC),
        ENTRY(READ_CONTROL),
        ENTRY(DELETE),
        // The file-specific access rights:
        ENTRY(FILE_WRITE_ATTRIBUTES),
        ENTRY(FILE_READ_ATTRIBUTES),
        ENTRY(FILE_EXECUTE),
        ENTRY(FILE_WRITE_EA),
        ENTRY(FILE_READ_EA),
        ENTRY(FILE_APPEND_DATA),
        ENTRY(FILE_WRITE_DATA),
        ENTRY(FILE_READ_DATA),
    };
#undef ENTRY
    ASSERT_NO_FATAL_FAILURE(AppendBitsToString(access, std::begin(kBitNames),
                                               std::end(kBitNames), result));
  }

  // Appends a string representation of the sharing mode bits present in
  // |share_mode| to |result|.
  static void AppendShareModeString(DWORD share_mode, std::string* result) {
#define ENTRY(a) \
  { a, #a }
    static constexpr BitAndName kBitNames[] = {
        ENTRY(FILE_SHARE_DELETE),
        ENTRY(FILE_SHARE_WRITE),
        ENTRY(FILE_SHARE_READ),
    };
#undef ENTRY
    ASSERT_NO_FATAL_FAILURE(AppendBitsToString(
        share_mode, std::begin(kBitNames), std::end(kBitNames), result));
  }

  // Returns true if we expect that a file opened with |access| access rights
  // and |share_mode| sharing can be moved via MoveFileEx, and can be deleted
  // via DeleteFile so long as it is not mapped into a process.
  static bool CanMoveFile(DWORD access, DWORD share_mode) {
    // A file can be moved as long as it is opened with FILE_SHARE_DELETE or
    // if nothing beyond the standard access rights (save DELETE) has been
    // requested. It can be deleted under those same circumstances as long as
    // it has not been mapped into a process.
    constexpr DWORD kStandardNoDelete = STANDARD_RIGHTS_ALL & ~DELETE;
    return ((share_mode & FILE_SHARE_DELETE) != 0) ||
           ((access & ~kStandardNoDelete) == 0);
  }

  // OsValidationTest:
  void SetUp() override {
    OsValidationTest::SetUp();

    // Determine the desired access and share mode for this test.
    access_ = GetAccess();
    share_mode_ = GetShareMode();

    // Make a ScopedTrace instance for comprehensible output.
    std::string access_string;
    ASSERT_NO_FATAL_FAILURE(AppendAccessString(access_, &access_string));
    std::string share_mode_string;
    ASSERT_NO_FATAL_FAILURE(
        AppendShareModeString(share_mode_, &share_mode_string));
    scoped_trace_ = std::make_unique<::testing::ScopedTrace>(
        __FILE__, __LINE__, access_string + ", " + share_mode_string);

    // Make a copy of imm32.dll in the temp dir for fiddling.
    ASSERT_TRUE(CreateTemporaryFileInDir(temp_path(), &temp_file_path_));
    ASSERT_TRUE(CopyFile(FilePath(FPL("c:\\windows\\system32\\imm32.dll")),
                         temp_file_path_));

    // Open the file
    file_handle_.Set(::CreateFileW(temp_file_path_.value().c_str(), access_,
                                   share_mode_, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr));
    ASSERT_TRUE(file_handle_.IsValid()) << ::GetLastError();

    // Get a second unique name in the temp dir to which the file might be
    // moved.
    temp_file_dest_path_ = temp_file_path_.InsertBeforeExtension(FPL("bla"));
  }

  void TearDown() override {
    file_handle_.Close();

    // Manually delete the temp files since the temp dir is reused across tests.
    ASSERT_TRUE(DeleteFile(temp_file_path_));
    ASSERT_TRUE(DeleteFile(temp_file_dest_path_));
  }

  DWORD access() const { return access_; }
  DWORD share_mode() const { return share_mode_; }
  const FilePath& temp_file_path() const { return temp_file_path_; }
  const FilePath& temp_file_dest_path() const { return temp_file_dest_path_; }
  HANDLE file_handle() const { return file_handle_.Get(); }

 private:
  struct BitAndName {
    DWORD bit;
    StringPiece name;
  };

  // Appends the names of the bits present in |bitfield| to |result| based on
  // the array of bit-to-name mappings bounded by |bits_begin| and |bits_end|.
  static void AppendBitsToString(DWORD bitfield,
                                 const BitAndName* bits_begin,
                                 const BitAndName* bits_end,
                                 std::string* result) {
    while (bits_begin < bits_end) {
      const BitAndName& bit_name = *bits_begin;
      if (bitfield & bit_name.bit) {
        if (!result->empty())
          result->append(" | ");
        result->append(bit_name.name.data(), bit_name.name.size());
        bitfield &= ~bit_name.bit;
      }
      ++bits_begin;
    }
    ASSERT_EQ(bitfield, DWORD{0});
  }

  DWORD access_ = 0;
  DWORD share_mode_ = 0;
  std::unique_ptr<::testing::ScopedTrace> scoped_trace_;
  FilePath temp_file_path_;
  FilePath temp_file_dest_path_;
  win::ScopedHandle file_handle_;
};

// Tests that an opened but not mapped file can be deleted as expected.
TEST_P(OpenFileTest, DeleteFile) {
  if (CanMoveFile(access(), share_mode())) {
    EXPECT_NE(::DeleteFileW(temp_file_path().value().c_str()), 0)
        << "Last error code: " << ::GetLastError();
  } else {
    EXPECT_EQ(::DeleteFileW(temp_file_path().value().c_str()), 0);
  }
}

// Tests that an opened file can be moved as expected.
TEST_P(OpenFileTest, MoveFileEx) {
  if (CanMoveFile(access(), share_mode())) {
    EXPECT_NE(::MoveFileExW(temp_file_path().value().c_str(),
                            temp_file_dest_path().value().c_str(), 0),
              0)
        << "Last error code: " << ::GetLastError();
  } else {
    EXPECT_EQ(::MoveFileExW(temp_file_path().value().c_str(),
                            temp_file_dest_path().value().c_str(), 0),
              0);
  }
}

// Tests that an open file cannot be moved after it has been marked for
// deletion.
TEST_P(OpenFileTest, DeleteThenMove) {
  // Don't test combinations that cannot be deleted.
  if (!CanMoveFile(access(), share_mode()))
    return;
  ASSERT_NE(::DeleteFileW(temp_file_path().value().c_str()), 0)
      << "Last error code: " << ::GetLastError();
  // Move fails with ERROR_ACCESS_DENIED (STATUS_DELETE_PENDING under the
  // covers).
  EXPECT_EQ(::MoveFileExW(temp_file_path().value().c_str(),
                          temp_file_dest_path().value().c_str(), 0),
            0);
}

// Tests that an open file that is mapped into memory can be moved but not
// deleted.
TEST_P(OpenFileTest, MapThenDelete) {
  // There is nothing to test if the file can't be read.
  if (!(access() & FILE_READ_DATA))
    return;

  // Pick the protection option that matches the access rights used to open the
  // file.
  static constexpr struct {
    DWORD access_bits;
    DWORD protection;
  } kAccessToProtection[] = {
      // Sorted from most- to least-bits used for logic below.
      {FILE_READ_DATA | FILE_WRITE_DATA | FILE_EXECUTE, PAGE_EXECUTE_READWRITE},
      {FILE_READ_DATA | FILE_WRITE_DATA, PAGE_READWRITE},
      {FILE_READ_DATA | FILE_EXECUTE, PAGE_EXECUTE_READ},
      {FILE_READ_DATA, PAGE_READONLY},
  };

  DWORD protection = 0;
  for (const auto& scan : kAccessToProtection) {
    if ((access() & scan.access_bits) == scan.access_bits) {
      protection = scan.protection;
      break;
    }
  }
  ASSERT_NE(protection, DWORD{0});

  win::ScopedHandle mapping(::CreateFileMappingA(
      file_handle(), nullptr, protection | SEC_IMAGE, 0, 0, nullptr));
  auto result = ::GetLastError();
  ASSERT_TRUE(mapping.IsValid()) << result;

  auto* view = ::MapViewOfFile(mapping.Get(), FILE_MAP_READ, 0, 0, 0);
  result = ::GetLastError();
  ASSERT_NE(view, nullptr) << result;
  ScopedClosureRunner unmapper(
      BindOnce([](const void* view) { ::UnmapViewOfFile(view); }, view));

  // Mapped files cannot be deleted under any circumstances.
  EXPECT_EQ(::DeleteFileW(temp_file_path().value().c_str()), 0);

  // But can still be moved under the same conditions as if it weren't mapped.
  if (CanMoveFile(access(), share_mode())) {
    EXPECT_NE(::MoveFileExW(temp_file_path().value().c_str(),
                            temp_file_dest_path().value().c_str(), 0),
              0)
        << "Last error code: " << ::GetLastError();
  } else {
    EXPECT_EQ(::MoveFileExW(temp_file_path().value().c_str(),
                            temp_file_dest_path().value().c_str(), 0),
              0);
  }
}

// These tests are intentionally disabled by default. They were created as an
// educational tool to understand the restrictions on moving and deleting files
// on Windows. There is every expectation that once they pass, they will always
// pass. It might be interesting to run them manually on new versions of the OS,
// but there is no need to run them on every try/CQ run. Here is one possible
// way to run them all locally:
//
// base_unittests.exe --single-process-tests --gtest_also_run_disabled_tests \
//     --gtest_filter=*OpenFileTest*
INSTANTIATE_TEST_CASE_P(
    DISABLED_Test,
    OpenFileTest,
    ::testing::Combine(
        // Standard access rights except for WRITE_OWNER, which requires admin.
        ::testing::Combine(::testing::Values(0, SYNCHRONIZE),
                           ::testing::Values(0, WRITE_DAC),
                           ::testing::Values(0, READ_CONTROL),
                           ::testing::Values(0, DELETE)),
        // Generic file access rights.
        ::testing::Combine(::testing::Values(0, FILE_GENERIC_READ),
                           ::testing::Values(0, FILE_GENERIC_WRITE),
                           ::testing::Values(0, FILE_GENERIC_EXECUTE)),
        // File sharing mode.
        ::testing::Combine(::testing::Values(0, FILE_SHARE_READ),
                           ::testing::Values(0, FILE_SHARE_WRITE),
                           ::testing::Values(0, FILE_SHARE_DELETE))));

}  // namespace base
