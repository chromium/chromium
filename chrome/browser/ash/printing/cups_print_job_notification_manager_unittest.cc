// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_print_job_notification_manager.h"

#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_notification.h"
#include "chrome/browser/ash/printing/fake_cups_print_job_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class CupsPrintJobNotificationManagerTest : public testing::Test {
 protected:
  CupsPrintJobNotificationManagerTest()
      : printJobManager_(&profile_), manager_(&profile_, &printJobManager_) {}

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  FakeCupsPrintJobManager printJobManager_;
  CupsPrintJobNotificationManager manager_;
};

TEST_F(CupsPrintJobNotificationManagerTest, PrintJobLifetimeCheck) {
  CupsPrintJob printJob(chromeos::Printer(), 0, std::string(), 1,
                        crosapi::mojom::PrintJob::Source::kUnknown,
                        std::string(), printing::proto::PrintSettings());

  manager_.OnPrintJobCreated(printJob.GetWeakPtr());
  CupsPrintJobNotification* notification =
      manager_.GetNotificationForTesting(&printJob);
  ASSERT_TRUE(notification);

  manager_.OnPrintJobNotificationRemoved(notification);
  notification = manager_.GetNotificationForTesting(&printJob);
  EXPECT_FALSE(notification);

  // Call this just to make sure it doesn't crash.
  manager_.OnPrintJobCancelled(printJob.GetWeakPtr());
}

}  // namespace ash
