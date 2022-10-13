// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_copy_or_move_hook_delegate.h"

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)

namespace policy {

class MockController : public DlpFilesController {
 public:
  explicit MockController(const DlpRulesManager& rules_manager)
      : DlpFilesController(rules_manager) {}
  MOCK_METHOD(void,
              CopySourceInformation,
              (const storage::FileSystemURL& source,
               const storage::FileSystemURL& destination),
              (override));
};

class MockHook : public DlpCopyOrMoveHookDelegate {
 public:
  explicit MockHook(DlpRulesManager* manager) : manager_(manager) {}
  DlpRulesManager* GetRulesManager() override { return manager_; }
  DlpRulesManager* manager_;
};

class DlpCopyOrMoveHookDelegateTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  MockDlpRulesManager manager_;
  MockController controller_{manager_};
  MockHook hook_{&manager_};
  const storage::FileSystemURL source =
      storage::FileSystemURL::CreateForTest(GURL("source"));
  const storage::FileSystemURL destination =
      storage::FileSystemURL::CreateForTest(GURL("destination"));
};

TEST_F(DlpCopyOrMoveHookDelegateTest, OnEndCopy) {
  EXPECT_CALL(manager_, GetDlpFilesController)
      .WillOnce(testing::Return(&controller_));
  EXPECT_CALL(controller_, CopySourceInformation)
      .WillOnce([this](const storage::FileSystemURL src,
                       const storage::FileSystemURL dest) {
        EXPECT_EQ(source, src);
        EXPECT_EQ(destination, dest);
      });
  auto task_runner = content::GetIOThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DlpCopyOrMoveHookDelegate::OnEndCopy,
                                base::Unretained(&hook_), source, destination));
  task_environment_.RunUntilIdle();
}

TEST_F(DlpCopyOrMoveHookDelegateTest, OnEndMove) {
  EXPECT_CALL(manager_, GetDlpFilesController)
      .WillOnce(testing::Return(&controller_));
  EXPECT_CALL(controller_, CopySourceInformation)
      .WillOnce([this](const storage::FileSystemURL src,
                       const storage::FileSystemURL dest) {
        EXPECT_EQ(source, src);
        EXPECT_EQ(destination, dest);
      });
  auto task_runner = content::GetIOThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DlpCopyOrMoveHookDelegate::OnEndMove,
                                base::Unretained(&hook_), source, destination));
  task_environment_.RunUntilIdle();
}

TEST_F(DlpCopyOrMoveHookDelegateTest, OnEndCopyNoManager) {
  hook_.manager_ = nullptr;
  EXPECT_CALL(manager_, GetDlpFilesController).Times(0);
  auto task_runner = content::GetIOThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DlpCopyOrMoveHookDelegate::OnEndCopy,
                                base::Unretained(&hook_), source, destination));
  task_environment_.RunUntilIdle();
}

TEST_F(DlpCopyOrMoveHookDelegateTest, OnEndCopyNoController) {
  EXPECT_CALL(manager_, GetDlpFilesController)
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(controller_, CopySourceInformation).Times(0);
  auto task_runner = content::GetIOThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DlpCopyOrMoveHookDelegate::OnEndCopy,
                                base::Unretained(&hook_), source, destination));
  task_environment_.RunUntilIdle();
}

}  // namespace policy

#endif
