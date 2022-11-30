// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/dbus/dlp/fake_dlp_client.h"
#include "components/file_access/file_access_copy_or_move_delegate_factory.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
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
  content::BrowserTaskEnvironment task_environment_;
  chromeos::FakeDlpClient fake_dlp_client_;
  DlpScopedFileAccessDelegate delegate_{&fake_dlp_client_};
};

TEST_F(DlpScopedFileAccessDelegateTest, TestNoSingleton) {
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

TEST_F(DlpScopedFileAccessDelegateTest, TestFileAccessSingletonForUrl) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);

  base::test::TestFuture<file_access::ScopedFileAccess> future1;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  delegate->RequestFilesAccess({file_path}, GURL("example.com"),
                               future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>().is_allowed());

  fake_dlp_client_.SetFileAccessAllowed(false);
  base::test::TestFuture<file_access::ScopedFileAccess> future2;
  delegate->RequestFilesAccess({file_path}, GURL("example.com"),
                               future2.GetCallback());
  EXPECT_FALSE(future2.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest,
       TestFileAccessSingletonForSystemComponent) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);

  base::test::TestFuture<file_access::ScopedFileAccess> future1;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  delegate->RequestFilesAccessForSystem({file_path}, future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, TestMultipleInstances) {
  DlpScopedFileAccessDelegate::Initialize(nullptr);
  EXPECT_NO_FATAL_FAILURE(DlpScopedFileAccessDelegate::Initialize(nullptr));
}

class DlpScopedFileAccessDelegateTaskTest : public testing::Test {
 public:
  content::BrowserTaskEnvironment browser_task_environment_;
  chromeos::FakeDlpClient fake_dlp_client_;
  scoped_refptr<base::SingleThreadTaskRunner> UIThread_ =
      content::GetUIThreadTaskRunner({});
  scoped_refptr<base::SingleThreadTaskRunner> IOThread_ =
      content::GetIOThreadTaskRunner({});

  void SetUp() override {
    file_access::ScopedFileAccessDelegate::DeleteInstance();
    browser_task_environment_.RunUntilIdle();
  }

  void TestPreInit() {
    EXPECT_FALSE(
        file_access::FileAccessCopyOrMoveDelegateFactory::HasInstance());
    UIThread_->PostTask(
        FROM_HERE, base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::Init,
                                  base::Unretained(this)));
  }

  void Init() {
    DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);
    IOThread_->PostTask(
        FROM_HERE,
        base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::TestPostInit,
                       base::Unretained(this)));
  }

  void TestPostInit() {
    EXPECT_TRUE(
        file_access::FileAccessCopyOrMoveDelegateFactory::HasInstance());
    UIThread_->PostTask(
        FROM_HERE, base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::Delete,
                                  base::Unretained(this)));
  }

  void Delete() {
    file_access::ScopedFileAccessDelegate::DeleteInstance();
    IOThread_->PostTask(
        FROM_HERE,
        base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::TestPostDelete,
                       base::Unretained(this)));
  }

  void TestPostDelete() {
    EXPECT_FALSE(
        file_access::FileAccessCopyOrMoveDelegateFactory::HasInstance());
  }
};

TEST_F(DlpScopedFileAccessDelegateTaskTest, TestSync) {
  IOThread_->PostTask(
      FROM_HERE,
      base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::TestPreInit,
                     base::Unretained(this)));
  browser_task_environment_.RunUntilIdle();
}

}  // namespace policy
