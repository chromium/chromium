// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_print_job_notification_manager.h"

#include "base/check_deref.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_notification.h"
#include "chrome/browser/ash/printing/fake_cups_print_job_manager.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
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

constexpr char kAppId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

constexpr char kAppName[] = "app_name";
constexpr char16_t kAppName16[] = u"app_name";

constexpr char kPrinterName[] = "printer";
constexpr char16_t kPrinterName16[] = u"printer";

}  // namespace

class CupsPrintJobNotificationTest : public testing::Test,
                                     public testing::WithParamInterface<bool> {
 protected:
  CupsPrintJobNotificationTest()
      : print_job_manager_(&profile_),
        notification_manager_(&profile_, &print_job_manager_) {}

  void SetUp() override {
    testing::Test::SetUp();

    // Wait for AppServiceProxy to be ready.
    app_service_test_.SetUp(&profile_);
    if (IsWebPrintingTest()) {
      AddIWA(kAppId, kAppName);
    }
  }

  CupsPrintJob CreateCupsPrintJob(const std::string& printer_name,
                                  uint32_t total_page_number) {
    chromeos::Printer printer;
    printer.set_display_name(printer_name);

    if (IsWebPrintingTest()) {
      return CupsPrintJob(
          printer, /*job_id=*/0, /*document_title=*/std::string(),
          total_page_number, crosapi::mojom::PrintJob::Source::kIsolatedWebApp,
          /*source_id=*/kAppId, printing::proto::PrintSettings());
    } else {
      return CupsPrintJob(
          printer, /*job_id=*/0, /*document_title=*/std::string(),
          total_page_number, crosapi::mojom::PrintJob::Source::kUnknown,
          /*source_id=*/std::string(), printing::proto::PrintSettings());
    }
  }

  std::unique_ptr<CupsPrintJobNotification> CreateNotificationForJob(
      base::WeakPtr<CupsPrintJob> job) {
    return std::make_unique<CupsPrintJobNotification>(&notification_manager_,
                                                      job, &profile_);
  }

  std::u16string GetExpectedNotificationAppName() const {
    return IsWebPrintingTest()
               ? kAppName16
               : l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  }

 private:
  bool IsWebPrintingTest() const { return GetParam(); }

  void AddIWA(const std::string& app_id, const std::string& short_name) {
    std::vector<apps::AppPtr> apps;
    apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kWeb, app_id);
    app->app_id = app_id;
    app->readiness = apps::Readiness::kReady;
    app->short_name = short_name;
    apps.push_back(std::move(app));
    app_service_test_.proxy()->OnApps(std::move(apps), apps::AppType::kWeb,
                                      /*should_notify_initialized=*/false);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  FakeCupsPrintJobManager print_job_manager_;
  CupsPrintJobNotificationManager notification_manager_;

  apps::AppServiceTest app_service_test_;
};

// Validates notification for the CLIENT_UNAUTHORIZED error.
TEST_P(CupsPrintJobNotificationTest, ClientUnauthorized) {
  CupsPrintJob job = CreateCupsPrintJob(kPrinterName, /*total_page_number=*/1);

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
                  kPrinterName16)))));
}

// Validates notification for a running job.
TEST_P(CupsPrintJobNotificationTest, RunningJob) {
  CupsPrintJob job = CreateCupsPrintJob(kPrinterName, /*total_page_number=*/10);

  // Create notification.
  auto cups_notification = CreateNotificationForJob(job.GetWeakPtr());

  // Dispatch an update to move it to the STARTED state.
  job.set_state(CupsPrintJob::State::STATE_STARTED);
  cups_notification->OnPrintJobStatusUpdated();

  const auto& notification =
      CHECK_DEREF(cups_notification->GetNotificationDataForTesting());
  EXPECT_THAT(
      notification,
      AllOf(Property(&message_center::Notification::title,
                     Eq(l10n_util::GetStringUTF16(
                         IDS_PRINT_JOB_PRINTING_NOTIFICATION_TITLE))),
            Property(&message_center::Notification::message,
                     Eq(l10n_util::GetStringFUTF16(
                         IDS_PRINT_JOB_NOTIFICATION_APP_PRINTING_IN_PROGRESS,
                         GetExpectedNotificationAppName(), u"10",
                         kPrinterName16)))));
}

// Validates notification for a running job with a single page.
TEST_P(CupsPrintJobNotificationTest, RunningJobSinglePage) {
  CupsPrintJob job = CreateCupsPrintJob(kPrinterName, /*total_page_number=*/1);

  // Create notification.
  auto cups_notification = CreateNotificationForJob(job.GetWeakPtr());

  // Dispatch an update to move it to the STARTED state.
  job.set_state(CupsPrintJob::State::STATE_STARTED);
  cups_notification->OnPrintJobStatusUpdated();

  const auto& notification =
      CHECK_DEREF(cups_notification->GetNotificationDataForTesting());
  EXPECT_THAT(
      notification,
      AllOf(
          Property(&message_center::Notification::title,
                   Eq(l10n_util::GetStringUTF16(
                       IDS_PRINT_JOB_PRINTING_NOTIFICATION_TITLE))),
          Property(
              &message_center::Notification::message,
              Eq(l10n_util::GetStringFUTF16(
                  IDS_PRINT_JOB_NOTIFICATION_APP_PRINTING_IN_PROGRESS_SINGLE_PAGE,
                  GetExpectedNotificationAppName(), kPrinterName16)))));
}

// Validates notification for a completed job.
TEST_P(CupsPrintJobNotificationTest, JobDone) {
  CupsPrintJob job = CreateCupsPrintJob(kPrinterName, /*total_page_number=*/1);

  // Create notification.
  auto cups_notification = CreateNotificationForJob(job.GetWeakPtr());

  // Dispatch an update to move it to the DOCUMENT_DONE state.
  job.set_state(CupsPrintJob::State::STATE_DOCUMENT_DONE);
  cups_notification->OnPrintJobStatusUpdated();

  const auto& notification =
      CHECK_DEREF(cups_notification->GetNotificationDataForTesting());
  EXPECT_THAT(
      notification,
      AllOf(Property(&message_center::Notification::title,
                     Eq(l10n_util::GetStringUTF16(
                         IDS_PRINT_JOB_DONE_NOTIFICATION_TITLE))),
            Property(&message_center::Notification::message,
                     Eq(l10n_util::GetStringFUTF16(
                         IDS_PRINT_JOB_NOTIFICATION_APP_PRINTING_DONE,
                         GetExpectedNotificationAppName(), kPrinterName16)))));
}

// The boolean controls whether the test is instantiated for Web Printing API
// job notifications or regular job notifications.
INSTANTIATE_TEST_SUITE_P(/**/, CupsPrintJobNotificationTest, testing::Bool());

}  // namespace ash
