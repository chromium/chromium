// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing_metrics/printing_metrics_api.h"

#include "chrome/browser/ash/printing/history/mock_print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/chromeos/extensions/printing_metrics/printing_metrics_service.h"
#include "chrome/browser/extensions/api/printing/printing_api.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/common/extensions/api/printing_metrics.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::ash::printing::proto::PrintJobInfo;

namespace extensions {

namespace {

constexpr char kTitle1[] = "title1";
constexpr char kTitle2[] = "title2";

constexpr int kPagesNumber = 3;

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<extensions::EventRouter>(context, nullptr);
}

std::unique_ptr<KeyedService> BuildPrintJobHistoryService(
    content::BrowserContext* context) {
  return std::make_unique<ash::MockPrintJobHistoryService>();
}

std::unique_ptr<KeyedService> BuildPrintingMetricsService(
    content::BrowserContext* context) {
  return std::make_unique<PrintingMetricsService>(context);
}

void ReturnNoPrintJobs(ash::PrintJobDatabase::GetPrintJobsCallback callback) {
  std::move(callback).Run(true, std::vector<PrintJobInfo>());
}

void ReturnOnePrintJob(ash::PrintJobDatabase::GetPrintJobsCallback callback) {
  PrintJobInfo print_job_info_proto;
  print_job_info_proto.set_title(kTitle1);
  print_job_info_proto.set_status(
      ash::printing::proto::PrintJobInfo_PrintJobStatus_FAILED);
  print_job_info_proto.set_number_of_pages(kPagesNumber);
  std::move(callback).Run(true,
                          std::vector<PrintJobInfo>{print_job_info_proto});
}

void ReturnTwoPrintJobs(ash::PrintJobDatabase::GetPrintJobsCallback callback) {
  PrintJobInfo print_job_info_proto1;
  print_job_info_proto1.set_title(kTitle1);
  print_job_info_proto1.set_status(
      ash::printing::proto::PrintJobInfo_PrintJobStatus_FAILED);
  print_job_info_proto1.set_number_of_pages(kPagesNumber);
  PrintJobInfo print_job_info_proto2;
  print_job_info_proto2.set_title(kTitle2);
  std::move(callback).Run(
      true,
      std::vector<PrintJobInfo>{print_job_info_proto1, print_job_info_proto2});
}

}  // namespace

class PrintingMetricsApiUnittest : public ExtensionApiUnittest {
 public:
  PrintingMetricsApiUnittest() {}

  PrintingMetricsApiUnittest(const PrintingMetricsApiUnittest&) = delete;
  PrintingMetricsApiUnittest& operator=(const PrintingMetricsApiUnittest&) =
      delete;

  ~PrintingMetricsApiUnittest() override = default;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(/*name=*/"printingMetrics API extension")
            .SetID("abcdefghijklmnopqrstuvwxyzabcdef")
            .Build();
    set_extension(extension);

    EventRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildEventRouter));
    ash::PrintJobHistoryServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildPrintJobHistoryService));
    PrintingMetricsService::GetFactoryInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildPrintingMetricsService));
  }

 protected:
  using GetPrintJobsCallback =
      void (*)(ash::PrintJobDatabase::GetPrintJobsCallback);

  void SetUpMockPrintJobHistoryService(GetPrintJobsCallback callback) {
    ash::MockPrintJobHistoryService* print_job_history_service =
        static_cast<ash::MockPrintJobHistoryService*>(
            ash::PrintJobHistoryServiceFactory::GetForBrowserContext(
                browser()->profile()));
    EXPECT_CALL(*print_job_history_service, GetPrintJobs(testing::_))
        .WillOnce(testing::Invoke(callback));
  }
};

// Test that calling |printingMetrics.getPrintJobs()| returns no print jobs.
TEST_F(PrintingMetricsApiUnittest, GetPrintJobs_NoPrintJobs) {
  SetUpMockPrintJobHistoryService(ReturnNoPrintJobs);

  auto function = base::MakeRefCounted<PrintingMetricsGetPrintJobsFunction>();
  auto result = RunFunctionAndReturnValue(function.get(), "[]");

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  EXPECT_TRUE(result->GetList().empty());
}

// Test that calling |printingMetrics.getPrintJobs()| returns the mock print
// job.
TEST_F(PrintingMetricsApiUnittest, GetPrintJobs_OnePrintJob) {
  SetUpMockPrintJobHistoryService(ReturnOnePrintJob);

  auto function = base::MakeRefCounted<PrintingMetricsGetPrintJobsFunction>();
  auto result = RunFunctionAndReturnValue(function.get(), "[]");

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(1u, result->GetList().size());
  std::optional<api::printing_metrics::PrintJobInfo> print_job_info =
      api::printing_metrics::PrintJobInfo::FromValue(result->GetList()[0]);
  ASSERT_TRUE(print_job_info.has_value());

  EXPECT_THAT(
      print_job_info,
      testing::AllOf(testing::Optional(testing::AllOf(
          testing::Field(&api::printing_metrics::PrintJobInfo::title, kTitle1),
          testing::Field(&api::printing_metrics::PrintJobInfo::status,
                         api::printing_metrics::PrintJobStatus::kFailed),
          testing::Field(&api::printing_metrics::PrintJobInfo::number_of_pages,
                         kPagesNumber)))));
}

// Test that calling |printingMetrics.getPrintJobs()| returns both mock print
// jobs.
TEST_F(PrintingMetricsApiUnittest, GetPrintJobs_TwoPrintJobs) {
  SetUpMockPrintJobHistoryService(ReturnTwoPrintJobs);

  auto function = base::MakeRefCounted<PrintingMetricsGetPrintJobsFunction>();
  auto result = RunFunctionAndReturnValue(function.get(), "[]");

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(2u, result->GetList().size());
  std::optional<api::printing_metrics::PrintJobInfo> print_job_info1 =
      api::printing_metrics::PrintJobInfo::FromValue(result->GetList()[0]);
  ASSERT_TRUE(print_job_info1.has_value());
  EXPECT_EQ(kTitle1, print_job_info1->title);
  std::optional<api::printing_metrics::PrintJobInfo> print_job_info2 =
      api::printing_metrics::PrintJobInfo::FromValue(result->GetList()[1]);
  ASSERT_TRUE(print_job_info2.has_value());
  EXPECT_EQ(kTitle2, print_job_info2->title);
}

}  // namespace extensions
