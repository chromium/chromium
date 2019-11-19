// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing_metrics/printing_metrics_api.h"

#include "chrome/browser/chromeos/printing/history/mock_print_job_history_service.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/common/extensions/api/printing_metrics.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::printing::proto::PrintJobInfo;

namespace extensions {

namespace {

constexpr char kTitle1[] = "title1";
constexpr char kTitle2[] = "title2";

constexpr int kPagesNumber = 3;

// Creates a new MockPrintJobHistoryService for the given |context|.
std::unique_ptr<KeyedService> BuildPrintJobHistoryService(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::MockPrintJobHistoryService>();
}

void ReturnNoPrintJobs(
    chromeos::PrintJobDatabase::GetPrintJobsCallback callback) {
  std::move(callback).Run(true, std::make_unique<std::vector<PrintJobInfo>>());
}

void ReturnOnePrintJob(
    chromeos::PrintJobDatabase::GetPrintJobsCallback callback) {
  chromeos::printing::proto::PrintJobInfo print_job_info_proto;
  print_job_info_proto.set_title(kTitle1);
  print_job_info_proto.set_status(
      chromeos::printing::proto::PrintJobInfo_PrintJobStatus_FAILED);
  print_job_info_proto.set_number_of_pages(kPagesNumber);
  std::move(callback).Run(true,
                          std::make_unique<std::vector<PrintJobInfo>>(
                              std::vector<PrintJobInfo>{print_job_info_proto}));
}

void ReturnTwoPrintJobs(
    chromeos::PrintJobDatabase::GetPrintJobsCallback callback) {
  chromeos::printing::proto::PrintJobInfo print_job_info_proto1;
  print_job_info_proto1.set_title(kTitle1);
  print_job_info_proto1.set_status(
      chromeos::printing::proto::PrintJobInfo_PrintJobStatus_FAILED);
  print_job_info_proto1.set_number_of_pages(kPagesNumber);
  chromeos::printing::proto::PrintJobInfo print_job_info_proto2;
  print_job_info_proto2.set_title(kTitle2);
  std::move(callback).Run(
      true,
      std::make_unique<std::vector<PrintJobInfo>>(std::vector<PrintJobInfo>{
          print_job_info_proto1, print_job_info_proto2}));
}

}  // namespace

class PrintingMetricsApiUnittest : public ExtensionApiUnittest {
 public:
  PrintingMetricsApiUnittest() {}
  ~PrintingMetricsApiUnittest() override = default;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(/*name=*/"printingMetrics API extension")
            .SetID("abcdefghijklmnopqrstuvwxyzabcdef")
            .Build();
    set_extension(extension);

    chromeos::PrintJobHistoryServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&BuildPrintJobHistoryService));
  }

 protected:
  using GetPrintJobsCallback =
      void (*)(chromeos::PrintJobDatabase::GetPrintJobsCallback);

  void SetUpMockPrintJobHistoryService(GetPrintJobsCallback callback) {
    chromeos::MockPrintJobHistoryService* print_job_history_service =
        static_cast<chromeos::MockPrintJobHistoryService*>(
            chromeos::PrintJobHistoryServiceFactory::GetForBrowserContext(
                browser()->profile()));
    EXPECT_CALL(*print_job_history_service, GetPrintJobs(testing::_))
        .WillOnce(testing::Invoke(callback));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintingMetricsApiUnittest);
};

// Test that calling |printingMetrics.getPrintJobs()| returns no print jobs.
TEST_F(PrintingMetricsApiUnittest, GetPrintJobs_NoPrintJobs) {
  SetUpMockPrintJobHistoryService(ReturnNoPrintJobs);

  auto function = base::MakeRefCounted<PrintingMetricsGetPrintJobsFunction>();
  std::unique_ptr<base::Value> result =
      RunFunctionAndReturnValue(function.get(), "[]");

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  EXPECT_TRUE(result->GetList().empty());
}

// Test that calling |printingMetrics.getPrintJobs()| returns the mock print
// job.
TEST_F(PrintingMetricsApiUnittest, GetPrintJobs_OnePrintJob) {
  SetUpMockPrintJobHistoryService(ReturnOnePrintJob);

  auto function = base::MakeRefCounted<PrintingMetricsGetPrintJobsFunction>();
  std::unique_ptr<base::Value> result =
      RunFunctionAndReturnValue(function.get(), "[]");

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(1u, result->GetList().size());
  std::unique_ptr<api::printing_metrics::PrintJobInfo> print_job_info =
      api::printing_metrics::PrintJobInfo::FromValue(result->GetList()[0]);
  EXPECT_TRUE(print_job_info);
  EXPECT_EQ(kTitle1, print_job_info->title);
  EXPECT_EQ(api::printing_metrics::PRINT_JOB_STATUS_FAILED,
            print_job_info->status);
  EXPECT_EQ(kPagesNumber, print_job_info->number_of_pages);
}

// Test that calling |printingMetrics.getPrintJobs()| returns both mock print
// jobs.
TEST_F(PrintingMetricsApiUnittest, GetPrintJobs_TwoPrintJobs) {
  SetUpMockPrintJobHistoryService(ReturnTwoPrintJobs);

  auto function = base::MakeRefCounted<PrintingMetricsGetPrintJobsFunction>();
  std::unique_ptr<base::Value> result =
      RunFunctionAndReturnValue(function.get(), "[]");

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(2u, result->GetList().size());
  std::unique_ptr<api::printing_metrics::PrintJobInfo> print_job_info1 =
      api::printing_metrics::PrintJobInfo::FromValue(result->GetList()[0]);
  EXPECT_TRUE(print_job_info1);
  EXPECT_EQ(kTitle1, print_job_info1->title);
  std::unique_ptr<api::printing_metrics::PrintJobInfo> print_job_info2 =
      api::printing_metrics::PrintJobInfo::FromValue(result->GetList()[1]);
  EXPECT_TRUE(print_job_info2);
  EXPECT_EQ(kTitle2, print_job_info2->title);
}

}  // namespace extensions
