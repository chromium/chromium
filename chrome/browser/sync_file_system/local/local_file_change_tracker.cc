// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_status.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

using storage::FileSystemContext;
using storage::FileSystemFileUtil;
using storage::FileSystemOperationContext;
using storage::FileSystemURL;
using storage::FileSystemURLSet;

namespace sync_file_system {

namespace {
const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("LocalFileChangeTracker");
const char kMark[] = "d";
}  // namespace

// A database class that stores local file changes in a local database. This
// object must be destructed on file_task_runner.
class LocalFileChangeTracker::TrackerDB {
 public:
  TrackerDB(const base::FilePath& base_path,
            leveldb::Env* env_override);

  TrackerDB(const TrackerDB&) = delete;
  TrackerDB& operator=(const TrackerDB&) = delete;

  SyncStatusCode MarkDirty(const std::string& url);
  SyncStatusCode ClearDirty(const std::string& url);
  SyncStatusCode GetDirtyEntries(base::queue<FileSystemURL>* dirty_files);
  SyncStatusCode WriteBatch(std::unique_ptr<leveldb::WriteBatch> batch);

 private:
  enum RecoveryOption {
    REPAIR_ON_CORRUPTION,
    FAIL_ON_CORRUPTION,
  };

  SyncStatusCode Init(RecoveryOption recovery_option);
  SyncStatusCode Repair(const std::string& db_path);
  void HandleError(const base::Location& from_here,
                   const leveldb::Status& status);

  const base::FilePath base_path_;
  raw_ptr<leveldb::Env> env_override_;
  std::unique_ptr<leveldb::DB> db_;
  SyncStatusCode db_status_;
};

LocalFileChangeTracker::ChangeInfo::ChangeInfo() : change_seq(-1) {}
LocalFileChangeTracker::ChangeInfo::~ChangeInfo() {}

// LocalFileChangeTracker ------------------------------------------------------

LocalFileChangeTracker::LocalFileChangeTracker(
    const base::FilePath& base_path,
    leveldb::Env* env_override,
    base::SequencedTaskRunner* file_task_runner)
    : initialized_(false),
      file_task_runner_(file_task_runner),
      tracker_db_(std::make_unique<TrackerDB>(base_path, env_override)),
      current_change_seq_number_(0),
      num_changes_(0) {}

LocalFileChangeTracker::~LocalFileChangeTracker() {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  tracker_db_.reset();
}

void LocalFileChangeTracker::OnStartUpdate(const FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  if (base::Contains(changes_, url) || base::Contains(demoted_changes_, url)) {
    return;
  }
  // TODO(nhiroki): propagate the error code (see http://crbug.com/152127).
  MarkDirtyOnDatabase(url);
}

void LocalFileChangeTracker::OnEndUpdate(const FileSystemURL& url) {}

void LocalFileChangeTracker::OnCreateFile(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               SYNC_FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnCreateFileFrom(const FileSystemURL& url,
                                              const FileSystemURL& src) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               SYNC_FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnMoveFileFrom(const FileSystemURL& url,
                                            const FileSystemURL& src) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               SYNC_FILE_TYPE_FILE));
  RecordChange(src,
               FileChange(FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnRemoveFile(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_DELETE,
                               SYNC_FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnModifyFile(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               SYNC_FILE_TYPE_FILE));
}

void LocalFileChangeTracker::OnCreateDirectory(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                               SYNC_FILE_TYPE_DIRECTORY));
}

void LocalFileChangeTracker::OnRemoveDirectory(const FileSystemURL& url) {
  RecordChange(url, FileChange(FileChange::FILE_CHANGE_DELETE,
                               SYNC_FILE_TYPE_DIRECTORY));
}

void LocalFileChangeTracker::GetNextChangedURLs(
    base::circular_deque<FileSystemURL>* urls,
    int max_urls) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(urls);
  urls->clear();
  // Mildly prioritizes the URLs that older changes and have not been updated
  // for a while.
  for (auto iter = change_seqs_.begin();
       iter != change_seqs_.end() &&
       (max_urls == 0 || urls->size() < static_cast<size_t>(max_urls));
       ++iter) {
    urls->push_back(iter->second);
  }
}

