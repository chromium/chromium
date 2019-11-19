// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/canned_syncable_file_system.h"

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/test/mock_blob_util.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using base::File;
using storage::FileSystemContext;
using storage::FileSystemOperationRunner;
using storage::FileSystemURL;
using storage::FileSystemURLSet;
using storage::QuotaManager;
using storage::ScopedTextBlob;

namespace sync_file_system {

namespace {

template <typename R>
void AssignAndQuit(base::TaskRunner* original_task_runner,
                   const base::Closure& quit_closure,
                   R* result_out, R result) {
  DCHECK(result_out);
  *result_out = std::forward<R>(result);
  original_task_runner->PostTask(FROM_HERE, quit_closure);
}

template <typename R, typename CallbackType>
R RunOnThread(base::SingleThreadTaskRunner* task_runner,
              const base::Location& location,
              base::OnceCallback<void(CallbackType callback)> task) {
  R result;
  base::RunLoop run_loop;
  task_runner->PostTask(
      location,
      base::BindOnce(
          std::move(task),
          base::Bind(&AssignAndQuit<R>,
                     base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                     run_loop.QuitClosure(), base::Unretained(&result))));
  run_loop.Run();
  return result;
}

void RunOnThread(base::SingleThreadTaskRunner* task_runner,
                 const base::Location& location,
                 base::OnceClosure task) {
  base::RunLoop run_loop;
  task_runner->PostTaskAndReply(
      location, std::move(task),
      base::BindOnce(
          base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
          base::ThreadTaskRunnerHandle::Get(), FROM_HERE,
          run_loop.QuitClosure()));
  run_loop.Run();
}

void EnsureRunningOn(base::SingleThreadTaskRunner* runner) {
  EXPECT_TRUE(runner->RunsTasksInCurrentSequence());
}

void VerifySameTaskRunner(
    base::SingleThreadTaskRunner* runner1,
    base::SingleThreadTaskRunner* runner2) {
  ASSERT_TRUE(runner1 != nullptr);
  ASSERT_TRUE(runner2 != nullptr);
  runner1->PostTask(
      FROM_HERE, base::BindOnce(&EnsureRunningOn, base::RetainedRef(runner2)));
}

void OnCreateSnapshotFileAndVerifyData(
    const std::string& expected_data,
    const CannedSyncableFileSystem::StatusCallback& callback,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    scoped_refptr<storage::ShareableFileReference> /* file_ref */) {
  if (result != base::File::FILE_OK) {
    callback.Run(result);
    return;
  }
  EXPECT_EQ(expected_data.size(), static_cast<size_t>(file_info.size));
  std::string data;
  const bool read_status = base::ReadFileToString(platform_path, &data);
  EXPECT_TRUE(read_status);
  EXPECT_EQ(expected_data, data);
  callback.Run(result);
}

void OnCreateSnapshotFile(
    base::File::Info* file_info_out,
    base::FilePath* platform_path_out,
    const CannedSyncableFileSystem::StatusCallback& callback,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    scoped_refptr<storage::ShareableFileReference> file_ref) {
  DCHECK(!file_ref.get());
  DCHECK(file_info_out);
  DCHECK(platform_path_out);
  *file_info_out = file_info;
  *platform_path_out = platform_path;
  callback.Run(result);
}

void OnReadDirectory(CannedSyncableFileSystem::FileEntryList* entries_out,
                     CannedSyncableFileSystem::StatusCallback callback,
                     base::File::Error error,
                     storage::FileSystemOperation::FileEntryList entries,
                     bool has_more) {
  DCHECK(entries_out);
  entries_out->reserve(entries_out->size() + entries.size());
  std::copy(entries.begin(), entries.end(), std::back_inserter(*entries_out));

  if (!has_more)
    std::move(callback).Run(error);
}

class WriteHelper {
 public:
  WriteHelper() : bytes_written_(0) {}
  WriteHelper(std::unique_ptr<storage::BlobStorageContext> blob_storage_context,
              const std::string& blob_data)
      : bytes_written_(0),
        blob_storage_context_(std::move(blob_storage_context)),
        blob_data_(new ScopedTextBlob(blob_storage_context_.get(),
                                      base::GenerateGUID(),
                                      blob_data)) {}

