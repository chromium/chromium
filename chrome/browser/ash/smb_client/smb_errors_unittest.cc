// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_errors.h"

#include "base/files/file.h"
#include "chromeos/dbus/smbprovider/directory_entry.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace smb_client {

class SmbErrorsTest : public ::testing::Test {
 public:
  SmbErrorsTest() = default;
  SmbErrorsTest(const SmbErrorsTest&) = delete;
  SmbErrorsTest& operator=(const SmbErrorsTest&) = delete;
  ~SmbErrorsTest() override = default;
};

TEST_F(SmbErrorsTest, SmbErrorToFileError) {
  EXPECT_EQ(base::File::FILE_OK, TranslateToFileError(smbprovider::ERROR_OK));
  EXPECT_EQ(base::File::FILE_ERROR_FAILED,
            TranslateToFileError(smbprovider::ERROR_FAILED));
  EXPECT_EQ(base::File::FILE_ERROR_IN_USE,
            TranslateToFileError(smbprovider::ERROR_IN_USE));
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS,
            TranslateToFileError(smbprovider::ERROR_EXISTS));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            TranslateToFileError(smbprovider::ERROR_NOT_FOUND));
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED,
            TranslateToFileError(smbprovider::ERROR_ACCESS_DENIED));
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED,
            TranslateToFileError(smbprovider::ERROR_TOO_MANY_OPENED));
  EXPECT_EQ(base::File::FILE_ERROR_NO_MEMORY,
            TranslateToFileError(smbprovider::ERROR_NO_MEMORY));
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE,
            TranslateToFileError(smbprovider::ERROR_NO_SPACE));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_DIRECTORY,
            TranslateToFileError(smbprovider::ERROR_NOT_A_DIRECTORY));
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION,
            TranslateToFileError(smbprovider::ERROR_INVALID_OPERATION));
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY,
            TranslateToFileError(smbprovider::ERROR_SECURITY));
  EXPECT_EQ(base::File::FILE_ERROR_ABORT,
            TranslateToFileError(smbprovider::ERROR_ABORT));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE,
            TranslateToFileError(smbprovider::ERROR_NOT_A_FILE));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_EMPTY,
            TranslateToFileError(smbprovider::ERROR_NOT_EMPTY));
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_URL,
            TranslateToFileError(smbprovider::ERROR_INVALID_URL));
  EXPECT_EQ(base::File::FILE_ERROR_IO,
            TranslateToFileError(smbprovider::ERROR_IO));

  // No 1:1 mapping.
  EXPECT_EQ(base::File::FILE_ERROR_FAILED,
            TranslateToFileError(smbprovider::ERROR_DBUS_PARSE_FAILED));
}

TEST_F(SmbErrorsTest, FileErrorToSmbError) {
  EXPECT_EQ(smbprovider::ERROR_OK, TranslateToErrorType(base::File::FILE_OK));
  EXPECT_EQ(smbprovider::ERROR_FAILED,
            TranslateToErrorType(base::File::FILE_ERROR_FAILED));
  EXPECT_EQ(smbprovider::ERROR_IN_USE,
            TranslateToErrorType(base::File::FILE_ERROR_IN_USE));
  EXPECT_EQ(smbprovider::ERROR_EXISTS,
            TranslateToErrorType(base::File::FILE_ERROR_EXISTS));
  EXPECT_EQ(smbprovider::ERROR_NOT_FOUND,
            TranslateToErrorType(base::File::FILE_ERROR_NOT_FOUND));
  EXPECT_EQ(smbprovider::ERROR_ACCESS_DENIED,
            TranslateToErrorType(base::File::FILE_ERROR_ACCESS_DENIED));
  EXPECT_EQ(smbprovider::ERROR_TOO_MANY_OPENED,
            TranslateToErrorType(base::File::FILE_ERROR_TOO_MANY_OPENED));
  EXPECT_EQ(smbprovider::ERROR_NO_MEMORY,
            TranslateToErrorType(base::File::FILE_ERROR_NO_MEMORY));
  EXPECT_EQ(smbprovider::ERROR_NO_SPACE,
            TranslateToErrorType(base::File::FILE_ERROR_NO_SPACE));
  EXPECT_EQ(smbprovider::ERROR_NOT_A_DIRECTORY,
            TranslateToErrorType(base::File::FILE_ERROR_NOT_A_DIRECTORY));
  EXPECT_EQ(smbprovider::ERROR_INVALID_OPERATION,
            TranslateToErrorType(base::File::FILE_ERROR_INVALID_OPERATION));
  EXPECT_EQ(smbprovider::ERROR_SECURITY,
            TranslateToErrorType(base::File::FILE_ERROR_SECURITY));
  EXPECT_EQ(smbprovider::ERROR_ABORT,
            TranslateToErrorType(base::File::FILE_ERROR_ABORT));
  EXPECT_EQ(smbprovider::ERROR_NOT_A_FILE,
            TranslateToErrorType(base::File::FILE_ERROR_NOT_A_FILE));
  EXPECT_EQ(smbprovider::ERROR_NOT_EMPTY,
            TranslateToErrorType(base::File::FILE_ERROR_NOT_EMPTY));
  EXPECT_EQ(smbprovider::ERROR_INVALID_URL,
            TranslateToErrorType(base::File::FILE_ERROR_INVALID_URL));
  EXPECT_EQ(smbprovider::ERROR_IO,
            TranslateToErrorType(base::File::FILE_ERROR_IO));
}