void LocalFileChangeTracker::GetChangesForURL(
    const FileSystemURL& url, FileChangeList* changes) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(changes);
  changes->clear();
  auto found = changes_.find(url);
  if (found == changes_.end()) {
    found = demoted_changes_.find(url);
    if (found == demoted_changes_.end())
      return;
  }
  *changes = found->second.change_list;
}

void LocalFileChangeTracker::ClearChangesForURL(const FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  ClearDirtyOnDatabase(url);
  mirror_changes_.erase(url);
  demoted_changes_.erase(url);
  auto found = changes_.find(url);
  if (found == changes_.end())
    return;
  change_seqs_.erase(found->second.change_seq);
  changes_.erase(found);
  UpdateNumChanges();
}

void LocalFileChangeTracker::CreateFreshMirrorForURL(
    const storage::FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!base::Contains(mirror_changes_, url));
  mirror_changes_[url] = ChangeInfo();
}

void LocalFileChangeTracker::RemoveMirrorAndCommitChangesForURL(
    const storage::FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  auto found = mirror_changes_.find(url);
  if (found == mirror_changes_.end())
    return;
  mirror_changes_.erase(found);

  if (base::Contains(changes_, url) || base::Contains(demoted_changes_, url)) {
    MarkDirtyOnDatabase(url);
  } else {
    ClearDirtyOnDatabase(url);
  }
  UpdateNumChanges();
}

void LocalFileChangeTracker::ResetToMirrorAndCommitChangesForURL(
    const storage::FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  auto found = mirror_changes_.find(url);
  if (found == mirror_changes_.end() || found->second.change_list.empty()) {
    ClearChangesForURL(url);
    return;
  }
  const ChangeInfo& info = found->second;
  if (base::Contains(demoted_changes_, url)) {
    DCHECK(!base::Contains(changes_, url));
    demoted_changes_[url] = info;
  } else {
    DCHECK(!base::Contains(demoted_changes_, url));
    change_seqs_[info.change_seq] = url;
    changes_[url] = info;
  }
  RemoveMirrorAndCommitChangesForURL(url);
}

void LocalFileChangeTracker::DemoteChangesForURL(
    const storage::FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  auto found = changes_.find(url);
  if (found == changes_.end())
    return;
  DCHECK(!base::Contains(demoted_changes_, url));
  change_seqs_.erase(found->second.change_seq);
  demoted_changes_.insert(*found);
  changes_.erase(found);
  UpdateNumChanges();
}

void LocalFileChangeTracker::PromoteDemotedChangesForURL(
    const storage::FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  auto iter = demoted_changes_.find(url);
  if (iter == demoted_changes_.end())
    return;

  FileChangeList::List change_list = iter->second.change_list.list();
  // Make sure that this URL is in no queues.
  DCHECK(!base::Contains(change_seqs_, iter->second.change_seq));
  DCHECK(!base::Contains(changes_, url));

  change_seqs_[iter->second.change_seq] = url;
  changes_.insert(*iter);
  demoted_changes_.erase(iter);
  UpdateNumChanges();
}

bool LocalFileChangeTracker::PromoteDemotedChanges() {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  if (demoted_changes_.empty())
    return false;
  while (!demoted_changes_.empty()) {
    storage::FileSystemURL url = demoted_changes_.begin()->first;
    PromoteDemotedChangesForURL(url);
  }
  UpdateNumChanges();
  return true;
}

SyncStatusCode LocalFileChangeTracker::Initialize(
    FileSystemContext* file_system_context) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!initialized_);
  DCHECK(file_system_context);

  SyncStatusCode status = CollectLastDirtyChanges(file_system_context);
  if (status == SYNC_STATUS_OK)
    initialized_ = true;
  return status;
}

