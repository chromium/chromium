// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_DATABASE_IMPL_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_DATABASE_IMPL_H_

#include <queue>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/history/print_job_database.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace base {
class FilePath;
}  // namespace base

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace ash {

class PrintJobDatabaseImpl : public PrintJobDatabase {
 public:
  PrintJobDatabaseImpl(leveldb_proto::ProtoDatabaseProvider* database_provider,
                       base::FilePath profile_path);

  PrintJobDatabaseImpl(const PrintJobDatabaseImpl&) = delete;
  PrintJobDatabaseImpl& operator=(const PrintJobDatabaseImpl&) = delete;

  ~PrintJobDatabaseImpl() override;

  // PrintJobDatabase:
  void Initialize(InitializeCallback callback) override;
  bool IsInitialized() override;
  void SavePrintJob(const printing::proto::PrintJobInfo& print_job_info,
                    SavePrintJobCallback callback) override;
  void DeletePrintJobs(const std::vector<std::string>& ids,
                       DeletePrintJobsCallback callback) override;
  void Clear(DeletePrintJobsCallback callback) override;
  void GetPrintJobs(GetPrintJobsCallback callback) override;

 private:
  friend class PrintJobDatabaseImplTest;

  enum class InitStatus { UNINITIALIZED, PENDING, INITIALIZED, FAILED };

  void OnInitialized(InitializeCallback callback,
                     leveldb_proto::Enums::InitStatus status);

  void OnKeysAndEntriesLoaded(
      InitializeCallback callback,
      bool success,
      std::unique_ptr<std::map<std::string, printing::proto::PrintJobInfo>>
          entries);

  void FinishInitialization(InitializeCallback callback, bool success);

  void OnPrintJobSaved(const printing::proto::PrintJobInfo& print_job_info,
                       SavePrintJobCallback callback,
                       bool success);

  void OnPrintJobsDeleted(const std::vector<std::string>& ids,
                          DeletePrintJobsCallback callback,
                          bool success);

  void GetPrintJobsFromProtoDatabase(
      GetPrintJobsFromProtoDatabaseCallback callback);

  void OnPrintJobsRetrieved(GetPrintJobsCallback callback,
                            bool success,
                            std::vector<printing::proto::PrintJobInfo> entries);

  void OnPrintJobRetrievedFromDatabase(
      GetPrintJobsFromProtoDatabaseCallback callback,
      bool success,
      std::unique_ptr<std::vector<printing::proto::PrintJobInfo>> entries);

  // The persistent ProtoDatabase for storing print job information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<printing::proto::PrintJobInfo>>
      database_;

  // Cached PrintJobInfo entries.
  std::unordered_map<std::string, printing::proto::PrintJobInfo> cache_;

  // Indicates the status of database initialization.
  InitStatus init_status_;

  // Number of initialize attempts.
  int initialize_attempts_ = 0;

  // Stores callbacks for delayed execution once database is initialized.
  std::queue<base::OnceClosure> deferred_callbacks_;

  base::WeakPtrFactory<PrintJobDatabaseImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_DATABASE_IMPL_H_
