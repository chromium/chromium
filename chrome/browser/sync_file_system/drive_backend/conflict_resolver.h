// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CONFLICT_RESOLVER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CONFLICT_RESOLVER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "google_apis/drive/drive_api_error_codes.h"

namespace drive {
class DriveServiceInterface;
}

namespace google_apis {
class FileResource;
}

namespace sync_file_system {
namespace drive_backend {

class MetadataDatabase;
class SyncEngineContext;
class TrackerIDSet;

// Resolves server side file confliction.
// If a remote file has an active tracker and multiple managed parents,
// ConflictResolver detaches the file from all parents other than the parent
// of the active tracker.
// If multiple trackers have the same local path or the same remote file,
// ConflictResolver picks up one of them and delete others.
class ConflictResolver : public SyncTask {
 public:
  typedef std::vector<std::string> FileIDList;

  explicit ConflictResolver(SyncEngineContext* sync_context);
  ~ConflictResolver() override;
  void RunPreflight(std::unique_ptr<SyncTaskToken> token) override;
  void RunExclusive(std::unique_ptr<SyncTaskToken> token);

 private:
  typedef std::pair<std::string, std::string> FileIDAndETag;

  void DetachFromNonPrimaryParents(std::unique_ptr<SyncTaskToken> token);
  void DidDetachFromParent(std::unique_ptr<SyncTaskToken> token,
                           google_apis::DriveApiErrorCode error);

  std::string PickPrimaryFile(const TrackerIDSet& trackers);
  void RemoveNonPrimaryFiles(std::unique_ptr<SyncTaskToken> token);
  void DidRemoveFile(std::unique_ptr<SyncTaskToken> token,
                     const std::string& file_id,
                     google_apis::DriveApiErrorCode error);

  void UpdateFileMetadata(const std::string& file_id,
                          std::unique_ptr<SyncTaskToken> token);
  void DidGetRemoteMetadata(const std::string& file_id,
                            std::unique_ptr<SyncTaskToken> token,
                            google_apis::DriveApiErrorCode error,
                            std::unique_ptr<google_apis::FileResource> entry);

  std::string target_file_id_;
  std::vector<std::string> parents_to_remove_;

  std::vector<FileIDAndETag> non_primary_file_ids_;
  FileIDList deleted_file_ids_;

  bool IsContextReady();
  drive::DriveServiceInterface* drive_service();
  MetadataDatabase* metadata_database();

  SyncEngineContext* sync_context_;  // Not owned.

  base::WeakPtrFactory<ConflictResolver> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ConflictResolver);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CONFLICT_RESOLVER_H_
