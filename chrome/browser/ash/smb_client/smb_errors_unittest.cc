// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_errors.h"

#include "base/files/file.h"
#include "chromeos/ash/components/dbus/smbprovider/directory_entry.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::smb_client {

class SmbErrorsTest : public ::testing::Test {
 public:
  SmbErrorsTest() = default;
  SmbErrorsTest(const SmbErrorsTest&) = delete;
  SmbErrorsTest& operator=(const SmbErrorsTest&) = delete;
  ~SmbErrorsTest() override = default;
};

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
  EXPECT_EQ(SmbMountResult::kUnknownFailure,
            TranslateErrorToMountResult(smbprovider::ERROR_OPERATION_FAILED));
}

}  // namespace ash::smb_client