void LocalFileChangeTracker::ResetForFileSystem(const GURL& origin,
                                                storage::FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  auto batch = std::make_unique<leveldb::WriteBatch>();
  for (auto iter = changes_.begin(); iter != changes_.end();) {
    storage::FileSystemURL url = iter->first;
    int change_seq = iter->second.change_seq;
    // Advance |iter| before calling ResetForURL to avoid the iterator
    // invalidation in it.
    ++iter;
    if (url.origin().GetURL() == origin && url.type() == type)
      ResetForURL(url, change_seq, batch.get());
  }

  for (auto iter = demoted_changes_.begin(); iter != demoted_changes_.end();) {
    storage::FileSystemURL url = iter->first;
    int change_seq = iter->second.change_seq;
    // Advance |iter| before calling ResetForURL to avoid the iterator
    // invalidation in it.
    ++iter;
    if (url.origin().GetURL() == origin && url.type() == type)
      ResetForURL(url, change_seq, batch.get());
  }

  // Fail to apply batch to database wouldn't have critical effect, they'll be
  // just marked deleted on next relaunch.
  tracker_db_->WriteBatch(std::move(batch));
  UpdateNumChanges();
}

void LocalFileChangeTracker::UpdateNumChanges() {
  base::AutoLock lock(num_changes_lock_);
  num_changes_ = static_cast<int64_t>(change_seqs_.size());
}

void LocalFileChangeTracker::GetAllChangedURLs(FileSystemURLSet* urls) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  base::circular_deque<FileSystemURL> url_deque;
  GetNextChangedURLs(&url_deque, 0);
  urls->clear();
  urls->insert(url_deque.begin(), url_deque.end());
}

void LocalFileChangeTracker::DropAllChanges() {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  changes_.clear();
  change_seqs_.clear();
  mirror_changes_.clear();
  UpdateNumChanges();
}

SyncStatusCode LocalFileChangeTracker::MarkDirtyOnDatabase(
    const FileSystemURL& url) {
  std::string serialized_url;
  if (!SerializeSyncableFileSystemURL(url, &serialized_url))
    return SYNC_FILE_ERROR_INVALID_URL;

  return tracker_db_->MarkDirty(serialized_url);
}

SyncStatusCode LocalFileChangeTracker::ClearDirtyOnDatabase(
    const FileSystemURL& url) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  std::string serialized_url;
  if (!SerializeSyncableFileSystemURL(url, &serialized_url))
    return SYNC_FILE_ERROR_INVALID_URL;

  return tracker_db_->ClearDirty(serialized_url);
}

SyncStatusCode LocalFileChangeTracker::CollectLastDirtyChanges(
    FileSystemContext* file_system_context) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  base::queue<FileSystemURL> dirty_files;
  const SyncStatusCode status = tracker_db_->GetDirtyEntries(&dirty_files);
  if (status != SYNC_STATUS_OK)
    return status;

  FileSystemFileUtil* file_util =
      file_system_context->sandbox_delegate()->sync_file_util();
  DCHECK(file_util);
  std::unique_ptr<FileSystemOperationContext> context(
      std::make_unique<FileSystemOperationContext>(file_system_context));

  base::File::Info file_info;
  base::FilePath platform_path;

  while (!dirty_files.empty()) {
    const FileSystemURL url = dirty_files.front();
    dirty_files.pop();
    DCHECK_EQ(url.type(), storage::kFileSystemTypeSyncable);

    switch (file_util->GetFileInfo(context.get(), url, &file_info,
                                   &platform_path)) {
      case base::File::FILE_OK: {
        if (!file_info.is_directory) {
          RecordChange(url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                       SYNC_FILE_TYPE_FILE));
          break;
        }

        RecordChange(url, FileChange(
            FileChange::FILE_CHANGE_ADD_OR_UPDATE,
            SYNC_FILE_TYPE_DIRECTORY));

        // Push files and directories in this directory into |dirty_files|.
        std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
            file_util->CreateFileEnumerator(context.get(), url, false));
        base::FilePath path_each;
        while (!(path_each = enumerator->Next()).empty()) {
          dirty_files.push(
              CreateSyncableFileSystemURL(url.origin().GetURL(), path_each));
        }
        break;
      }
      case base::File::FILE_ERROR_NOT_FOUND: {
        // File represented by |url| has already been deleted. Since we cannot
        // figure out if this file was directory or not from the URL, file
        // type is treated as SYNC_FILE_TYPE_UNKNOWN.
        //
        // NOTE: Directory to have been reverted (that is, ADD -> DELETE) is
        // also treated as FILE_CHANGE_DELETE.
        RecordChange(url, FileChange(FileChange::FILE_CHANGE_DELETE,
                                     SYNC_FILE_TYPE_UNKNOWN));
        break;
      }
      case base::File::FILE_ERROR_FAILED:
      default:
        // TODO(nhiroki): handle file access error (http://crbug.com/155251).
        LOG(WARNING) << "Failed to access local file.";
        break;
    }
  }
  return SYNC_STATUS_OK;
}

