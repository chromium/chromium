// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_database_impl.h"

#include "base/containers/contains.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using printing::proto::PrintJobInfo;

constexpr char kId1[] = "id1";
constexpr char kId2[] = "id2";
constexpr char kTitle1[] = "title1";
constexpr char kTitle2[] = "title2";

PrintJobInfo ConstructPrintJobInfo(const std::string& id,
                                   const std::string& title) {
  PrintJobInfo print_job_info;
  print_job_info.set_id(id);
  print_job_info.set_title(title);
  return print_job_info;
}

}  // namespace

class PrintJobDatabaseImplTest : public ::testing::Test {
 public:
  PrintJobDatabaseImplTest() = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    database_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    print_job_database_ = std::make_unique<PrintJobDatabaseImpl>(
        database_provider_.get(), temp_dir_.GetPath());
  }

  void TearDown() override {
    database_provider_.reset();
    print_job_database_.reset();
  }

  void OnInitializedWithClosure(base::RepeatingClosure run_loop_closure,
                                bool success) {
    EXPECT_TRUE(success);
    run_loop_closure.Run();
  }

  void OnPrintJobSaved(base::RepeatingClosure run_loop_closure, bool success) {
    EXPECT_TRUE(success);
    run_loop_closure.Run();
  }

  void OnPrintJobsRetrieved(base::RepeatingClosure run_loop_closure,
                            bool success,
                            std::vector<PrintJobInfo> entries) {
    EXPECT_TRUE(success);
    entries_ = std::move(entries);
    run_loop_closure.Run();
  }

  void OnPrintJobsRetrievedFromDatabase(
      base::RepeatingClosure run_loop_closure,
      bool success,
      std::unique_ptr<std::vector<PrintJobInfo>> entries) {
    EXPECT_TRUE(success);
    entries_ = *entries;
    run_loop_closure.Run();
  }

 protected:
  void Initialize() {
    print_job_database_->Initialize(base::BindOnce(
        &PrintJobDatabaseImplTest::OnInitialized, base::Unretained(this)));
  }

  void OnInitialized(bool success) { EXPECT_TRUE(success); }

  void SavePrintJob(const PrintJobInfo& print_job_info) {
    base::RunLoop run_loop;
    print_job_database_->SavePrintJob(
        print_job_info,
        base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobSaved,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void DeletePrintJobs(const std::vector<std::string>& ids) {
    base::RunLoop run_loop;
    print_job_database_->DeletePrintJobs(
        ids, base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobsDeleted,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void Clear() {
    base::RunLoop run_loop;
    print_job_database_->Clear(
        base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobsDeleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnPrintJobsDeleted(base::RepeatingClosure run_loop_closure,
                          bool success) {
    EXPECT_TRUE(success);
    run_loop_closure.Run();
  }

  std::vector<PrintJobInfo> GetPrintJobs() {
    base::RunLoop run_loop;
    print_job_database_->GetPrintJobs(
        base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobsRetrieved,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return entries_;
  }

  std::vector<PrintJobInfo> GetPrintJobsFromProtoDatabase() {
    base::RunLoop run_loop;
    print_job_database_->GetPrintJobsFromProtoDatabase(base::BindOnce(
        &PrintJobDatabaseImplTest::OnPrintJobsRetrievedFromDatabase,
        base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return entries_;
  }

  const std::vector<PrintJobInfo>& entries() { return entries_; }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrintJobDatabaseImpl> print_job_database_;
  base::HistogramTester histogram_tester_;

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> database_provider_;
  std::vector<PrintJobInfo> entries_;
};

TEST_F(PrintJobDatabaseImplTest, Initialize) {
  base::RunLoop run_loop;
  print_job_database_->Initialize(
      base::BindOnce(&PrintJobDatabaseImplTest::OnInitializedWithClosure,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(print_job_database_->IsInitialized());
}

TEST_F(PrintJobDatabaseImplTest, SavePrintJob) {
  Initialize();
  PrintJobInfo print_job_info = ConstructPrintJobInfo(kId1, kTitle1);
  SavePrintJob(print_job_info);
  std::vector<PrintJobInfo> entries = GetPrintJobs();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kId1, entries[0].id());
  EXPECT_EQ(kTitle1, entries[0].title());
}

TEST_F(PrintJobDatabaseImplTest, DeletePrintJobs) {
  Initialize();
  PrintJobInfo print_job_info1 = ConstructPrintJobInfo(kId1, kTitle1);
  SavePrintJob(print_job_info1);
  PrintJobInfo print_job_info2 = ConstructPrintJobInfo(kId2, kTitle2);
  SavePrintJob(print_job_info2);
  DeletePrintJobs({kId1});
  std::vector<PrintJobInfo> entries = GetPrintJobs();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kId2, entries[0].id());
  EXPECT_EQ(kTitle2, entries[0].title());
}

TEST_F(PrintJobDatabaseImplTest, Clear) {
  Initialize();
  PrintJobInfo print_job_info1 = ConstructPrintJobInfo(kId1, kTitle1);
  SavePrintJob(print_job_info1);
  PrintJobInfo print_job_info2 = ConstructPrintJobInfo(kId2, kTitle2);
  SavePrintJob(print_job_info2);
  std::vector<PrintJobInfo> entries = GetPrintJobs();
  EXPECT_EQ(2u, entries.size());
  Clear();
  entries = GetPrintJobs();
  EXPECT_EQ(0u, entries.size());
}

TEST_F(PrintJobDatabaseImplTest, GetPrintJobsFromDatabase) {
  Initialize();
  PrintJobInfo print_job_info = ConstructPrintJobInfo(kId1, kTitle1);
  SavePrintJob(print_job_info);
  std::vector<PrintJobInfo> entries = GetPrintJobsFromProtoDatabase();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kId1, entries[0].id());
  EXPECT_EQ(kTitle1, entries[0].title());
}

TEST_F(PrintJobDatabaseImplTest, TwoSimultaneousSavePrintJobRequests) {
  Initialize();
  PrintJobInfo print_job_info = ConstructPrintJobInfo(kId1, kTitle1);
  PrintJobInfo print_job_info2 = ConstructPrintJobInfo(kId2, kTitle2);

  base::RunLoop run_loop;
  base::RunLoop run_loop2;
  print_job_database_->SavePrintJob(
      print_job_info,
      base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobSaved,
                     base::Unretained(this), run_loop.QuitClosure()));
  print_job_database_->SavePrintJob(
      print_job_info2,
      base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobSaved,
                     base::Unretained(this), run_loop2.QuitClosure()));
  run_loop.Run();
  run_loop2.Run();

  std::vector<PrintJobInfo> entries = GetPrintJobsFromProtoDatabase();
  ASSERT_EQ(2u, entries.size());
  std::vector<std::string> ids = {entries[0].id(), entries[1].id()};
  EXPECT_TRUE(base::Contains(ids, kId1));
  EXPECT_TRUE(base::Contains(ids, kId2));
}

TEST_F(PrintJobDatabaseImplTest, RequestsBeforeInitialization) {
  PrintJobInfo print_job_info = ConstructPrintJobInfo(kId1, kTitle1);
  base::RunLoop save_print_job_run_loop;
  print_job_database_->SavePrintJob(
      print_job_info, base::BindOnce(&PrintJobDatabaseImplTest::OnPrintJobSaved,
                                     base::Unretained(this),
                                     save_print_job_run_loop.QuitClosure()));
  base::RunLoop get_print_jobs_run_loop;
  print_job_database_->GetPrintJobs(base::BindOnce(
      &PrintJobDatabaseImplTest::OnPrintJobsRetrieved, base::Unretained(this),
      get_print_jobs_run_loop.QuitClosure()));
  Initialize();
  save_print_job_run_loop.Run();
  get_print_jobs_run_loop.Run();

  std::vector<PrintJobInfo> print_job_entries = entries();
  ASSERT_EQ(1u, print_job_entries.size());
  EXPECT_EQ(kId1, print_job_entries[0].id());
  EXPECT_EQ(kTitle1, print_job_entries[0].title());
}

}  // namespace ash
