// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_management/printing_manager.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_impl.h"
#include "chrome/browser/ash/printing/history/test_print_job_database.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace printing {
namespace print_management {
namespace {

using ::chromeos::printing::printing_manager::mojom::PrintJobInfoPtr;

constexpr char kTitle[] = "title";
const int kPagesNumber = 3;

void VerifyPrintJobIsOngoing(const PrintJobInfoPtr& jobInfo) {
  // An ongoing print job has a null |completed_info| and a non-null
  // |active_print_job_info|.
  EXPECT_FALSE(jobInfo->completed_info);
  EXPECT_TRUE(jobInfo->active_print_job_info);
}

void VerifyPrintJobIsCompleted(const PrintJobInfoPtr& jobInfo) {
  // A completed print job has a non-null |completed_info| and a null
  // |active_print_job_info|.
  EXPECT_TRUE(jobInfo->completed_info);
  EXPECT_FALSE(jobInfo->active_print_job_info);
}

// Waits for OnURLsDeleted event and when run quits the supplied run loop.
class WaitForURLsDeletedObserver : public history::HistoryServiceObserver {
 public:
  explicit WaitForURLsDeletedObserver(base::RunLoop* runner)
      : runner_(runner) {}
  ~WaitForURLsDeletedObserver() override {}
  WaitForURLsDeletedObserver(const WaitForURLsDeletedObserver&) = delete;
  WaitForURLsDeletedObserver& operator=(const WaitForURLsDeletedObserver&) =
      delete;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* service,
                          const history::DeletionInfo& deletion_info) override {
    runner_->Quit();
  }

 private:
  raw_ptr<base::RunLoop> runner_;
};

void WaitForURLsDeletedNotification(history::HistoryService* history_service) {
  base::RunLoop runner;
  WaitForURLsDeletedObserver observer(&runner);
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      scoped_observer(&observer);
  scoped_observer.Observe(history_service);
  runner.Run();
}
}  // namespace