  ~WriteHelper() {}

  ScopedTextBlob* scoped_text_blob() const { return blob_data_.get(); }

  void DidWrite(const base::Callback<void(int64_t result)>& completion_callback,
                File::Error error,
                int64_t bytes,
                bool complete) {
    if (error == base::File::FILE_OK) {
      bytes_written_ += bytes;
      if (!complete)
        return;
    }
    completion_callback.Run(error == base::File::FILE_OK
                                ? bytes_written_
                                : static_cast<int64_t>(error));
  }

 private:
  int64_t bytes_written_;
  std::unique_ptr<storage::BlobStorageContext> blob_storage_context_;
  std::unique_ptr<ScopedTextBlob> blob_data_;

  DISALLOW_COPY_AND_ASSIGN(WriteHelper);
};

void DidGetUsageAndQuota(storage::StatusCallback callback,
                         int64_t* usage_out,
                         int64_t* quota_out,
                         blink::mojom::QuotaStatusCode status,
                         int64_t usage,
                         int64_t quota) {
  *usage_out = usage;
  *quota_out = quota;
  std::move(callback).Run(status);
}

void EnsureLastTaskRuns(base::SingleThreadTaskRunner* runner) {
  base::RunLoop run_loop;
  runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                           run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

CannedSyncableFileSystem::CannedSyncableFileSystem(
    const GURL& origin,
    bool in_memory_file_system,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner)
    : origin_(origin),
      type_(storage::kFileSystemTypeSyncable),
      result_(base::File::FILE_OK),
      sync_status_(sync_file_system::SYNC_STATUS_OK),
      in_memory_file_system_(in_memory_file_system),
      io_task_runner_(io_task_runner),
      file_task_runner_(file_task_runner),
      is_filesystem_set_up_(false),
      is_filesystem_opened_(false),
      sync_status_observers_(new ObserverList) {}

CannedSyncableFileSystem::~CannedSyncableFileSystem() {}

void CannedSyncableFileSystem::SetUp(QuotaMode quota_mode) {
  ASSERT_FALSE(is_filesystem_set_up_);
  ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

  scoped_refptr<storage::SpecialStoragePolicy> storage_policy =
      new content::MockSpecialStoragePolicy();

  if (quota_mode == QUOTA_ENABLED) {
    quota_manager_ = new QuotaManager(
        false /* is_incognito */, data_dir_.GetPath(), io_task_runner_.get(),
        storage_policy.get(), storage::GetQuotaSettingsFunc());
  }

  std::vector<std::string> additional_allowed_schemes;
  additional_allowed_schemes.push_back(origin_.scheme());
  storage::FileSystemOptions options(
      storage::FileSystemOptions::PROFILE_MODE_NORMAL, in_memory_file_system_,
      additional_allowed_schemes);

  std::vector<std::unique_ptr<storage::FileSystemBackend>> additional_backends;
  additional_backends.push_back(SyncFileSystemBackend::CreateForTesting());

  file_system_context_ = new FileSystemContext(
      io_task_runner_.get(), file_task_runner_.get(),
      storage::ExternalMountPoints::CreateRefCounted().get(),
      storage_policy.get(),
      quota_manager_.get() ? quota_manager_->proxy() : nullptr,
      std::move(additional_backends),
      std::vector<storage::URLRequestAutoMountHandler>(), data_dir_.GetPath(),
      options);

  is_filesystem_set_up_ = true;
}

void CannedSyncableFileSystem::TearDown() {
  quota_manager_ = nullptr;
  file_system_context_ = nullptr;

  // Make sure we give some more time to finish tasks on other threads.
  EnsureLastTaskRuns(io_task_runner_.get());
  EnsureLastTaskRuns(file_task_runner_.get());
  base::ThreadPoolInstance::Get()->FlushForTesting();
}

FileSystemURL CannedSyncableFileSystem::URL(const std::string& path) const {
  EXPECT_TRUE(is_filesystem_set_up_);
  EXPECT_FALSE(root_url_.is_empty());

  GURL url(root_url_.spec() + path);
  return file_system_context_->CrackURL(url);
}

File::Error CannedSyncableFileSystem::OpenFileSystem() {
  EXPECT_TRUE(is_filesystem_set_up_);

  base::RunLoop run_loop;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CannedSyncableFileSystem::DoOpenFileSystem, base::Unretained(this),
          base::Bind(&CannedSyncableFileSystem::DidOpenFileSystem,
                     base::Unretained(this),
                     base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                     run_loop.QuitClosure())));
  run_loop.Run();

