// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/browser/printing/printer_query.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/child_process_host.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

class TestPrintJobWorker : public PrintJobWorker {
 public:
  TestPrintJobWorker()
      : PrintJobWorker(content::ChildProcessHost::kInvalidUniqueID,
                       content::ChildProcessHost::kInvalidUniqueID) {}
  friend class TestQuery;
};

class TestQuery : public PrinterQuery {
 public:
  TestQuery()
      : PrinterQuery(content::ChildProcessHost::kInvalidUniqueID,
                     content::ChildProcessHost::kInvalidUniqueID) {}

  void GetSettingsDone(base::OnceClosure callback,
                       std::unique_ptr<PrintSettings> new_settings,
                       PrintingContext::Result result) override {
    FAIL();
  }

  ~TestQuery() override {}

  std::unique_ptr<PrintJobWorker> DetachWorker() override {
    {
      // Do an actual detach to keep the parent class happy.
      auto real_worker = PrinterQuery::DetachWorker();
    }

    // We're screwing up here since we're calling worker from the main thread.
    // That's fine for testing. It is actually simulating PrinterQuery behavior.
    auto worker = std::make_unique<TestPrintJobWorker>();
    EXPECT_TRUE(worker->Start());
    worker->printing_context()->UseDefaultSettings();
    SetSettingsForTest(worker->printing_context()->TakeAndResetSettings());

    return std::move(worker);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestQuery);
};

class TestPrintJob : public PrintJob {
 public:
  explicit TestPrintJob(volatile bool* check) : check_(check) {
  }
 private:
  ~TestPrintJob() override { *check_ = true; }
  volatile bool* check_;
};

class TestPrintNotificationObserver : public content::NotificationObserver {
 public:
  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    ADD_FAILURE();
  }
};

}  // namespace

TEST(PrintJobTest, SimplePrint) {
  // Test the multi-threaded nature of PrintJob to make sure we can use it with
  // known lifetime.

  content::BrowserTaskEnvironment task_environment;
  content::NotificationRegistrar registrar;
  TestPrintNotificationObserver observer;
  registrar.Add(&observer, content::NOTIFICATION_ALL,
                content::NotificationService::AllSources());
  volatile bool check = false;
  scoped_refptr<PrintJob> job(new TestPrintJob(&check));
  job->Initialize(std::make_unique<TestQuery>(), base::string16(), 1);
#if defined(OS_CHROMEOS)
  job->SetSource(PrintJob::Source::PRINT_PREVIEW, /*source_id=*/"");
#endif  // defined(OS_CHROMEOS)
  job->Stop();
  while (job->document()) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_FALSE(job->document());
  job = nullptr;
  while (!check) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(check);
}

TEST(PrintJobTest, SimplePrintLateInit) {
  volatile bool check = false;
  content::BrowserTaskEnvironment task_environment;
  scoped_refptr<PrintJob> job(new TestPrintJob(&check));
  job = nullptr;
  EXPECT_TRUE(check);
  /* TODO(maruel): Test these.
  job->Initialize()
  job->Observe();
  job->GetSettingsDone();
  job->DetachWorker();
  job->settings();
  job->cookie();
  job->GetSettings(DEFAULTS, ASK_USER, nullptr);
  job->StartPrinting();
  job->Stop();
  job->Cancel();
  job->RequestMissingPages();
  job->FlushJob(timeout);
  job->is_job_pending();
  job->document();
  // Private
  job->UpdatePrintedDocument(nullptr);
  scoped_refptr<JobEventDetails> event_details;
  job->OnNotifyPrintJobEvent(event_details);
  job->OnDocumentDone();
  job->ControlledWorkerShutdown();
  */
}

#if defined(OS_WIN)
TEST(PrintJobTest, PageRangeMapping) {
  content::BrowserTaskEnvironment task_environment;

  int page_count = 4;
  std::vector<int> input_full = {0, 1, 2, 3};
  std::vector<int> expected_output_full = {0, 1, 2, 3};
  EXPECT_EQ(expected_output_full,
            PrintJob::GetFullPageMapping(input_full, page_count));

  std::vector<int> input_12 = {1, 2};
  std::vector<int> expected_output_12 = {-1, 1, 2, -1};
  EXPECT_EQ(expected_output_12,
            PrintJob::GetFullPageMapping(input_12, page_count));

  std::vector<int> input_03 = {0, 3};
  std::vector<int> expected_output_03 = {0, -1, -1, 3};
  EXPECT_EQ(expected_output_03,
            PrintJob::GetFullPageMapping(input_03, page_count));

  std::vector<int> input_0 = {0};
  std::vector<int> expected_output_0 = {0, -1, -1, -1};
  EXPECT_EQ(expected_output_0,
            PrintJob::GetFullPageMapping(input_0, page_count));

  std::vector<int> input_invalid = {4, 100};
  std::vector<int> expected_output_invalid = {-1, -1, -1, -1};
  EXPECT_EQ(expected_output_invalid,
            PrintJob::GetFullPageMapping(input_invalid, page_count));
}
#endif  // defined(OS_WIN)

}  // namespace printing