void LocalFileChangeTracker::RecordChange(
    const FileSystemURL& url, const FileChange& change) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  int change_seq = current_change_seq_number_++;
  if (base::Contains(demoted_changes_, url)) {
    RecordChangeToChangeMaps(url, change, change_seq,
                             &demoted_changes_, nullptr);
  } else {
    RecordChangeToChangeMaps(url, change, change_seq, &changes_, &change_seqs_);
  }
  if (base::Contains(mirror_changes_, url)) {
    RecordChangeToChangeMaps(url, change, change_seq, &mirror_changes_,
                             nullptr);
  }
  UpdateNumChanges();
}

// static
void LocalFileChangeTracker::RecordChangeToChangeMaps(
    const FileSystemURL& url,
    const FileChange& change,
    int new_change_seq,
    FileChangeMap* changes,
    ChangeSeqMap* change_seqs) {
  ChangeInfo& info = (*changes)[url];
  if (info.change_seq >= 0 && change_seqs)
    change_seqs->erase(info.change_seq);
  info.change_list.Update(change);
  if (info.change_list.empty()) {
    changes->erase(url);
    return;
  }
  info.change_seq = new_change_seq;
  if (change_seqs)
    (*change_seqs)[info.change_seq] = url;
}

void LocalFileChangeTracker::ResetForURL(const storage::FileSystemURL& url,
                                         int change_seq,
                                         leveldb::WriteBatch* batch) {
  mirror_changes_.erase(url);
  demoted_changes_.erase(url);
  change_seqs_.erase(change_seq);
  changes_.erase(url);

  std::string serialized_url;
  if (!SerializeSyncableFileSystemURL(url, &serialized_url)) {
    NOTREACHED_IN_MIGRATION() << "Failed to serialize: " << url.DebugString();
    return;
  }
  batch->Delete(serialized_url);
}

// TrackerDB -------------------------------------------------------------------

LocalFileChangeTracker::TrackerDB::TrackerDB(const base::FilePath& base_path,
                                             leveldb::Env* env_override)
  : base_path_(base_path),
    env_override_(env_override),
    db_status_(SYNC_STATUS_OK) {}

SyncStatusCode LocalFileChangeTracker::TrackerDB::Init(
    RecoveryOption recovery_option) {
  if (db_.get() && db_status_ == SYNC_STATUS_OK)
    return SYNC_STATUS_OK;

  std::string path =
      storage::FilePathToString(base_path_.Append(kDatabaseName));
  leveldb_env::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;
  if (env_override_)
    options.env = env_override_;
  leveldb::Status status = leveldb_env::OpenDB(options, path, &db_);
  if (status.ok()) {
    return SYNC_STATUS_OK;
  }

  HandleError(FROM_HERE, status);
  if (!status.IsCorruption())
    return LevelDBStatusToSyncStatusCode(status);

  // Try to repair the corrupted DB.
  switch (recovery_option) {
    case FAIL_ON_CORRUPTION:
      return SYNC_DATABASE_ERROR_CORRUPTION;
    case REPAIR_ON_CORRUPTION:
      return Repair(path);
  }
  NOTREACHED_IN_MIGRATION();
  return SYNC_DATABASE_ERROR_FAILED;
}

