// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_copy_or_move_hook_delegate.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_files_test_base.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_copy.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

class MockController : public DlpFilesController {
 public:
  explicit MockController(const DlpRulesManager& rules_manager)
      : DlpFilesController(rules_manager) {}
  MOCK_METHOD(void,
              RequestCopyAccess,
              (const storage::FileSystemURL&,
               const storage::FileSystemURL&,
               base::OnceCallback<
                   void(std::unique_ptr<file_access::ScopedFileAccess>)>),
              (override));

  MOCK_METHOD(std::optional<data_controls::Component>,
              MapFilePathToPolicyComponent,
              (Profile * profile, const base::FilePath& file_path),
              (override));

  MOCK_METHOD(bool,
              IsInLocalFileSystem,
              (const base::FilePath& file_path),
              (override));

  MOCK_METHOD(void,
              ShowDlpBlockedFiles,
              (std::optional<uint64_t> task_id,
               std::vector<base::FilePath> blocked_files,
               dlp::FileAction action),
              (override));
};

class DlpCopyOrMoveHookDelegateTest : public DlpFilesTestBase {
 protected:
  DlpCopyOrMoveHookDelegateTest()
      : DlpFilesTestBase(std::make_unique<content::BrowserTaskEnvironment>(
            content::BrowserTaskEnvironment::ThreadPoolExecutionMode::QUEUED,
            content::BrowserTaskEnvironment::REAL_IO_THREAD)) {}
  ~DlpCopyOrMoveHookDelegateTest() override = default;

  void SetUp() override {
    DlpFilesTestBase::SetUp();
    controller_ = std::make_unique<MockController>(*rules_manager_);
  }

  absl::flat_hash_map<std::pair<base::FilePath, base::FilePath>,
                      std::unique_ptr<file_access::ScopedFileAccess>>&
  GetAccessMap() {
    return hook_->current_access_map_;
  }

  std::unique_ptr<DlpCopyOrMoveHookDelegate> hook_{
      std::make_unique<DlpCopyOrMoveHookDelegate>()};
  const storage::FileSystemURL source =
      storage::FileSystemURL::CreateForTest(GURL("source"));
  const storage::FileSystemURL destination =
      storage::FileSystemURL::CreateForTest(GURL("destination"));

  std::unique_ptr<MockController> controller_;
};

TEST_F(DlpCopyOrMoveHookDelegateTest, OnBeginProcessFileAllow) {
  base::RunLoop continuation_run_loop;
  base::RunLoop status_callback_run_loop;
  base::MockCallback<base::OnceCallback<void()>> destructor_continuation;
  EXPECT_CALL(destructor_continuation, Run)
      .WillOnce([&continuation_run_loop]() { continuation_run_loop.Quit(); });
  EXPECT_CALL(*rules_manager_, GetDlpFilesController)
      .WillOnce(testing::Return(controller_.get()));

  EXPECT_CALL(*controller_, RequestCopyAccess(source, destination,
                                              base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(
          std::make_unique<file_access::ScopedFileAccessCopy>(
              true, base::ScopedFD(), destructor_continuation.Get())));

  auto task_runner = content::GetIOThreadTaskRunner({});
  base::MockCallback<base::OnceCallback<void(base::File::Error)>> status;
  EXPECT_CALL(status, Run)
      .WillOnce([&status_callback_run_loop](base::File::Error status) {
        DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
        EXPECT_EQ(base::File::FILE_OK, status);
        status_callback_run_loop.Quit();
      });
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DlpCopyOrMoveHookDelegate::OnBeginProcessFile,
                                base::Unretained(hook_.get()), source,
                                destination, status.Get()));
  status_callback_run_loop.Run();
  EXPECT_EQ(1ul, GetAccessMap().size());
  EXPECT_TRUE(GetAccessMap().contains(
      std::make_pair(source.path(), destination.path())));
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DlpCopyOrMoveHookDelegate::OnEndCopy,
                     base::Unretained(hook_.get()), source, destination));
  continuation_run_loop.Run();
  // At this point the value in the map is removed - that does not mean the map
  // is fully updated. For this we have to wait until at least the current task
  // IO task is finished.
  base::RunLoop end_run_loop;
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&base::RunLoop::Quit, base::Unretained(&end_run_loop)));
  end_run_loop.Run();
  EXPECT_EQ(0ul, GetAccessMap().size());
}

