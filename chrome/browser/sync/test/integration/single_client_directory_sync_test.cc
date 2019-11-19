// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/syncable/directory.h"
#include "sql/test/test_helpers.h"
#include "url/gurl.h"

using base::FileEnumerator;
using base::FilePath;

namespace {

// USS ModelTypeStore uses the same folder as the Directory. However, all of its
// content is in a sub-folder. By not asking for recursive files, this function
// will avoid seeing any of those, and return iff Directory database files still
// exist.
bool FolderContainsFiles(const FilePath& folder) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (base::DirectoryExists(folder)) {
    return !FileEnumerator(folder, false, FileEnumerator::FILES).Next().empty();
  } else {
    return false;
  }
}

class SingleClientDirectorySyncTest : public SyncTest {
 public:
  SingleClientDirectorySyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientDirectorySyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientDirectorySyncTest);
};

// A status change checker that waits for an unrecoverable sync error to occur.
class SyncUnrecoverableErrorChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncUnrecoverableErrorChecker(syncer::ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for a Sync Unrecoverable Error";
    return service()->HasUnrecoverableError();
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientDirectorySyncTest,
                       StopThenDisableDeletesDirectory) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  syncer::ProfileSyncService* sync_service = GetSyncService(0);
  FilePath directory_path = sync_service->GetSyncClientForTest()
                                ->GetSyncDataPath();
  ASSERT_TRUE(FolderContainsFiles(directory_path));
  sync_service->StopAndClear();

  // Wait for StartupController::StartUp()'s tasks to finish.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  // Wait for the directory deletion to finish.
  sync_service->FlushBackendTaskRunnerForTest();
  EXPECT_FALSE(FolderContainsFiles(directory_path));
}

}  // namespace
