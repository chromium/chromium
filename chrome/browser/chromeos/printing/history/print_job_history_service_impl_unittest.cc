// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_history_service_impl.h"

#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chrome/browser/chromeos/printing/history/test_print_job_database.h"
#include "chrome/browser/chromeos/printing/history/test_print_job_history_service_observer.h"
#include "chrome/browser/chromeos/printing/test_cups_print_job_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kTitle[] = "title";

const int kPagesNumber = 3;

}  // namespace

class PrintJobHistoryServiceImplTest : public ::testing::Test {
 public:
  PrintJobHistoryServiceImplTest() {}

  void SetUp() override {
    test_prefs_.SetInitializationCompleted();
    PrintJobHistoryService::RegisterProfilePrefs(test_prefs_.registry());

    std::unique_ptr<PrintJobDatabase> print_job_database =
        std::make_unique<TestPrintJobDatabase>();
    print_job_manager_ = std::make_unique<TestCupsPrintJobManager>(&profile_);
    print_job_history_service_ = std::make_unique<PrintJobHistoryServiceImpl>(
        std::move(print_job_database), print_job_manager_.get(), &test_prefs_);
  }

  void TearDown() override {
    print_job_history_service_.reset();
    print_job_manager_.reset();
  }

  void OnPrintJobSaved(base::RepeatingClosure run_loop_closure, bool success) {
    EXPECT_TRUE(success);
    run_loop_closure.Run();
  }

  void OnPrintJobsRetrieved(
      base::RepeatingClosure run_loop_closure,
      bool success,
      std::unique_ptr<std::vector<printing::proto::PrintJobInfo>> entries) {
    EXPECT_TRUE(success);
    entries_ = *entries;
    run_loop_closure.Run();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestCupsPrintJobManager> print_job_manager_;
  std::unique_ptr<PrintJobHistoryService> print_job_history_service_;
  std::vector<printing::proto::PrintJobInfo> entries_;

 private:
  TestingProfile profile_;
  TestingPrefServiceSimple test_prefs_;
};

TEST_F(PrintJobHistoryServiceImplTest, SaveObservedCupsPrintJob) {
  base::RunLoop save_print_job_run_loop;
  TestPrintJobHistoryServiceObserver observer(
      print_job_history_service_.get(), save_print_job_run_loop.QuitClosure());

  std::unique_ptr<CupsPrintJob> print_job = std::make_unique<CupsPrintJob>(
      chromeos::Printer(), /*job_id=*/0, kTitle, kPagesNumber,
      ::printing::PrintJob::Source::PRINT_PREVIEW,
      /*source_id=*/"", printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());
  print_job_manager_->CancelPrintJob(print_job.get());
  save_print_job_run_loop.Run();

  base::RunLoop get_print_jobs_run_loop;
  print_job_history_service_->GetPrintJobs(base::BindOnce(
      &PrintJobHistoryServiceImplTest::OnPrintJobsRetrieved,
      base::Unretained(this), get_print_jobs_run_loop.QuitClosure()));
  get_print_jobs_run_loop.Run();

  EXPECT_EQ(1u, entries_.size());
  EXPECT_EQ(kTitle, entries_[0].title());
  EXPECT_EQ(kPagesNumber, entries_[0].number_of_pages());
  EXPECT_EQ(printing::proto::PrintJobInfo_PrintJobStatus_CANCELED,
            entries_[0].status());
}

TEST_F(PrintJobHistoryServiceImplTest, ObserverTest) {
  base::RunLoop run_loop;
  TestPrintJobHistoryServiceObserver observer(print_job_history_service_.get(),
                                              run_loop.QuitClosure());

  std::unique_ptr<CupsPrintJob> print_job = std::make_unique<CupsPrintJob>(
      chromeos::Printer(), /*job_id=*/0, kTitle, kPagesNumber,
      ::printing::PrintJob::Source::PRINT_PREVIEW,
      /*source_id=*/"", printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());
  print_job_manager_->CancelPrintJob(print_job.get());
  run_loop.Run();

  EXPECT_EQ(1, observer.num_print_jobs());
}

}  // namespace chromeos