  if (backend()->sync_context()) {
    // Register 'this' as a sync status observer.
    RunOnThread(
        io_task_runner_.get(),
        FROM_HERE,
        base::Bind(&CannedSyncableFileSystem::InitializeSyncStatusObserver,
                   base::Unretained(this)));
  }
  return result_;
}

void CannedSyncableFileSystem::AddSyncStatusObserver(
    LocalFileSyncStatus::Observer* observer) {
  sync_status_observers_->AddObserver(observer);
}

void CannedSyncableFileSystem::RemoveSyncStatusObserver(
    LocalFileSyncStatus::Observer* observer) {
  sync_status_observers_->RemoveObserver(observer);
}

SyncStatusCode CannedSyncableFileSystem::MaybeInitializeFileSystemContext(
    LocalFileSyncContext* sync_context) {
  DCHECK(sync_context);
  base::RunLoop run_loop;
  sync_status_ = sync_file_system::SYNC_STATUS_UNKNOWN;
  VerifySameTaskRunner(io_task_runner_.get(),
                       sync_context->io_task_runner_.get());
  sync_context->MaybeInitializeFileSystemContext(
      origin_,
      file_system_context_.get(),
      base::Bind(&CannedSyncableFileSystem::DidInitializeFileSystemContext,
                 base::Unretained(this),
                 run_loop.QuitClosure()));
  run_loop.Run();
  return sync_status_;
}

File::Error CannedSyncableFileSystem::CreateDirectory(
    const FileSystemURL& url) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoCreateDirectory,
                     base::Unretained(this), url));
}

File::Error CannedSyncableFileSystem::CreateFile(const FileSystemURL& url) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoCreateFile,
                     base::Unretained(this), url));
}

File::Error CannedSyncableFileSystem::Copy(
    const FileSystemURL& src_url, const FileSystemURL& dest_url) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoCopy, base::Unretained(this),
                     src_url, dest_url));
}

File::Error CannedSyncableFileSystem::Move(
    const FileSystemURL& src_url, const FileSystemURL& dest_url) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoMove, base::Unretained(this),
                     src_url, dest_url));
}

File::Error CannedSyncableFileSystem::TruncateFile(const FileSystemURL& url,
                                                   int64_t size) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoTruncateFile,
                     base::Unretained(this), url, size));
}

File::Error CannedSyncableFileSystem::TouchFile(
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoTouchFile,
                     base::Unretained(this), url, last_access_time,
                     last_modified_time));
}

File::Error CannedSyncableFileSystem::Remove(
    const FileSystemURL& url, bool recursive) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoRemove,
                     base::Unretained(this), url, recursive));
}

File::Error CannedSyncableFileSystem::FileExists(
    const FileSystemURL& url) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoFileExists,
                     base::Unretained(this), url));
}

File::Error CannedSyncableFileSystem::DirectoryExists(
    const FileSystemURL& url) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoDirectoryExists,
                     base::Unretained(this), url));
}

File::Error CannedSyncableFileSystem::VerifyFile(
    const FileSystemURL& url,
    const std::string& expected_data) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoVerifyFile,
                     base::Unretained(this), url, expected_data));
}