class PrintingManagerTest : public ::testing::Test {
 public:
  PrintingManagerTest() {
    test_prefs_.SetInitializationCompleted();
    PrintJobHistoryService::RegisterProfilePrefs(test_prefs_.registry());
    test_prefs_.registry()->RegisterBooleanPref(
        prefs::kDeletePrintJobHistoryAllowed, true);

    print_job_manager_ = std::make_unique<TestCupsPrintJobManager>(&profile_);
    auto print_job_database = std::make_unique<TestPrintJobDatabase>();
    print_job_history_service_ = std::make_unique<PrintJobHistoryServiceImpl>(
        std::move(print_job_database), print_job_manager_.get(), &test_prefs_);
    EXPECT_TRUE(history_dir_.CreateUniqueTempDir());
    local_history_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);
    printing_manager_ = std::make_unique<PrintingManager>(
        print_job_history_service_.get(), local_history_.get(),
        print_job_manager_.get(), &test_prefs_);
    mock_time_task_runner_ =
        base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  }

  void OnPrintJobsRetrieved(base::RepeatingClosure run_loop_closure,
                            std::vector<PrintJobInfoPtr> entries) {
    entries_ = std::move(entries);
    run_loop_closure.Run();
  }

  void OnDeleteAllPrintJobs(base::RepeatingClosure run_loop_closure,
                            bool expected_result,
                            bool success) {
    EXPECT_EQ(expected_result, success);
    run_loop_closure.Run();
  }

  void RunGetPrintJobs() {
    base::RunLoop run_loop;
    printing_manager_->GetPrintJobs(
        base::BindOnce(&PrintingManagerTest::OnPrintJobsRetrieved,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void CreateCompletedPrintJob(int id) {
    DCHECK(print_job_manager_);
    auto print_job = CreateOngoingPrintJob(id);
    print_job_manager_->CancelPrintJob(print_job.get());
  }

  std::unique_ptr<CupsPrintJob> CreateOngoingPrintJob(int id) {
    auto print_job = std::make_unique<CupsPrintJob>(
        chromeos::Printer(), id, kTitle, kPagesNumber,
        ::printing::PrintJob::Source::kPrintPreview,
        /*source_id=*/"", proto::PrintSettings());
    print_job_manager_->CreatePrintJob(print_job.get());
    return print_job;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  // This cannot be declared after |print_job_manager| and |printing_manager|
  // as we have to ensure that those services are destructed before this
  // is destructed.
  TestingPrefServiceSimple test_prefs_;
  std::unique_ptr<TestCupsPrintJobManager> print_job_manager_;
  std::unique_ptr<history::HistoryService> local_history_;
  std::unique_ptr<PrintingManager> printing_manager_;
  std::vector<PrintJobInfoPtr> entries_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

 private:
  std::unique_ptr<PrintJobHistoryService> print_job_history_service_;
  TestingProfile profile_;
  base::ScopedTempDir history_dir_;
};

TEST_F(PrintingManagerTest, RetrieveOngoingPrintJob) {
  // Assert no initial print jobs are ongoing or saved.
  RunGetPrintJobs();
  ASSERT_EQ(0u, entries_.size());

  auto print_job = CreateOngoingPrintJob(/*id=*/0);

  // Expect to have retrieved one ongoing print job.
  RunGetPrintJobs();
  EXPECT_EQ(1u, entries_.size());
  VerifyPrintJobIsOngoing(entries_[0]);

  // Finish the print job and expect that it is now a completed print job.
  print_job_manager_->CancelPrintJob(print_job.get());
  RunGetPrintJobs();
  EXPECT_EQ(1u, entries_.size());
  VerifyPrintJobIsCompleted(entries_[0]);
}

TEST_F(PrintingManagerTest, RetrieveCompletedPrintJob) {
  // Assert no initial print jobs are ongoing or saved.
  RunGetPrintJobs();
  ASSERT_EQ(0u, entries_.size());

  CreateCompletedPrintJob(/*id=*/0);

  // Expect to have retrieved one ongoing print job.
  RunGetPrintJobs();
  EXPECT_EQ(1u, entries_.size());
  VerifyPrintJobIsCompleted(entries_[0]);
}

TEST_F(PrintingManagerTest, RetrieveCompletedAndOngoingPrintJobs) {
  // Assert no initial print jobs are ongoing or saved.
  RunGetPrintJobs();
  ASSERT_EQ(0u, entries_.size());

  // Create completed print job.
  CreateCompletedPrintJob(/*id=*/0);

  // Create ongoing print job.
  auto ongoing_print_job = CreateOngoingPrintJob(/*id=*/1);

  // Expect to have retrieved one ongoing print job.
  RunGetPrintJobs();
  EXPECT_EQ(2u, entries_.size());
  VerifyPrintJobIsCompleted(entries_[0]);
  VerifyPrintJobIsOngoing(entries_[1]);
}

TEST_F(PrintingManagerTest, DeleteAllPrintJobs) {
  // Assert no initial print jobs are ongoing or saved.
  RunGetPrintJobs();
  ASSERT_EQ(0u, entries_.size());

  CreateCompletedPrintJob(/*id=*/0);
  RunGetPrintJobs();
  EXPECT_EQ(1u, entries_.size());

  // Delete all print jobs.
  base::RunLoop delete_all_print_jobs_run_loop;
  printing_manager_->DeleteAllPrintJobs(base::BindOnce(
      &PrintingManagerTest::OnDeleteAllPrintJobs, base::Unretained(this),
      delete_all_print_jobs_run_loop.QuitClosure(),
      /*expected_result=*/true));
  delete_all_print_jobs_run_loop.Run();

  // Run GetPrintJobs again and verify that the print job has been deleted.
  RunGetPrintJobs();
  EXPECT_EQ(0u, entries_.size());
}

TEST_F(PrintingManagerTest, DeleteAllPrintJobsPreventedByPolicy) {
  test_prefs_.SetBoolean(prefs::kDeletePrintJobHistoryAllowed, false);

  // Assert no initial print jobs are ongoing or saved.
  RunGetPrintJobs();
  ASSERT_EQ(0u, entries_.size());

  CreateCompletedPrintJob(/*id=*/0);
  RunGetPrintJobs();
  EXPECT_EQ(1u, entries_.size());

  // Delete all print jobs.
  base::RunLoop delete_all_print_jobs_run_loop;
  printing_manager_->DeleteAllPrintJobs(base::BindOnce(
      &PrintingManagerTest::OnDeleteAllPrintJobs, base::Unretained(this),
      delete_all_print_jobs_run_loop.QuitClosure(),
      /*expected_result=*/false));
  delete_all_print_jobs_run_loop.Run();

  // Run GetPrintJobs again and verify that the print job has not been deleted.
  RunGetPrintJobs();
  EXPECT_EQ(1u, entries_.size());
}

TEST_F(PrintingManagerTest, DeletingBrowserHistoryDeletesAllPrintJobs) {
  // Assert no initial print jobs are ongoing or saved.
  RunGetPrintJobs();
  ASSERT_EQ(0u, entries_.size());

  // Store a print job.
  CreateCompletedPrintJob(/*id=*/0);
  RunGetPrintJobs();
  EXPECT_EQ(1u, entries_.size());

  // Simulate deleting all history, expect print job history to also be deleted.
  base::CancelableTaskTracker task_tracker;
  local_history_->ExpireHistoryBetween(
      std::set<GURL>(), history::kNoAppIdFilter, base::Time(), base::Time(),
      /*user_initiated*/ true, base::DoNothing(), &task_tracker);
  mock_time_task_runner_->RunUntilIdle();

  WaitForURLsDeletedNotification(local_history_.get());

  // Run GetPrintJobs again and verify that the print job has been deleted.
  RunGetPrintJobs();
  EXPECT_EQ(0u, entries_.size());
}

TEST_F(PrintingManagerTest, PolicyPreventsDeletingBrowserHistoryDeletingJobs) {
  test_prefs_.SetBoolean(prefs::kDeletePrintJobHistoryAllowed, false);

  // Assert no initial print jobs are ongoing or saved.
  RunGetPrintJobs();
  ASSERT_EQ(0u, entries_.size());

  // Store a print job.
  CreateCompletedPrintJob(/*id=*/0);
  RunGetPrintJobs();
  EXPECT_EQ(1u, entries_.size());

  // Simulate deleting all history, expect print job history to not be deleted.
  base::CancelableTaskTracker task_tracker;
  local_history_->ExpireHistoryBetween(
      std::set<GURL>(), history::kNoAppIdFilter, base::Time(), base::Time(),
      /*user_initiated*/ true, base::DoNothing(), &task_tracker);
  mock_time_task_runner_->RunUntilIdle();

  WaitForURLsDeletedNotification(local_history_.get());

  // Run GetPrintJobs again and verify that the print job has not been deleted.
  RunGetPrintJobs();
  EXPECT_EQ(1u, entries_.size());
}

TEST_F(PrintingManagerTest, ResetReceiverOnBindInterface) {
  // This test simulates a user refreshing the WebUI page. The receiver should
  // be reset before binding the new receiver. Otherwise we would get a DCHECK
  // error from mojo::Receiver
  mojo::Remote<
      chromeos::printing::printing_manager::mojom::PrintingMetadataProvider>
      remote;
  printing_manager_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  remote.reset();

  printing_manager_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
}
}  // namespace print_management
}  // namespace printing
}  // namespace ash
