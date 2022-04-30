// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_util.h"

#include "net/base/net_errors.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fusebox {

namespace {

class FuseBoxUtilTest : public testing::Test {
 protected:
  FuseBoxUtilTest() = default;
};

}  // namespace

TEST_F(FuseBoxUtilTest, FileErrorToErrno) {
  auto ok = base::File::Error::FILE_OK;
  EXPECT_EQ(0, FileErrorToErrno(ok));

  auto not_found = base::File::Error::FILE_ERROR_NOT_FOUND;
  EXPECT_EQ(ENOENT, FileErrorToErrno(not_found));

  auto security = base::File::Error::FILE_ERROR_SECURITY;
  EXPECT_EQ(EACCES, FileErrorToErrno(security));

  auto io = base::File::Error::FILE_ERROR_IO;
  EXPECT_EQ(EIO, FileErrorToErrno(io));
}

TEST_F(FuseBoxUtilTest, NetErrorToErrno) {
  auto ok = net::OK;
  EXPECT_EQ(0, NetErrorToErrno(ok));

  auto not_found = net::ERR_FILE_NOT_FOUND;
  EXPECT_EQ(ENOENT, NetErrorToErrno(not_found));

  auto access_denied = net::ERR_ACCESS_DENIED;
  EXPECT_EQ(EACCES, NetErrorToErrno(access_denied));

  auto invalid_url = net::ERR_INVALID_URL;
  EXPECT_EQ(EINVAL, NetErrorToErrno(invalid_url));
}

}  // namespace fusebox