File::Error CannedSyncableFileSystem::GetMetadataAndPlatformPath(
    const FileSystemURL& url,
    base::File::Info* info,
    base::FilePath* platform_path) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoGetMetadataAndPlatformPath,
                     base::Unretained(this), url, info, platform_path));
}

File::Error CannedSyncableFileSystem::ReadDirectory(
    const storage::FileSystemURL& url,
    FileEntryList* entries) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoReadDirectory,
                     base::Unretained(this), url, entries));
}

int64_t CannedSyncableFileSystem::Write(
    const FileSystemURL& url,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
  return RunOnThread<int64_t>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoWrite, base::Unretained(this),
                     url, std::move(blob_data_handle)));
}

int64_t CannedSyncableFileSystem::WriteString(const FileSystemURL& url,
                                              const std::string& data) {
  return RunOnThread<int64_t>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoWriteString,
                     base::Unretained(this), url, data));
}

File::Error CannedSyncableFileSystem::DeleteFileSystem() {
  EXPECT_TRUE(is_filesystem_set_up_);
  return RunOnThread<File::Error>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&FileSystemContext::DeleteFileSystem, file_system_context_,
                     origin_, type_));
}

blink::mojom::QuotaStatusCode CannedSyncableFileSystem::GetUsageAndQuota(
    int64_t* usage,
    int64_t* quota) {
  return RunOnThread<blink::mojom::QuotaStatusCode>(
      io_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CannedSyncableFileSystem::DoGetUsageAndQuota,
                     base::Unretained(this), usage, quota));
}

void CannedSyncableFileSystem::GetChangedURLsInTracker(
    FileSystemURLSet* urls) {
  RunOnThread(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&LocalFileChangeTracker::GetAllChangedURLs,
                     base::Unretained(backend()->change_tracker()), urls));
}

void CannedSyncableFileSystem::ClearChangeForURLInTracker(
    const FileSystemURL& url) {
  RunOnThread(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&LocalFileChangeTracker::ClearChangesForURL,
                     base::Unretained(backend()->change_tracker()), url));
}

void CannedSyncableFileSystem::GetChangesForURLInTracker(
    const FileSystemURL& url,
    FileChangeList* changes) {
  RunOnThread(file_task_runner_.get(), FROM_HERE,
              base::BindOnce(&LocalFileChangeTracker::GetChangesForURL,
                             base::Unretained(backend()->change_tracker()), url,
                             changes));
}

SyncFileSystemBackend* CannedSyncableFileSystem::backend() {
  return SyncFileSystemBackend::GetBackend(file_system_context_.get());
}

FileSystemOperationRunner* CannedSyncableFileSystem::operation_runner() {
  return file_system_context_->operation_runner();
}

void CannedSyncableFileSystem::OnSyncEnabled(const FileSystemURL& url) {
  sync_status_observers_->Notify(
      FROM_HERE, &LocalFileSyncStatus::Observer::OnSyncEnabled, url);
}

void CannedSyncableFileSystem::OnWriteEnabled(const FileSystemURL& url) {
  sync_status_observers_->Notify(
      FROM_HERE, &LocalFileSyncStatus::Observer::OnWriteEnabled, url);
}

void CannedSyncableFileSystem::DoOpenFileSystem(
    OpenFileSystemCallback callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_FALSE(is_filesystem_opened_);
  file_system_context_->OpenFileSystem(
      origin_, type_, storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      std::move(callback));
}

void CannedSyncableFileSystem::DoCreateDirectory(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->CreateDirectory(
      url, false /* exclusive */, false /* recursive */, callback);
}

void CannedSyncableFileSystem::DoCreateFile(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->CreateFile(url, false /* exclusive */, callback);
}

void CannedSyncableFileSystem::DoCopy(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->Copy(
      src_url, dest_url, storage::FileSystemOperation::OPTION_NONE,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      storage::FileSystemOperationRunner::CopyProgressCallback(), callback);
}

void CannedSyncableFileSystem::DoMove(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->Move(
      src_url, dest_url, storage::FileSystemOperation::OPTION_NONE, callback);
}