TEST_F(DlpCopyOrMoveHookDelegateTest, OnBeginProcessFileDeny) {
  EXPECT_CALL(*rules_manager_, GetDlpFilesController)
      .WillOnce(testing::Return(controller_.get()));

  EXPECT_CALL(*controller_, RequestCopyAccess(source, destination,
                                              base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(
          std::make_unique<file_access::ScopedFileAccess>(false,
                                                          base::ScopedFD())));

  auto task_runner = content::GetIOThreadTaskRunner({});
  base::RunLoop status_callback_run_loop;
  base::MockCallback<base::OnceCallback<void(base::File::Error)>> status;
  EXPECT_CALL(status, Run)
      .WillOnce([&status_callback_run_loop](base::File::Error status) {
        DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
        EXPECT_EQ(base::File::FILE_ERROR_SECURITY, status);
        status_callback_run_loop.Quit();
      });
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DlpCopyOrMoveHookDelegate::OnBeginProcessFile,
                                base::Unretained(hook_.get()), source,
                                destination, status.Get()));
  status_callback_run_loop.Run();
}

TEST_F(DlpCopyOrMoveHookDelegateTest, OnBeginProcessFileAllowHookDestruct) {
  base::RunLoop hook_destruction_run_loop;
  base::RunLoop continuation_run_loop;
  base::RunLoop status_callback_run_loop;
  base::MockCallback<base::OnceCallback<void()>> destructor_continuation;
  EXPECT_CALL(destructor_continuation, Run)
      .WillOnce([&continuation_run_loop]() { continuation_run_loop.Quit(); });
  EXPECT_CALL(*rules_manager_, GetDlpFilesController)
      .WillOnce(testing::Return(controller_.get()));

  EXPECT_CALL(*controller_, RequestCopyAccess(source, destination,
                                              base::test::IsNotNullCallback()))
      .WillOnce(
          [&](const storage::FileSystemURL& source,
              const storage::FileSystemURL& destination,
              base::OnceCallback<void(
                  std::unique_ptr<file_access::ScopedFileAccess>)> callback) {
            hook_.reset();
            hook_destruction_run_loop.Quit();
            std::move(callback).Run(
                std::make_unique<file_access::ScopedFileAccessCopy>(
                    true, base::ScopedFD(), destructor_continuation.Get()));
          });

  auto task_runner = content::GetIOThreadTaskRunner({});
  base::MockCallback<base::OnceCallback<void(base::File::Error)>> status;
  EXPECT_CALL(status, Run)
      .WillOnce([&status_callback_run_loop](base::File::Error status) {
        DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
        EXPECT_EQ(base::File::FILE_OK, status);
        status_callback_run_loop.Quit();
      });
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DlpCopyOrMoveHookDelegate::OnBeginProcessFile,
                                base::Unretained(hook_.get()), source,
                                destination, status.Get()));
  hook_destruction_run_loop.Run();
  status_callback_run_loop.Run();
  continuation_run_loop.Run();
}

TEST_F(DlpCopyOrMoveHookDelegateTest, OnBeginProcessFileNoManager) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      profile_,
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return nullptr;
          }));
  auto task_runner = content::GetIOThreadTaskRunner({});
  base::RunLoop status_callback_run_loop;
  base::MockCallback<base::OnceCallback<void(base::File::Error)>> status;
  EXPECT_CALL(status, Run)
      .WillOnce([&status_callback_run_loop](base::File::Error status) {
        DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
        EXPECT_EQ(base::File::FILE_OK, status);
        status_callback_run_loop.Quit();
      });
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DlpCopyOrMoveHookDelegate::OnBeginProcessFile,
                                base::Unretained(hook_.get()), source,
                                destination, status.Get()));
  status_callback_run_loop.Run();
}

TEST_F(DlpCopyOrMoveHookDelegateTest, OnBeginProcessFileNoController) {
  EXPECT_CALL(*rules_manager_, GetDlpFilesController)
      .WillOnce(testing::Return(nullptr));
  auto task_runner = content::GetIOThreadTaskRunner({});
  base::RunLoop status_callback_run_loop;
  base::MockCallback<base::OnceCallback<void(base::File::Error)>> status;
  EXPECT_CALL(status, Run)
      .WillOnce([&status_callback_run_loop](base::File::Error status) {
        DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
        EXPECT_EQ(base::File::FILE_OK, status);
        status_callback_run_loop.Quit();
      });
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DlpCopyOrMoveHookDelegate::OnBeginProcessFile,
                                base::Unretained(hook_.get()), source,
                                destination, status.Get()));
  status_callback_run_loop.Run();
}

}  // namespace policy
