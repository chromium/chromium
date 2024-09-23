// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "chromeos/dbus/dlp/fake_dlp_client.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/file_access/file_access_copy_or_move_delegate_factory.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using DefaultAccess = file_access::ScopedFileAccessDelegate::DefaultAccess;

class DlpScopedFileAccessDelegateTest : public testing::Test {
 public:
  DlpScopedFileAccessDelegateTest() = default;
  ~DlpScopedFileAccessDelegateTest() override = default;

  DlpScopedFileAccessDelegateTest(const DlpScopedFileAccessDelegateTest&) =
      delete;
  DlpScopedFileAccessDelegateTest& operator=(
      const DlpScopedFileAccessDelegateTest&) = delete;

 protected:
  void InitializeWithFakeClient() {
    DlpScopedFileAccessDelegate::Initialize(base::BindLambdaForTesting(
        [this]() -> chromeos::DlpClient* { return &fake_dlp_client_; }));
  }

  content::BrowserTaskEnvironment task_environment_;
  chromeos::FakeDlpClient fake_dlp_client_;
  std::unique_ptr<DlpScopedFileAccessDelegate> delegate_{
      new DlpScopedFileAccessDelegate(base::BindLambdaForTesting(
          [this]() -> chromeos::DlpClient* { return &fake_dlp_client_; }))};
};