void CannedSyncableFileSystem::DoTruncateFile(const FileSystemURL& url,
                                              int64_t size,
                                              const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->Truncate(url, size, callback);
}

void CannedSyncableFileSystem::DoTouchFile(
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->TouchFile(url, last_access_time,
                                last_modified_time, callback);
}

void CannedSyncableFileSystem::DoRemove(
    const FileSystemURL& url, bool recursive,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->Remove(url, recursive, callback);
}

void CannedSyncableFileSystem::DoFileExists(
    const FileSystemURL& url, const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->FileExists(url, callback);
}

void CannedSyncableFileSystem::DoDirectoryExists(
    const FileSystemURL& url, const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->DirectoryExists(url, callback);
}

void CannedSyncableFileSystem::DoVerifyFile(
    const FileSystemURL& url,
    const std::string& expected_data,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->CreateSnapshotFile(
      url,
      base::Bind(&OnCreateSnapshotFileAndVerifyData, expected_data, callback));
}

void CannedSyncableFileSystem::DoGetMetadataAndPlatformPath(
    const FileSystemURL& url,
    base::File::Info* info,
    base::FilePath* platform_path,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->CreateSnapshotFile(
      url, base::Bind(&OnCreateSnapshotFile, info, platform_path, callback));
}

void CannedSyncableFileSystem::DoReadDirectory(
    const FileSystemURL& url,
    FileEntryList* entries,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->ReadDirectory(
      url, base::BindRepeating(&OnReadDirectory, entries, callback));
}

void CannedSyncableFileSystem::DoWrite(
    const FileSystemURL& url,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle,
    const WriteCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  WriteHelper* helper = new WriteHelper;
  operation_runner()->Write(
      url, std::move(blob_data_handle), 0,
      base::Bind(&WriteHelper::DidWrite, base::Owned(helper), callback));
}

void CannedSyncableFileSystem::DoWriteString(
    const FileSystemURL& url,
    const std::string& data,
    const WriteCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  auto blob_storage_context = std::make_unique<storage::BlobStorageContext>();
  WriteHelper* helper = new WriteHelper(std::move(blob_storage_context), data);
  operation_runner()->Write(url,
                            helper->scoped_text_blob()->GetBlobDataHandle(), 0,
                            base::BindRepeating(&WriteHelper::DidWrite,
                                                base::Owned(helper), callback));
}

void CannedSyncableFileSystem::DoGetUsageAndQuota(
    int64_t* usage,
    int64_t* quota,
    storage::StatusCallback callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  EXPECT_TRUE(is_filesystem_opened_);
  DCHECK(quota_manager_.get());
  quota_manager_->GetUsageAndQuota(
      url::Origin::Create(origin_), storage_type(),
      base::BindOnce(&DidGetUsageAndQuota, std::move(callback), usage, quota));
}

void CannedSyncableFileSystem::DidOpenFileSystem(
    base::SingleThreadTaskRunner* original_task_runner,
    const base::Closure& quit_closure,
    const GURL& root,
    const std::string& name,
    File::Error result) {
  if (io_task_runner_->RunsTasksInCurrentSequence()) {
    EXPECT_FALSE(is_filesystem_opened_);
    is_filesystem_opened_ = true;
  }
  if (!original_task_runner->RunsTasksInCurrentSequence()) {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    original_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&CannedSyncableFileSystem::DidOpenFileSystem,
                                  base::Unretained(this),
                                  base::RetainedRef(original_task_runner),
                                  quit_closure, root, name, result));
    return;
  }
  result_ = result;
  root_url_ = root;
  quit_closure.Run();
}

void CannedSyncableFileSystem::DidInitializeFileSystemContext(
    const base::Closure& quit_closure,
    SyncStatusCode status) {
  sync_status_ = status;
  quit_closure.Run();
}

void CannedSyncableFileSystem::InitializeSyncStatusObserver() {
  ASSERT_TRUE(io_task_runner_->RunsTasksInCurrentSequence());
  backend()->sync_context()->sync_status()->AddObserver(this);
}

}  // namespace sync_file_system
