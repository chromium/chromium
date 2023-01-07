// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_FAKE_REMOTE_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_FAKE_REMOTE_CHANGE_PROCESSOR_H_

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"

namespace base {
class FilePath;
}

namespace storage {
class FileSystemURL;
}

namespace sync_file_system {

class FileChange;

class FakeRemoteChangeProcessor : public RemoteChangeProcessor {
 public:
  typedef std::map<storage::FileSystemURL,
                   std::vector<FileChange>,
                   storage::FileSystemURL::Comparator> URLToFileChangesMap;
  typedef std::map<storage::FileSystemURL,
                   FileChangeList,
                   storage::FileSystemURL::Comparator> URLToFileChangeList;
  typedef std::map<storage::FileSystemURL,
                   SyncFileMetadata,
                   storage::FileSystemURL::Comparator> URLToFileMetadata;

  FakeRemoteChangeProcessor();

  FakeRemoteChangeProcessor(const FakeRemoteChangeProcessor&) = delete;
  FakeRemoteChangeProcessor& operator=(const FakeRemoteChangeProcessor&) =
      delete;

  ~FakeRemoteChangeProcessor() override;

  // RemoteChangeProcessor overrides.
  void PrepareForProcessRemoteChange(const storage::FileSystemURL& url,
                                     PrepareChangeCallback callback) override;
  void ApplyRemoteChange(const FileChange& change,
                         const base::FilePath& local_path,
                         const storage::FileSystemURL& url,
                         SyncStatusCallback callback) override;
  void FinalizeRemoteSync(const storage::FileSystemURL& url,
                          bool clear_local_changes,
                          base::OnceClosure completion_callback) override;
  void RecordFakeLocalChange(const storage::FileSystemURL& url,
                             const FileChange& change,
                             SyncStatusCallback callback) override;

  void UpdateLocalFileMetadata(const storage::FileSystemURL& url,
                               const FileChange& change);
  void ClearLocalChanges(const storage::FileSystemURL& url);

  const URLToFileChangesMap& GetAppliedRemoteChanges() const;

  // Compare |applied_changes_| with |expected_changes|.
  // This internally calls EXPECT_FOO, ASSERT_FOO methods in the
  // verification.
  void VerifyConsistency(const URLToFileChangesMap& expected_changes);

 private:
  // History of file changes given by ApplyRemoteChange(). Changes are arranged
  // in chronological order, that is, the end of the vector represents the last
  // change.
  URLToFileChangesMap applied_changes_;

  // History of local file changes.
  URLToFileChangeList local_changes_;

  // Initial local file metadata. These are overridden by applied changes.
  URLToFileMetadata local_file_metadata_;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_FAKE_REMOTE_CHANGE_PROCESSOR_H_