TEST_F(DlpScopedFileAccessDelegateTest, TestNoSingleton) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  base::test::TestFuture<file_access::ScopedFileAccess> future1;
  delegate_->RequestFilesAccess({file_path}, GURL("https://example.com"),
                                future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>().is_allowed());

  fake_dlp_client_.SetFileAccessAllowed(false);
  base::test::TestFuture<file_access::ScopedFileAccess> future2;
  delegate_->RequestFilesAccess({file_path}, GURL("https://example.com"),
                                future2.GetCallback());
  EXPECT_FALSE(future2.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, TestFileAccessSingletonForUrl) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  InitializeWithFakeClient();

  base::test::TestFuture<file_access::ScopedFileAccess> future1;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  delegate->RequestFilesAccess({file_path}, GURL("https://example.com"),
                               future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>().is_allowed());

  fake_dlp_client_.SetFileAccessAllowed(false);
  base::test::TestFuture<file_access::ScopedFileAccess> future2;
  delegate->RequestFilesAccess({file_path}, GURL("https://example.com"),
                               future2.GetCallback());
  EXPECT_FALSE(future2.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest,
       TestFileAccessSingletonForSystemComponent) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  InitializeWithFakeClient();

  base::test::TestFuture<file_access::ScopedFileAccess> future1;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  delegate->RequestFilesAccessForSystem({file_path}, future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, CreateFileAccessCallbackAllowTest) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  InitializeWithFakeClient();
  fake_dlp_client_.SetFileAccessAllowed(true);

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  auto cb = delegate->CreateFileAccessCallback(GURL("https://google.com"));
  cb.Run({file_path}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, CreateFileAccessCallbackDenyTest) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  InitializeWithFakeClient();
  fake_dlp_client_.SetFileAccessAllowed(false);

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  auto cb = delegate->CreateFileAccessCallback(GURL("https://google.com"));
  cb.Run({file_path}, future.GetCallback());
  EXPECT_FALSE(future.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest,
       CreateFileAccessCallbackLostInstanceTest) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  InitializeWithFakeClient();
  fake_dlp_client_.SetFileAccessAllowed(false);

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  auto cb = delegate->CreateFileAccessCallback(GURL("https://google.com"));
  delegate_.reset();
  cb.Run({file_path}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, GetCallbackSystemTest) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  InitializeWithFakeClient();

  // Post a task on IO thread to sync with to be sure the IO task setting
  // `request_files_access_for_system_io_callback_` has run.
  base::RunLoop init;
  auto io_thread = content::GetIOThreadTaskRunner({});
  io_thread->PostTask(
      FROM_HERE, base::BindOnce(&base::RunLoop::Quit, base::Unretained(&init)));
  init.Run();

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  auto cb = delegate->GetCallbackForSystem();
  EXPECT_TRUE(cb);
  cb.Run({file_path}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, GetCallbackSystemNoSingletonTest) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  auto cb = delegate->GetCallbackForSystem();
  EXPECT_TRUE(cb);
  cb.Run({file_path}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, NoDlpClientAvailable) {
  // Creating a new instance will automatically delete the old one. Reset the
  // pointer so that we don't attempt to deallocate.
  delegate_.reset();
  auto delegate =
      std::make_unique<DlpScopedFileAccessDelegate>(base::BindLambdaForTesting(
          []() -> chromeos::DlpClient* { return nullptr; }));

  // Defaults to allowed.
  base::test::TestFuture<file_access::ScopedFileAccess> future1;
  delegate->RequestFilesAccess({base::FilePath()},
                               GURL("https://no_dlp_client.com"),
                               future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>().is_allowed());

  // Defaults to allowed.
  base::test::TestFuture<file_access::ScopedFileAccess> future2;
  delegate->RequestFilesAccessForSystem({base::FilePath()},
                                        future2.GetCallback());
  EXPECT_TRUE(future2.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, DlpClientNotAlive) {
  InitializeWithFakeClient();

  fake_dlp_client_.SetIsAlive(false);

  // Defaults to allowed.
  base::test::TestFuture<file_access::ScopedFileAccess> future1;
  delegate_->RequestFilesAccess({base::FilePath()},
                                GURL("https://no_dlp_client.com"),
                                future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>().is_allowed());

  // Defaults to allowed.
  base::test::TestFuture<file_access::ScopedFileAccess> future2;
  delegate_->RequestFilesAccessForSystem({base::FilePath()},
                                         future2.GetCallback());
  EXPECT_TRUE(future2.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, TestMultipleInstances) {
  auto null_client_provider = []() -> chromeos::DlpClient* { return nullptr; };
  DlpScopedFileAccessDelegate::Initialize(
      base::BindLambdaForTesting(null_client_provider));
  EXPECT_NO_FATAL_FAILURE(DlpScopedFileAccessDelegate::Initialize(
      base::BindLambdaForTesting(null_client_provider)));
}

class DlpScopedFileAccessDelegateTaskTest : public testing::Test {
 public:
  content::BrowserTaskEnvironment browser_task_environment_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD};
  base::RunLoop run_loop_;
  chromeos::FakeDlpClient fake_dlp_client_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_ =
      content::GetUIThreadTaskRunner({});
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_ =
      content::GetIOThreadTaskRunner({});

  void SetUp() override {
    file_access::ScopedFileAccessDelegate::DeleteInstance();
    browser_task_environment_.RunUntilIdle();
  }

  void TestPreInit() {
    EXPECT_FALSE(
        file_access::FileAccessCopyOrMoveDelegateFactory::HasInstance());
    ui_thread_->PostTask(
        FROM_HERE, base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::Init,
                                  base::Unretained(this)));
  }

  void Init() {
    InitializeWithFakeClient();
    io_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::TestPostInit,
                       base::Unretained(this)));
  }

  void TestPostInit() {
    EXPECT_TRUE(
        file_access::FileAccessCopyOrMoveDelegateFactory::HasInstance());
    ui_thread_->PostTask(
        FROM_HERE, base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::Delete,
                                  base::Unretained(this)));
  }

  void Delete() {
    file_access::ScopedFileAccessDelegate::DeleteInstance();
    io_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::TestPostDelete,
                       base::Unretained(this)));
  }

  void TestPostDelete() {
    EXPECT_FALSE(
        file_access::FileAccessCopyOrMoveDelegateFactory::HasInstance());
    run_loop_.Quit();
  }

 protected:
  void InitializeWithFakeClient() {
    DlpScopedFileAccessDelegate::Initialize(base::BindLambdaForTesting(
        [this]() -> chromeos::DlpClient* { return &fake_dlp_client_; }));
  }
};

