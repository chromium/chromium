// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_copy_or_move_hook_delegate.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"

namespace policy {
namespace {

constexpr char kEmailId[] = "test@example.com";
constexpr char kGaiaId[] = "12345";

}  // namespace

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

class DlpCopyOrMoveHookDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_manager = std::make_unique<MockDlpRulesManager>();
    manager_ = scoped_manager.get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  MockDlpRulesManager* manager_;
  std::unique_ptr<DlpCopyOrMoveHookDelegate> hook_{
      std::make_unique<DlpCopyOrMoveHookDelegate>()};
  const storage::FileSystemURL source =
      storage::FileSystemURL::CreateForTest(GURL("source"));
  const storage::FileSystemURL destination =
      storage::FileSystemURL::CreateForTest(GURL("destination"));
  std::unique_ptr<MockDlpRulesManager> scoped_manager;
};

TEST_F(DlpCopyOrMoveHookDelegateTest, OnEndCopyNoManager) {
  EXPECT_CALL(*manager_, GetDlpFilesController).Times(0);
  auto task_runner = content::GetIOThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DlpCopyOrMoveHookDelegate::OnEndCopy,
                     base::Unretained(hook_.get()), source, destination));
  task_environment_.RunUntilIdle();
}

class DlpCopyOrMoveHookDelegateTestWithProfile
    : public DlpCopyOrMoveHookDelegateTest {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
    profile_->SetIsNewProfile(true);
    user_manager::User* user =
        user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, /*is_affiliated=*/false,
            user_manager::USER_TYPE_REGULAR, profile_.get());
    user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                /*browser_restart=*/false,
                                /*is_child=*/false);
    user_manager_->SimulateUserProfileLoad(account_id);

    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating(
            &DlpCopyOrMoveHookDelegateTestWithProfile::SetDlpRulesManager,
            base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>();
    manager_ = dlp_rules_manager.get();
    controller_ = std::make_unique<MockController>(*dlp_rules_manager);
    return dlp_rules_manager;
  }

 protected:
  std::unique_ptr<TestingProfile> profile_;
  ash::FakeChromeUserManager* user_manager_{new ash::FakeChromeUserManager()};
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_{
      std::make_unique<user_manager::ScopedUserManager>(
          base::WrapUnique(user_manager_))};
  std::unique_ptr<MockController> controller_;
  MockDlpRulesManager* manager_;
};

TEST_F(DlpCopyOrMoveHookDelegateTestWithProfile, OnEndCopy) {
  EXPECT_CALL(*manager_, GetDlpFilesController)
      .WillOnce(testing::Return(controller_.get()));
  EXPECT_CALL(*controller_, CopySourceInformation)
      .WillOnce([this](const storage::FileSystemURL src,
                       const storage::FileSystemURL dest) {
        EXPECT_EQ(source, src);
        EXPECT_EQ(destination, dest);
      });
  auto task_runner = content::GetIOThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DlpCopyOrMoveHookDelegate::OnEndCopy,
                     base::Unretained(hook_.get()), source, destination));
  task_environment_.RunUntilIdle();
}

TEST_F(DlpCopyOrMoveHookDelegateTestWithProfile, OnEndCopyDelete) {
  EXPECT_CALL(*manager_, GetDlpFilesController)
      .WillOnce(testing::Return(controller_.get()));
  EXPECT_CALL(*controller_, CopySourceInformation)
      .WillOnce([this](const storage::FileSystemURL src,
                       const storage::FileSystemURL dest) {
        EXPECT_EQ(source, src);
        EXPECT_EQ(destination, dest);
      });
  auto task_runner = content::GetIOThreadTaskRunner({});
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(
                            [](std::unique_ptr<DlpCopyOrMoveHookDelegate> hook,
                               const storage::FileSystemURL& source,
                               const storage::FileSystemURL& destination) {
                              hook->OnEndCopy(source, destination);
                              hook.reset();
                            },
                            std::move(hook_), source, destination));
  task_environment_.RunUntilIdle();
}

TEST_F(DlpCopyOrMoveHookDelegateTestWithProfile, OnEndMove) {
  EXPECT_CALL(*manager_, GetDlpFilesController)
      .WillOnce(testing::Return(controller_.get()));
  EXPECT_CALL(*controller_, CopySourceInformation)
      .WillOnce([this](const storage::FileSystemURL src,
                       const storage::FileSystemURL dest) {
        EXPECT_EQ(source, src);
        EXPECT_EQ(destination, dest);
      });
  auto task_runner = content::GetIOThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DlpCopyOrMoveHookDelegate::OnEndMove,
                     base::Unretained(hook_.get()), source, destination));
  task_environment_.RunUntilIdle();
}

TEST_F(DlpCopyOrMoveHookDelegateTestWithProfile, OnEndCopyNoController) {
  EXPECT_CALL(*manager_, GetDlpFilesController)
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(*controller_, CopySourceInformation).Times(0);
  auto task_runner = content::GetIOThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DlpCopyOrMoveHookDelegate::OnEndCopy,
                     base::Unretained(hook_.get()), source, destination));
  task_environment_.RunUntilIdle();
}

}  // namespace policy

#endif
