// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_print_job_notification_manager.h"

#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "chrome/browser/ash/printing/cups_print_job_notification.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class FakeCupsPrintJobManager : public CupsPrintJobManager {
 public:
  explicit FakeCupsPrintJobManager(Profile* profile)
      : CupsPrintJobManager(profile) {}

  bool CreatePrintJob(const std::string& printer_id,
                      const std::string& title,
                      int job_id,
                      int total_page_number,
                      ::printing::PrintJob::Source source,
                      const std::string& source_id,
                      const printing::proto::PrintSettings& settings) override {
    return true;
  }
  void CancelPrintJob(CupsPrintJob* job) override {}
  bool SuspendPrintJob(CupsPrintJob* job) override { return true; }
  bool ResumePrintJob(CupsPrintJob* job) override { return true; }
};

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
                        crosapi::mojom::PrintJob::Source::UNKNOWN,
                        std::string(), printing::proto::PrintSettings());

  manager_.OnPrintJobCreated(printJob.GetWeakPtr());
  absl::optional<CupsPrintJobNotification*> notification =
      manager_.GetNotificationForTesting(&printJob);
  ASSERT_TRUE(notification.has_value());

  manager_.OnPrintJobNotificationRemoved(*notification);
  notification = manager_.GetNotificationForTesting(&printJob);
  EXPECT_FALSE(notification.has_value());

  // Call this just to make sure it doesn't crash.
  manager_.OnPrintJobCancelled(printJob.GetWeakPtr());
}

}  // namespace ash