TEST_F(DlpScopedFileAccessDelegateTaskTest, TestSync) {
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::TestPreInit,
                     base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(DlpScopedFileAccessDelegateTaskTest,
       TestGetDefaultFilesAccessIONoInstance) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  io_thread_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this, &file_path]() {
        file_access::ScopedFileAccessDelegate::RequestDefaultFilesAccessIO(
            {file_path}, base::BindLambdaForTesting(
                             [this](file_access::ScopedFileAccess file_access) {
                               DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
                               EXPECT_TRUE(file_access.is_allowed());
                               run_loop_.Quit();
                             }));
      }));
  run_loop_.Run();
}

// This test should simulate calling RequestDefaultFilesAccessIO with existing
// callback for the IO thread but destructed DlpScopedFileAccessDelegate on the
// UI thread.
TEST_F(DlpScopedFileAccessDelegateTaskTest,
       TestRequestDefaultFilesAccessIODestroyedInstance) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  InitializeWithFakeClient();
  // Dlp would disallow but missing ScopedFileAccessDelegate should fall back to
  // allow.
  fake_dlp_client_.SetFileAccessAllowed(false);

  // Post a task on IO thread to sync with to be sure the IO task setting
  // `request_files_access_for_system_io_callback_` has run.
  base::RunLoop init;
  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&base::RunLoop::Quit, base::Unretained(&init)));
  init.Run();

  // Callback that calls the original
  // request_files_access_for_system_io_callback_ after destructing
  // DlpScopedFileAccessDelegate.
  file_access::ScopedFileAccessDelegate::
      ScopedRequestFilesAccessCallbackForTesting file_access_callback(
          base::BindLambdaForTesting(
              [this, &file_access_callback](
                  const std::vector<base::FilePath>& path,
                  base::OnceCallback<void(file_access::ScopedFileAccess)>
                      callback) {
                ui_thread_->PostTask(
                    FROM_HERE,
                    base::BindOnce(&file_access::ScopedFileAccessDelegate::
                                       DeleteInstance));
                file_access_callback.RunOriginalCallback(path,
                                                         std::move(callback));
              }),
          false /* = restore_original_callback*/);
  // The request for file access should be granted as that is the default
  // behaviour for no running dlp (no rules).
  io_thread_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this, &file_path]() {
        file_access::ScopedFileAccessDelegate::RequestDefaultFilesAccessIO(
            {file_path}, base::BindLambdaForTesting(
                             [this](file_access::ScopedFileAccess file_access) {
                               DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
                               EXPECT_TRUE(file_access.is_allowed());
                               run_loop_.Quit();
                             }));
      }));
  run_loop_.Run();
}

TEST_F(DlpScopedFileAccessDelegateTaskTest, TestGetDefaultFilesAccess) {
  InitializeWithFakeClient();
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  base::MockRepeatingCallback<void(
      const dlp::RequestFileAccessRequest,
      chromeos::DlpClient::RequestFileAccessCallback)>
      request_file_access;
  fake_dlp_client_.SetRequestFileAccessMock(request_file_access.Get());
  dlp::RequestFileAccessResponse response;
  response.set_allowed(true);
  EXPECT_CALL(request_file_access, Run)
      .WillOnce(base::test::RunOnceCallback<1>(response, base::ScopedFD()));

  io_thread_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this, &file_path]() {
        file_access::ScopedFileAccessDelegate::RequestDefaultFilesAccessIO(
            {file_path}, base::BindLambdaForTesting(
                             [this](file_access::ScopedFileAccess file_access) {
                               DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
                               EXPECT_TRUE(file_access.is_allowed());
                               run_loop_.Quit();
                             }));
      }));
  run_loop_.Run();
}

