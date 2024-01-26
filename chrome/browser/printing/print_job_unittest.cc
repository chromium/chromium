// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/browser/printing/printer_query.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

class TestPrintJobWorker : public PrintJobWorker {
 public:
  TestPrintJobWorker(
      std::unique_ptr<PrintingContext::Delegate> printing_context_delegate,
      std::unique_ptr<PrintingContext> printing_context,
      PrintJob* print_job)
      : PrintJobWorker(std::move(printing_context_delegate),
                       std::move(printing_context),
                       print_job) {}
  ~TestPrintJobWorker() override = default;
  friend class TestQuery;
};

class TestQuery : public PrinterQuery {
 public:
  TestQuery() : PrinterQuery(content::GlobalRenderFrameHostId()) {}

  void GetSettingsDone(base::OnceClosure callback,
                       std::optional<bool> maybe_is_modifiable,
                       std::unique_ptr<PrintSettings> new_settings,
                       mojom::ResultCode result) override {
    FAIL();
  }

  TestQuery(const TestQuery&) = delete;
  TestQuery& operator=(const TestQuery&) = delete;

  ~TestQuery() override = default;

  std::unique_ptr<PrintJobWorker> TransferContextToNewWorker(
      PrintJob* print_job) override {
    // We're screwing up here since we're calling worker from the main thread.
    // That's fine for testing. It is actually simulating PrinterQuery behavior.
    auto worker = std::make_unique<TestPrintJobWorker>(
        std::move(printing_context_delegate_), std::move(printing_context_),
        print_job);
    worker->printing_context()->UseDefaultSettings();
    SetSettingsForTest(worker->printing_context()->TakeAndResetSettings());

    return std::move(worker);
  }
};

class TestPrintJob : public PrintJob {
 public:
  explicit TestPrintJob(bool* check) : check_(check) {}

 private:
  ~TestPrintJob() override { *check_ = true; }
  const raw_ptr<bool> check_;
};

}  // namespace

TEST(PrintJobTest, SimplePrint) {
  // Test the multi-threaded nature of PrintJob to make sure we can use it with
  // known lifetime.

  content::BrowserTaskEnvironment task_environment;
  bool check = false;
  scoped_refptr<PrintJob> job(base::MakeRefCounted<TestPrintJob>(&check));
  job->Initialize(std::make_unique<TestQuery>(), std::u16string(), 1);
#if BUILDFLAG(IS_CHROMEOS)
  job->SetSource(PrintJob::Source::kPrintPreview, /*source_id=*/"");
#endif  // BUILDFLAG(IS_CHROMEOS)
  job->Stop();
  while (job->document()) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_FALSE(job->document());
  job.reset();
  while (!check) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(check);
}

TEST(PrintJobTest, SimplePrintLateInit) {
  bool check = false;
  content::BrowserTaskEnvironment task_environment;
  scoped_refptr<PrintJob> job(base::MakeRefCounted<TestPrintJob>(&check));
  job.reset();
  EXPECT_TRUE(check);
}

#if BUILDFLAG(IS_WIN)
TEST(PrintJobTest, PageRangeMapping) {
  content::BrowserTaskEnvironment task_environment;

  int page_count = 4;
  std::vector<uint32_t> input_full = {0, 1, 2, 3};
  std::vector<uint32_t> expected_output_full = {0, 1, 2, 3};
  EXPECT_EQ(expected_output_full,
            PrintJob::GetFullPageMapping(input_full, page_count));

  std::vector<uint32_t> input_12 = {1, 2};
  std::vector<uint32_t> expected_output_12 = {kInvalidPageIndex, 1, 2,
                                              kInvalidPageIndex};
  EXPECT_EQ(expected_output_12,
            PrintJob::GetFullPageMapping(input_12, page_count));

  std::vector<uint32_t> input_03 = {0, 3};
  std::vector<uint32_t> expected_output_03 = {0, kInvalidPageIndex,
                                              kInvalidPageIndex, 3};
  EXPECT_EQ(expected_output_03,
            PrintJob::GetFullPageMapping(input_03, page_count));

  std::vector<uint32_t> input_0 = {0};
  std::vector<uint32_t> expected_output_0 = {
      0, kInvalidPageIndex, kInvalidPageIndex, kInvalidPageIndex};
  EXPECT_EQ(expected_output_0,
            PrintJob::GetFullPageMapping(input_0, page_count));

  std::vector<uint32_t> input_invalid = {4, 100};
  std::vector<uint32_t> expected_output_invalid = {
      kInvalidPageIndex, kInvalidPageIndex, kInvalidPageIndex,
      kInvalidPageIndex};
  EXPECT_EQ(expected_output_invalid,
            PrintJob::GetFullPageMapping(input_invalid, page_count));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace printing
