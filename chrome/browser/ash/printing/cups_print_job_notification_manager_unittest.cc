// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_print_job_notification_manager.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_notification.h"
#include "chrome/browser/ash/printing/fake_cups_print_job_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

namespace ash {

class CupsPrintJobNotificationManagerTest : public testing::Test {
 protected:
  CupsPrintJobNotificationManagerTest()
      : printJobManager_(&profile_), manager_(&profile_, &printJobManager_) {}

  void SetUp() override {
    testing::Test::SetUp();
    message_center::MessageCenter::Initialize();

    user_ = fake_user_manager_->AddUser(user_manager::StubAccountId());
    fake_user_manager_->LoginUser(user_manager::StubAccountId());
    AnnotatedAccountId::Set(&profile_, user_manager::StubAccountId());
  }

  void TearDown() override {
    message_center::MessageCenter::Shutdown();
    testing::Test::TearDown();
  }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_{std::make_unique<FakeChromeUserManager>()};
  raw_ptr<const user_manager::User> user_;
  TestingProfile profile_;
  FakeCupsPrintJobManager printJobManager_;
  CupsPrintJobNotificationManager manager_;
};

TEST_F(CupsPrintJobNotificationManagerTest, PrintJobLifetimeCheck) {
  CupsPrintJob printJob(chromeos::Printer(), 0, std::string(), 1,
                        ::printing::PrintJob::Source::kPrintPreview,
                        std::string(), printing::proto::PrintSettings());

  manager_.OnPrintJobCreated(printJob.GetWeakPtr());
  CupsPrintJobNotification* notification =
      manager_.GetNotificationForTesting(&printJob);
  ASSERT_TRUE(notification);
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindNotificationById(
      CreateUserScopedNotificationId(printJob.GetUniqueId(),
                                     user_->username_hash())));

  manager_.OnPrintJobNotificationRemoved(notification);
  notification = manager_.GetNotificationForTesting(&printJob);
  EXPECT_FALSE(notification);

  // Call this just to make sure it doesn't crash.
  manager_.OnPrintJobCancelled(printJob.GetWeakPtr());
}

}  // namespace ash
