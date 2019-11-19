// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <shlobj.h>

#include <iterator>
#include <memory>
#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL FILE_PATH_LITERAL

namespace base {

// A basic test harness that creates a temporary directory during setup and
// deletes it during teardown.
class OsValidationTest : public ::testing::Test {
 protected:
  OsValidationTest() = default;

  // ::testing::Test:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }
  void TearDown() override { ASSERT_TRUE(temp_dir_.Delete()); }

  // Returns the path to the test's temporary directory.
  const FilePath& temp_path() const { return temp_dir_.GetPath(); }

 private:
  ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(OsValidationTest);
};

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
  // and |share_mode| sharing can be deleted via DeleteFile and/or moved via
  // MoveFileEx.
  static bool CanDeleteFile(DWORD access, DWORD share_mode) {
    // A file can be deleted as long as it is opened with FILE_SHARE_DELETE or
    // if nothing beyond the standard access rights (save DELETE) has been
    // requested.
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

    // Create a file on which to operate.
    ASSERT_TRUE(CreateTemporaryFileInDir(temp_path(), &temp_file_path_));

    // Open the file
    file_handle_.Set(::CreateFileW(temp_file_path_.value().c_str(), access_,
                                   share_mode_, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr));
    ASSERT_TRUE(file_handle_.IsValid()) << ::GetLastError();
  }

  void TearDown() override { file_handle_.Close(); }

  DWORD access() const { return access_; }
  DWORD share_mode() const { return share_mode_; }
  const FilePath& temp_file_path() const { return temp_file_path_; }

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
        bit_name.name.AppendToString(result);
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
  win::ScopedHandle file_handle_;

  DISALLOW_COPY_AND_ASSIGN(OpenFileTest);
};

// Tests that an opened file can be deleted as expected.
TEST_P(OpenFileTest, DeleteFile) {
  if (CanDeleteFile(access(), share_mode())) {
    EXPECT_NE(::DeleteFileW(temp_file_path().value().c_str()), 0)
        << "Last error code: " << ::GetLastError();
  } else {
    EXPECT_EQ(::DeleteFileW(temp_file_path().value().c_str()), 0);
  }
}

// Tests that an opened file can be moved as expected.
TEST_P(OpenFileTest, MoveFileEx) {
  const FilePath dest_path = temp_file_path().InsertBeforeExtension(FPL("bla"));
  if (CanDeleteFile(access(), share_mode())) {
    EXPECT_NE(::MoveFileExW(temp_file_path().value().c_str(),
                            dest_path.value().c_str(), 0),
              0)
        << "Last error code: " << ::GetLastError();
  } else {
    EXPECT_EQ(::MoveFileExW(temp_file_path().value().c_str(),
                            dest_path.value().c_str(), 0),
              0);
  }
}

// Tests that an open file cannot be moved after it has been marked for
// deletion.
TEST_P(OpenFileTest, DeleteThenMove) {
  // Don't test combinations that cannot be deleted.
  if (!CanDeleteFile(access(), share_mode()))
    return;
  ASSERT_NE(::DeleteFileW(temp_file_path().value().c_str()), 0)
      << "Last error code: " << ::GetLastError();
  const FilePath dest_path = temp_file_path().InsertBeforeExtension(FPL("bla"));
  // Move fails with ERROR_ACCESS_DENIED (STATUS_DELETE_PENDING under the
  // covers).
  EXPECT_EQ(::MoveFileExW(temp_file_path().value().c_str(),
                          dest_path.value().c_str(), 0),
            0);
}

INSTANTIATE_TEST_CASE_P(
    ,
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