TEST_F(DlpScopedFileAccessDelegateTaskTest, TestGetDefaultDenyFilesAccess) {
  InitializeWithFakeClient();
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  base::MockRepeatingCallback<void(
      const dlp::RequestFileAccessRequest,
      chromeos::DlpClient::RequestFileAccessCallback)>
      request_file_access;
  fake_dlp_client_.SetRequestFileAccessMock(request_file_access.Get());
  EXPECT_CALL(request_file_access, Run).Times(0);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chromeos::features::kDataControlsFileAccessDefaultDeny);

  io_thread_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this, &file_path]() {
        file_access::ScopedFileAccessDelegate::RequestDefaultFilesAccessIO(
            {file_path}, base::BindLambdaForTesting(
                             [this](file_access::ScopedFileAccess file_access) {
                               DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
                               EXPECT_TRUE(file_access.is_allowed());
                               run_loop_.Quit();
                             }));
      }));
  run_loop_.Run();
}

class DlpScopedFileAccessDelegateUMATest
    : public DlpScopedFileAccessDelegateTaskTest {
 protected:
  void RequestDefault(const base::FilePath& file_path) {
    base::RunLoop run_loop;
    io_thread_->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&file_path, &run_loop]() {
          file_access::ScopedFileAccessDelegate::RequestDefaultFilesAccessIO(
              {file_path},
              base::BindLambdaForTesting(
                  [&run_loop](file_access::ScopedFileAccess file_access) {
                    run_loop.Quit();
                  }));
        }));
    run_loop.Run();
  }
  const base::HistogramTester histogram_tester_;
  base::ScopedTempDir temp_dir_;
};

// Test if the right UMA histogram is created without the default deny flag set.
TEST_F(DlpScopedFileAccessDelegateUMATest, TestUMADefaultAllow) {
  InitializeWithFakeClient();
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir_.GetPath();
  base::FilePath my_files = file_path.AppendASCII("MyFiles");
  ASSERT_TRUE(
      base::PathService::Override(chrome::DIR_USER_DOCUMENTS, my_files));
  RequestDefault(file_path.AppendASCII("file"));
  RequestDefault(file_path.AppendASCII("not").AppendASCII("MyFiles"));
  RequestDefault(my_files.AppendASCII("file"));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          data_controls::GetDlpHistogramPrefix() +
          std::string(data_controls::dlp::kFilesDefaultFileAccess)),
      base::BucketsAre(base::Bucket(DefaultAccess::kMyFilesAllow, 1),
                       base::Bucket(DefaultAccess::kSystemFilesAllow, 2),
                       base::Bucket(DefaultAccess::kMyFilesDeny, 0),
                       base::Bucket(DefaultAccess::kSystemFilesDeny, 0)));
}

// Test if the right UMA histogram is created with the default deny flag set.
TEST_F(DlpScopedFileAccessDelegateUMATest, TestUMADefaultDeny) {
  InitializeWithFakeClient();
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir_.GetPath();
  base::FilePath my_files = file_path.AppendASCII("MyFiles");
  ASSERT_TRUE(
      base::PathService::Override(chrome::DIR_USER_DOCUMENTS, my_files));
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chromeos::features::kDataControlsFileAccessDefaultDeny);
  RequestDefault(file_path.AppendASCII("file"));
  RequestDefault(file_path.AppendASCII("not").AppendASCII("MyFiles"));
  RequestDefault(my_files.AppendASCII("file"));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(
          data_controls::GetDlpHistogramPrefix() +
          std::string(data_controls::dlp::kFilesDefaultFileAccess)),
      base::BucketsAre(base::Bucket(DefaultAccess::kMyFilesAllow, 0),
                       base::Bucket(DefaultAccess::kSystemFilesAllow, 0),
                       base::Bucket(DefaultAccess::kMyFilesDeny, 1),
                       base::Bucket(DefaultAccess::kSystemFilesDeny, 2)));
}

}  // namespace policy
