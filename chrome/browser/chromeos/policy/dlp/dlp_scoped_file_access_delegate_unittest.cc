// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/dbus/dlp/fake_dlp_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DlpScopedFileAccessDelegateTest : public testing::Test {
 public:
  DlpScopedFileAccessDelegateTest() = default;
  ~DlpScopedFileAccessDelegateTest() override = default;

  DlpScopedFileAccessDelegateTest(const DlpScopedFileAccessDelegateTest&) =
      delete;
  DlpScopedFileAccessDelegateTest& operator=(
      const DlpScopedFileAccessDelegateTest&) = delete;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  chromeos::FakeDlpClient fake_dlp_client_;
  DlpScopedFileAccessDelegate delegate_{&fake_dlp_client_};
};

TEST_F(DlpScopedFileAccessDelegateTest, Test) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  base::test::TestFuture<file_access::ScopedFileAccess> future1;
  delegate_.RequestFilesAccess({file_path}, GURL("example.com"),
                               future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>().is_allowed());

  fake_dlp_client_.SetFileAccessAllowed(false);
  base::test::TestFuture<file_access::ScopedFileAccess> future2;
  delegate_.RequestFilesAccess({file_path}, GURL("example.com"),
                               future2.GetCallback());
  EXPECT_FALSE(future2.Get<0>().is_allowed());
}

}  // namespace policy