SyncStatusCode LocalFileChangeTracker::TrackerDB::Repair(
    const std::string& db_path) {
  DCHECK(!db_.get());
  LOG(WARNING) << "Attempting to repair TrackerDB.";

  leveldb_env::Options options;
  options.reuse_logs = false;  // Compact log file if repairing.
  options.max_open_files = 0;  // Use minimum.
  if (leveldb::RepairDB(db_path, options).ok() &&
      Init(FAIL_ON_CORRUPTION) == SYNC_STATUS_OK) {
    // TODO(nhiroki): perform some consistency checks between TrackerDB and
    // syncable file system.
    LOG(WARNING) << "Repairing TrackerDB completed.";
    return SYNC_STATUS_OK;
  }

  LOG(WARNING) << "Failed to repair TrackerDB.";
  return SYNC_DATABASE_ERROR_CORRUPTION;
}

// TODO(nhiroki): factor out the common methods into somewhere else.
void LocalFileChangeTracker::TrackerDB::HandleError(
    const base::Location& from_here,
    const leveldb::Status& status) {
  LOG(ERROR) << "LocalFileChangeTracker::TrackerDB failed at: "
             << from_here.ToString() << " with error: " << status.ToString();
}

SyncStatusCode LocalFileChangeTracker::TrackerDB::MarkDirty(
    const std::string& url) {
  if (db_status_ != SYNC_STATUS_OK)
    return db_status_;

  db_status_ = Init(REPAIR_ON_CORRUPTION);
  if (db_status_ != SYNC_STATUS_OK) {
    db_.reset();
    return db_status_;
  }

  leveldb::Status status = db_->Put(leveldb::WriteOptions(), url, kMark);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    db_status_ = LevelDBStatusToSyncStatusCode(status);
    db_.reset();
    return db_status_;
  }
  return SYNC_STATUS_OK;
}

SyncStatusCode LocalFileChangeTracker::TrackerDB::ClearDirty(
    const std::string& url) {
  if (db_status_ != SYNC_STATUS_OK)
    return db_status_;

  // Should not reach here before initializing the database. The database should
  // be cleared after read, and should be initialized during read if
  // uninitialized.
  DCHECK(db_.get());

  leveldb::Status status = db_->Delete(leveldb::WriteOptions(), url);
  if (!status.ok() && !status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    db_status_ = LevelDBStatusToSyncStatusCode(status);
    db_.reset();
    return db_status_;
  }
  return SYNC_STATUS_OK;
}

SyncStatusCode LocalFileChangeTracker::TrackerDB::GetDirtyEntries(
    base::queue<FileSystemURL>* dirty_files) {
  if (db_status_ != SYNC_STATUS_OK)
    return db_status_;

  db_status_ = Init(REPAIR_ON_CORRUPTION);
  if (db_status_ != SYNC_STATUS_OK) {
    db_.reset();
    return db_status_;
  }

  std::unique_ptr<leveldb::Iterator> iter(
      db_->NewIterator(leveldb::ReadOptions()));
  iter->SeekToFirst();
  FileSystemURL url;
  while (iter->Valid()) {
    if (!DeserializeSyncableFileSystemURL(iter->key().ToString(), &url)) {
      LOG(WARNING) << "Failed to deserialize an URL. "
                   << "TrackerDB might be corrupted.";
      db_status_ = SYNC_DATABASE_ERROR_CORRUPTION;
      iter.reset();  // Must delete before closing the database.
      db_.reset();
      return db_status_;
    }
    dirty_files->push(url);
    iter->Next();
  }
  return SYNC_STATUS_OK;
}

SyncStatusCode LocalFileChangeTracker::TrackerDB::WriteBatch(
    std::unique_ptr<leveldb::WriteBatch> batch) {
  if (db_status_ != SYNC_STATUS_OK)
    return db_status_;

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), batch.get());
  if (!status.ok() && !status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    db_status_ = LevelDBStatusToSyncStatusCode(status);
    db_.reset();
    return db_status_;
  }
  return SYNC_STATUS_OK;
}

}  // namespace sync_file_system
