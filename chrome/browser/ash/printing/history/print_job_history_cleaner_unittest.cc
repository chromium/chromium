// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_history_cleaner.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/ash/printing/history/test_print_job_database.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using printing::proto::PrintJobInfo;

constexpr char kId1[] = "id1";
constexpr char kId2[] = "id2";

PrintJobInfo ConstructPrintJobInfo(const std::string& id,
                                   const base::Time& completion_time) {
  PrintJobInfo print_job_info;
  print_job_info.set_id(id);
  print_job_info.set_completion_time(
      static_cast<int64_t>(completion_time.InMillisecondsFSinceUnixEpoch()));
  return print_job_info;
}

}  // namespace

class PrintJobHistoryCleanerTest : public ::testing::Test {
 public:
  PrintJobHistoryCleanerTest() = default;

  void SetUp() override {
    test_prefs_.SetInitializationCompleted();

    print_job_database_ = std::make_unique<TestPrintJobDatabase>();
    print_job_history_cleaner_ = std::make_unique<PrintJobHistoryCleaner>(
        print_job_database_.get(), &test_prefs_);

    print_job_history_cleaner_->SetClockForTesting(
        task_environment_.GetMockClock());

    PrintJobHistoryService::RegisterProfilePrefs(test_prefs_.registry());
  }

  void TearDown() override {}

 protected:
  void SavePrintJob(const PrintJobInfo& print_job_info) {
    base::RunLoop run_loop;
    print_job_database_->SavePrintJob(
        print_job_info,
        base::BindOnce(&PrintJobHistoryCleanerTest::OnPrintJobSaved,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnPrintJobSaved(base::RepeatingClosure run_loop_closure, bool success) {
    EXPECT_TRUE(success);
    run_loop_closure.Run();
  }

  std::vector<PrintJobInfo> GetPrintJobs() {
    base::RunLoop run_loop;
    print_job_database_->GetPrintJobs(
        base::BindOnce(&PrintJobHistoryCleanerTest::OnPrintJobsRetrieved,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return entries_;
  }

  void OnPrintJobsRetrieved(base::RepeatingClosure run_loop_closure,
                            bool success,
                            std::vector<PrintJobInfo> entries) {
    EXPECT_TRUE(success);
    entries_ = std::move(entries);
    run_loop_closure.Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple test_prefs_;
  std::unique_ptr<PrintJobDatabase> print_job_database_;
  std::unique_ptr<PrintJobHistoryCleaner> print_job_history_cleaner_;

 private:
  std::vector<PrintJobInfo> entries_;
};

TEST_F(PrintJobHistoryCleanerTest, CleanExpiredPrintJobs) {
  // Set expiration period to 1 day.
  test_prefs_.SetInteger(prefs::kPrintJobHistoryExpirationPeriod, 1);
  print_job_database_->Initialize(base::DoNothing());

  task_environment_.FastForwardBy(base::Days(300));
  PrintJobInfo print_job_info1 =
      ConstructPrintJobInfo(kId1, task_environment_.GetMockClock()->Now());
  SavePrintJob(print_job_info1);

  task_environment_.FastForwardBy(base::Days(1));
  PrintJobInfo print_job_info2 =
      ConstructPrintJobInfo(kId2, task_environment_.GetMockClock()->Now());
  SavePrintJob(print_job_info2);

  task_environment_.FastForwardBy(base::Hours(12));
  base::RunLoop run_loop1;
  print_job_history_cleaner_->CleanUp(run_loop1.QuitClosure());
  run_loop1.Run();

  std::vector<PrintJobInfo> entries = GetPrintJobs();
  // CleanUp() call should clear the first entry which is expected to expire.
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kId2, entries[0].id());

  task_environment_.FastForwardBy(base::Days(1));
  base::RunLoop run_loop2;
  print_job_history_cleaner_->CleanUp(run_loop2.QuitClosure());
  run_loop2.Run();
  entries = GetPrintJobs();
  // The second entry is expected to be removed after cleaner ran again.
  EXPECT_TRUE(entries.empty());
}

TEST_F(PrintJobHistoryCleanerTest, CleanExpiredPrintJobsAfterPrefChanged) {
  // Set expiration period to 1 day.
  test_prefs_.SetInteger(prefs::kPrintJobHistoryExpirationPeriod, 3);
  print_job_database_->Initialize(base::DoNothing());

  task_environment_.FastForwardBy(base::Days(300));
  PrintJobInfo print_job_info =
      ConstructPrintJobInfo(kId1, task_environment_.GetMockClock()->Now());
  SavePrintJob(print_job_info);

  task_environment_.FastForwardBy(base::Hours(36));
  base::RunLoop run_loop1;
  print_job_history_cleaner_->CleanUp(run_loop1.QuitClosure());
  run_loop1.Run();

  std::vector<PrintJobInfo> entries = GetPrintJobs();
  // CleanUp() shouldn't clear anything.
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kId1, entries[0].id());

  test_prefs_.SetInteger(prefs::kPrintJobHistoryExpirationPeriod, 1);
  base::RunLoop run_loop2;
  print_job_history_cleaner_->CleanUp(run_loop2.QuitClosure());
  run_loop2.Run();
  entries = GetPrintJobs();
  // The entry is expected to be removed after pref has changed.
  EXPECT_TRUE(entries.empty());
}

TEST_F(PrintJobHistoryCleanerTest, StorePrintJobHistoryIndefinite) {
  // Set expiration period policy to store history indefinitely.
  test_prefs_.SetInteger(prefs::kPrintJobHistoryExpirationPeriod, -1);
  print_job_database_->Initialize(base::DoNothing());

  task_environment_.FastForwardBy(base::Days(300));
  PrintJobInfo print_job_info1 =
      ConstructPrintJobInfo(kId1, task_environment_.GetMockClock()->Now());
  SavePrintJob(print_job_info1);

  task_environment_.FastForwardBy(base::Days(300));

  base::RunLoop run_loop;
  print_job_history_cleaner_->CleanUp(run_loop.QuitClosure());
  run_loop.Run();

  std::vector<PrintJobInfo> entries = GetPrintJobs();
  // CleanUp() call shouldn't clear anything as according to pref we store
  // history indefinitely.
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kId1, entries[0].id());
}

}  // namespace ash