TEST_F(SmbErrorsTest, SmbErrorToMountResult) {
  EXPECT_EQ(SmbMountResult::kSuccess,
            TranslateErrorToMountResult(smbprovider::ERROR_OK));
  EXPECT_EQ(SmbMountResult::kUnknownFailure,
            TranslateErrorToMountResult(smbprovider::ERROR_FAILED));
  EXPECT_EQ(SmbMountResult::kMountExists,
            TranslateErrorToMountResult(smbprovider::ERROR_IN_USE));
  EXPECT_EQ(SmbMountResult::kMountExists,
            TranslateErrorToMountResult(smbprovider::ERROR_EXISTS));
  EXPECT_EQ(SmbMountResult::kNotFound,
            TranslateErrorToMountResult(smbprovider::ERROR_NOT_FOUND));
  EXPECT_EQ(SmbMountResult::kAuthenticationFailed,
            TranslateErrorToMountResult(smbprovider::ERROR_ACCESS_DENIED));
  EXPECT_EQ(SmbMountResult::kTooManyOpened,
            TranslateErrorToMountResult(smbprovider::ERROR_TOO_MANY_OPENED));
  EXPECT_EQ(SmbMountResult::kOutOfMemory,
            TranslateErrorToMountResult(smbprovider::ERROR_NO_MEMORY));
  EXPECT_EQ(SmbMountResult::kOutOfMemory,
            TranslateErrorToMountResult(smbprovider::ERROR_NO_SPACE));
  EXPECT_EQ(SmbMountResult::kNotFound,
            TranslateErrorToMountResult(smbprovider::ERROR_NOT_A_DIRECTORY));
  EXPECT_EQ(SmbMountResult::kInvalidOperation,
            TranslateErrorToMountResult(smbprovider::ERROR_INVALID_OPERATION));
  EXPECT_EQ(SmbMountResult::kAuthenticationFailed,
            TranslateErrorToMountResult(smbprovider::ERROR_SECURITY));
  EXPECT_EQ(SmbMountResult::kAborted,
            TranslateErrorToMountResult(smbprovider::ERROR_ABORT));
  EXPECT_EQ(SmbMountResult::kUnknownFailure,
            TranslateErrorToMountResult(smbprovider::ERROR_NOT_A_FILE));
  EXPECT_EQ(SmbMountResult::kUnknownFailure,
            TranslateErrorToMountResult(smbprovider::ERROR_NOT_EMPTY));
  EXPECT_EQ(SmbMountResult::kInvalidUrl,
            TranslateErrorToMountResult(smbprovider::ERROR_INVALID_URL));
  EXPECT_EQ(SmbMountResult::kIoError,
            TranslateErrorToMountResult(smbprovider::ERROR_IO));
  EXPECT_EQ(SmbMountResult::kDbusParseFailed,
            TranslateErrorToMountResult(smbprovider::ERROR_DBUS_PARSE_FAILED));
}

TEST_F(SmbErrorsTest, FileErrorToMountResult) {
  EXPECT_EQ(SmbMountResult::kSuccess,
            TranslateErrorToMountResult(base::File::FILE_OK));
  EXPECT_EQ(SmbMountResult::kUnknownFailure,
            TranslateErrorToMountResult(base::File::FILE_ERROR_FAILED));
  EXPECT_EQ(SmbMountResult::kMountExists,
            TranslateErrorToMountResult(base::File::FILE_ERROR_IN_USE));
  EXPECT_EQ(SmbMountResult::kMountExists,
            TranslateErrorToMountResult(base::File::FILE_ERROR_EXISTS));
  EXPECT_EQ(SmbMountResult::kNotFound,
            TranslateErrorToMountResult(base::File::FILE_ERROR_NOT_FOUND));
  EXPECT_EQ(SmbMountResult::kAuthenticationFailed,
            TranslateErrorToMountResult(base::File::FILE_ERROR_ACCESS_DENIED));
  EXPECT_EQ(
      SmbMountResult::kTooManyOpened,
      TranslateErrorToMountResult(base::File::FILE_ERROR_TOO_MANY_OPENED));
  EXPECT_EQ(SmbMountResult::kOutOfMemory,
            TranslateErrorToMountResult(base::File::FILE_ERROR_NO_MEMORY));
  EXPECT_EQ(SmbMountResult::kOutOfMemory,
            TranslateErrorToMountResult(base::File::FILE_ERROR_NO_SPACE));
  EXPECT_EQ(
      SmbMountResult::kNotFound,
      TranslateErrorToMountResult(base::File::FILE_ERROR_NOT_A_DIRECTORY));
  EXPECT_EQ(
      SmbMountResult::kInvalidOperation,
      TranslateErrorToMountResult(base::File::FILE_ERROR_INVALID_OPERATION));
  EXPECT_EQ(SmbMountResult::kAuthenticationFailed,
            TranslateErrorToMountResult(base::File::FILE_ERROR_SECURITY));
  EXPECT_EQ(SmbMountResult::kAborted,
            TranslateErrorToMountResult(base::File::FILE_ERROR_ABORT));
  EXPECT_EQ(SmbMountResult::kUnknownFailure,
            TranslateErrorToMountResult(base::File::FILE_ERROR_NOT_A_FILE));
  EXPECT_EQ(SmbMountResult::kUnknownFailure,
            TranslateErrorToMountResult(base::File::FILE_ERROR_NOT_EMPTY));
  EXPECT_EQ(SmbMountResult::kInvalidUrl,
            TranslateErrorToMountResult(base::File::FILE_ERROR_INVALID_URL));
  EXPECT_EQ(SmbMountResult::kIoError,
            TranslateErrorToMountResult(base::File::FILE_ERROR_IO));
}

}  // namespace smb_client
}  // namespace ash
