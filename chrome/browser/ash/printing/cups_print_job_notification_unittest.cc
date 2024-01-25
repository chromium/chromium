// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_print_job_notification_manager.h"

#include "base/check_deref.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_notification.h"
#include "chrome/browser/ash/printing/fake_cups_print_job_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

using testing::AllOf;
using testing::Eq;
using testing::Property;

}  // namespace

class CupsPrintJobNotificationTest : public testing::Test {
 protected:
  CupsPrintJobNotificationTest()
      : print_job_manager_(&profile_),
        notification_manager_(&profile_, &print_job_manager_) {}

  std::unique_ptr<CupsPrintJobNotification> CreateNotificationForJob(
      base::WeakPtr<CupsPrintJob> job) {
    return std::make_unique<CupsPrintJobNotification>(&notification_manager_,
                                                      job, &profile_);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  FakeCupsPrintJobManager print_job_manager_;
  CupsPrintJobNotificationManager notification_manager_;
};

// Validates notification for the CLIENT_UNAUTHORIZED error.
TEST_F(CupsPrintJobNotificationTest, ClientUnauthorized) {
  chromeos::Printer printer;

  printer.set_display_name("printer");
  CupsPrintJob job(printer, 0, std::string(), /*total_page_number=*/1,
                   crosapi::mojom::PrintJob::Source::kUnknown, std::string(),
                   printing::proto::PrintSettings());

  // Create notification.
  auto cups_notification = CreateNotificationForJob(job.GetWeakPtr());

  // Dispatch an update to move it to the FAILED state.
  job.set_error_code(chromeos::PrinterErrorCode::CLIENT_UNAUTHORIZED);
  job.set_state(CupsPrintJob::State::STATE_FAILED);
  cups_notification->OnPrintJobStatusUpdated();

  const auto& notification =
      CHECK_DEREF(cups_notification->GetNotificationDataForTesting());
  EXPECT_THAT(
      notification,
      AllOf(
          Property(&message_center::Notification::title,
                   Eq(l10n_util::GetStringUTF16(
                       IDS_PRINT_JOB_AUTHORIZATION_ERROR_NOTIFICATION_TITLE))),
          Property(
              &message_center::Notification::message,
              Eq(l10n_util::GetStringFUTF16(
                  IDS_PRINT_JOB_NOTIFICATION_IDENTIFICATION_REQUIRED_MESSAGE,
                  u"printer")))));
}

// Validates notification for a running job.
TEST_F(CupsPrintJobNotificationTest, RunningJob) {
  chromeos::Printer printer;

  printer.set_display_name("printer");
  CupsPrintJob job(printer, 0, std::string(), /*total_page_number=*/10,
                   crosapi::mojom::PrintJob::Source::kUnknown, std::string(),
                   printing::proto::PrintSettings());

  // Create notification.
  auto cups_notification = CreateNotificationForJob(job.GetWeakPtr());

  // Dispatch an update to move it to the STARTED state.
  job.set_state(CupsPrintJob::State::STATE_STARTED);
  cups_notification->OnPrintJobStatusUpdated();

  const auto& notification =
      CHECK_DEREF(cups_notification->GetNotificationDataForTesting());
  EXPECT_THAT(notification,
              AllOf(Property(&message_center::Notification::title,
                             Eq(l10n_util::GetStringUTF16(
                                 IDS_PRINT_JOB_PRINTING_NOTIFICATION_TITLE))),
                    Property(&message_center::Notification::message,
                             Eq(l10n_util::GetStringFUTF16(
                                 IDS_PRINT_JOB_NOTIFICATION_MESSAGE, u"10",
                                 u"printer")))));
}

// Validates notification for a running job with a single page.
TEST_F(CupsPrintJobNotificationTest, RunningJobSinglePage) {
  chromeos::Printer printer;

  printer.set_display_name("printer");
  CupsPrintJob job(printer, 0, std::string(), /*total_page_number=*/1,
                   crosapi::mojom::PrintJob::Source::kUnknown, std::string(),
                   printing::proto::PrintSettings());

  // Create notification.
  auto cups_notification = CreateNotificationForJob(job.GetWeakPtr());

  // Dispatch an update to move it to the STARTED state.
  job.set_state(CupsPrintJob::State::STATE_STARTED);
  cups_notification->OnPrintJobStatusUpdated();

  const auto& notification =
      CHECK_DEREF(cups_notification->GetNotificationDataForTesting());
  EXPECT_THAT(notification,
              AllOf(Property(&message_center::Notification::title,
                             Eq(l10n_util::GetStringUTF16(
                                 IDS_PRINT_JOB_PRINTING_NOTIFICATION_TITLE))),
                    Property(&message_center::Notification::message,
                             Eq(l10n_util::GetStringFUTF16(
                                 IDS_PRINT_JOB_NOTIFICATION_SINGLE_PAGE_MESSAGE,
                                 u"printer")))));
}

// Validates notification for a completed job.
TEST_F(CupsPrintJobNotificationTest, JobDone) {
  chromeos::Printer printer;

  printer.set_display_name("printer");
  CupsPrintJob job(printer, 0, std::string(), /*total_page_number=*/1,
                   crosapi::mojom::PrintJob::Source::kUnknown, std::string(),
                   printing::proto::PrintSettings());

  // Create notification.
  auto cups_notification = CreateNotificationForJob(job.GetWeakPtr());

  // Dispatch an update to move it to the DOCUMENT_DONE state.
  job.set_state(CupsPrintJob::State::STATE_DOCUMENT_DONE);
  cups_notification->OnPrintJobStatusUpdated();

  const auto& notification =
      CHECK_DEREF(cups_notification->GetNotificationDataForTesting());
  EXPECT_THAT(notification,
              AllOf(Property(&message_center::Notification::title,
                             Eq(l10n_util::GetStringUTF16(
                                 IDS_PRINT_JOB_DONE_NOTIFICATION_TITLE))),
                    Property(&message_center::Notification::message,
                             Eq(l10n_util::GetStringFUTF16(
                                 IDS_PRINT_JOB_NOTIFICATION_SINGLE_PAGE_MESSAGE,
                                 u"printer")))));
}

}  // namespace ash
