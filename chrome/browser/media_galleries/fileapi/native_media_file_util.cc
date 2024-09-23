// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/native_file_util.h"
#include "url/gurl.h"

namespace {

// Returns true if the current thread is capable of doing IO.
bool IsOnTaskRunnerThread(storage::FileSystemOperationContext* context) {
  return context->task_runner()->RunsTasksInCurrentSequence();
}

base::File::Error IsMediaHeader(const char* buf, size_t length) {
  if (length == 0)
    return base::File::FILE_ERROR_SECURITY;

  std::string mime_type;
  if (!net::SniffMimeTypeFromLocalData(std::string_view(buf, length),
                                       &mime_type)) {
    return base::File::FILE_ERROR_SECURITY;
  }

  if (base::StartsWith(mime_type, "image/", base::CompareCase::SENSITIVE) ||
      base::StartsWith(mime_type, "audio/", base::CompareCase::SENSITIVE) ||
      base::StartsWith(mime_type, "video/", base::CompareCase::SENSITIVE) ||
      mime_type == "application/x-shockwave-flash") {
    return base::File::FILE_OK;
  }
  return base::File::FILE_ERROR_SECURITY;
}

void HoldFileRef(scoped_refptr<storage::ShareableFileReference> file_ref) {}

void DidOpenSnapshot(storage::AsyncFileUtil::CreateOrOpenCallback callback,
                     scoped_refptr<storage::ShareableFileReference> file_ref,
                     base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!file.IsValid()) {
    std::move(callback).Run(std::move(file), base::OnceClosure());
    return;
  }
  std::move(callback).Run(std::move(file),
                          base::BindOnce(&HoldFileRef, std::move(file_ref)));
}

}  // namespace

// |NativeMediaFileUtil::Core| is used and torn-down on the media TaskRunner by
// the owning NativeMediaFileUtil.
class NativeMediaFileUtil::Core {
 public:
  explicit Core(scoped_refptr<base::SequencedTaskRunner> media_task_runner)
      : media_task_runner_(std::move(media_task_runner)) {}

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() = default;

  // The following calls are made on the media TaskRunner, using
  // PostTaskAndReplyWithResult() to return the result to the IO thread.

  // Necessary for copy/move to succeed.
  base::File::Error CreateDirectory(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      bool exclusive,
      bool recursive);

  base::File::Error CopyOrMoveFileLocal(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOptionSet options,
      bool copy);
  base::File::Error CopyInForeignFile(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const base::FilePath& src_file_path,
      const storage::FileSystemURL& dest_url);
  base::File::Error DeleteFile(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url);

  // Necessary for move to succeed.
  base::File::Error DeleteDirectory(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url);

  // The following calls are posted to the media TaskRunner, where they perform
  // the specified operation, before posting |callback| back to the IO thread
  // with the result.
  void GetFileInfoOnTaskRunnerThread(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      GetFileInfoCallback callback);
  void ReadDirectoryOnTaskRunnerThread(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      ReadDirectoryCallback callback);
  void CreateSnapshotFileOnTaskRunnerThread(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      CreateSnapshotFileCallback callback);

 private:
  base::File::Error GetFileInfoSync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      base::File::Info* file_info,
      base::FilePath* platform_path);
  base::File::Error ReadDirectorySync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      EntryList* file_list);
  base::File::Error CreateSnapshotFileSync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      base::File::Info* file_info,
      base::FilePath* platform_path,
      scoped_refptr<storage::ShareableFileReference>* file_ref);

  // Translates the specified URL to a |local_file_path|, with no filtering.
  base::File::Error GetLocalFilePath(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& file_system_url,
      base::FilePath* local_file_path);

  // Like GetLocalFilePath(), but always take media_path_filter() into
  // consideration. If the media_path_filter() check fails, return
  // Fila::FILE_ERROR_SECURITY. |local_file_path| does not have to exist.
  base::File::Error GetFilteredLocalFilePath(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& file_system_url,
      base::FilePath* local_file_path);

  // Like GetLocalFilePath(), but if the file does not exist, then return
  // |failure_error|.
  // If |local_file_path| is a file, then take media_path_filter() into
  // consideration.
  // If the media_path_filter() check fails, return |failure_error|.
  // If |local_file_path| is a directory, return File::FILE_OK.
  base::File::Error GetFilteredLocalFilePathForExistingFileOrDirectory(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& file_system_url,
      base::File::Error failure_error,
      base::FilePath* local_file_path);

  MediaPathFilter media_path_filter_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
};

NativeMediaFileUtil::NativeMediaFileUtil(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner)
    : media_task_runner_(std::move(media_task_runner)),
      core_(std::make_unique<Core>(media_task_runner_)) {}

NativeMediaFileUtil::~NativeMediaFileUtil() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  media_task_runner_->DeleteSoon(FROM_HERE, std::move(core_));
}

// static
base::File::Error NativeMediaFileUtil::IsMediaFile(
    const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return file.error_details();

  char buffer[net::kMaxBytesToSniff];

  // Read as much as net::SniffMimeTypeFromLocalData() will bother looking at.
  int64_t len = file.Read(0, buffer, net::kMaxBytesToSniff);
  if (len < 0)
    return base::File::FILE_ERROR_FAILED;

  return IsMediaHeader(buffer, len);
}

// static
base::File::Error NativeMediaFileUtil::BufferIsMediaHeader(
    net::IOBuffer* buf, size_t length) {
  return IsMediaHeader(buf->data(), length);
}

// static
void NativeMediaFileUtil::CreatedSnapshotFileForCreateOrOpen(
    base::SequencedTaskRunner* media_task_runner,
    uint32_t file_flags,
    storage::AsyncFileUtil::CreateOrOpenCallback callback,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    scoped_refptr<storage::ShareableFileReference> file_ref) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(base::File(), base::OnceClosure());
    return;
  }
  media_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&storage::NativeFileUtil::CreateOrOpen, platform_path,
                     file_flags),
      base::BindOnce(&DidOpenSnapshot, std::move(callback),
                     std::move(file_ref)));
}

void NativeMediaFileUtil::CreateOrOpen(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    uint32_t file_flags,
    CreateOrOpenCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Returns an error if any unsupported flag is found.
  if (file_flags &
      ~(base::File::FLAG_OPEN | base::File::FLAG_READ |
        base::File::FLAG_WRITE_ATTRIBUTES | base::File::FLAG_WIN_NO_EXECUTE)) {
    std::move(callback).Run(base::File(base::File::FILE_ERROR_SECURITY),
                            base::OnceClosure());
    return;
  }
  scoped_refptr<base::SequencedTaskRunner> task_runner = context->task_runner();
  CreateSnapshotFile(
      std::move(context), url,
      base::BindOnce(&NativeMediaFileUtil::CreatedSnapshotFileForCreateOrOpen,
                     base::RetainedRef(task_runner), file_flags,
                     std::move(callback)));
}

void NativeMediaFileUtil::EnsureFileExists(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    EnsureFileExistsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(base::File::FILE_ERROR_SECURITY, false);
}

void NativeMediaFileUtil::CreateDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  storage::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&NativeMediaFileUtil::Core::CreateDirectory,
                     base::Unretained(core_.get()), std::move(context), url,
                     exclusive, recursive),
      std::move(callback));
  DCHECK(success);
}

void NativeMediaFileUtil::GetFileInfo(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    GetMetadataFieldSet fields,
    GetFileInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  storage::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeMediaFileUtil::Core::GetFileInfoOnTaskRunnerThread,
                     base::Unretained(core_.get()), std::move(context), url,
                     std::move(callback)));
  DCHECK(success);
}

void NativeMediaFileUtil::ReadDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    ReadDirectoryCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  storage::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NativeMediaFileUtil::Core::ReadDirectoryOnTaskRunnerThread,
          base::Unretained(core_.get()), std::move(context), url,
          std::move(callback)));
  DCHECK(success);
}

void NativeMediaFileUtil::Touch(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
}

void NativeMediaFileUtil::Truncate(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int64_t length,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
}

void NativeMediaFileUtil::CopyFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    CopyFileProgressCallback progress_callback,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  storage::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&NativeMediaFileUtil::Core::CopyOrMoveFileLocal,
                     base::Unretained(core_.get()), std::move(context), src_url,
                     dest_url, options, true /* copy */),
      std::move(callback));
  DCHECK(success);
}

void NativeMediaFileUtil::MoveFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  storage::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&NativeMediaFileUtil::Core::CopyOrMoveFileLocal,
                     base::Unretained(core_.get()), std::move(context), src_url,
                     dest_url, options, false /* copy */),
      std::move(callback));
  DCHECK(success);
}

void NativeMediaFileUtil::CopyInForeignFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  storage::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&NativeMediaFileUtil::Core::CopyInForeignFile,
                     base::Unretained(core_.get()), std::move(context),
                     src_file_path, dest_url),
      std::move(callback));
  DCHECK(success);
}

void NativeMediaFileUtil::DeleteFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  storage::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&NativeMediaFileUtil::Core::DeleteFile,
                     base::Unretained(core_.get()), std::move(context), url),
      std::move(callback));
  DCHECK(success);
}

// This is needed to support Copy and Move.
void NativeMediaFileUtil::DeleteDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  storage::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&NativeMediaFileUtil::Core::DeleteDirectory,
                     base::Unretained(core_.get()), std::move(context), url),
      std::move(callback));
  DCHECK(success);
}

void NativeMediaFileUtil::DeleteRecursively(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void NativeMediaFileUtil::CreateSnapshotFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    CreateSnapshotFileCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  storage::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NativeMediaFileUtil::Core::CreateSnapshotFileOnTaskRunnerThread,
          base::Unretained(core_.get()), std::move(context), url,
          std::move(callback)));
  DCHECK(success);
}

base::File::Error NativeMediaFileUtil::Core::CreateDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context.get(), url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  return storage::NativeFileUtil::CreateDirectory(
      file_path, exclusive, recursive);
}

base::File::Error NativeMediaFileUtil::Core::CopyOrMoveFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    bool copy) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::FilePath src_file_path;
  base::File::Error error = GetFilteredLocalFilePathForExistingFileOrDirectory(
      context.get(), src_url, base::File::FILE_ERROR_NOT_FOUND, &src_file_path);
  if (error != base::File::FILE_OK)
    return error;
  if (storage::NativeFileUtil::DirectoryExists(src_file_path))
    return base::File::FILE_ERROR_NOT_A_FILE;

  base::FilePath dest_file_path;
  error = GetLocalFilePath(context.get(), dest_url, &dest_file_path);
  if (error != base::File::FILE_OK)
    return error;
  base::File::Info file_info;
  error = storage::NativeFileUtil::GetFileInfo(dest_file_path, &file_info);
  if (error != base::File::FILE_OK &&
      error != base::File::FILE_ERROR_NOT_FOUND) {
    return error;
  }
  if (error == base::File::FILE_OK && file_info.is_directory)
    return base::File::FILE_ERROR_INVALID_OPERATION;
  if (!media_path_filter_.Match(dest_file_path))
    return base::File::FILE_ERROR_SECURITY;

  return storage::NativeFileUtil::CopyOrMoveFile(
      src_file_path, dest_file_path, options,
      storage::NativeFileUtil::CopyOrMoveModeForDestination(dest_url, copy));
}

base::File::Error NativeMediaFileUtil::Core::CopyInForeignFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(IsOnTaskRunnerThread(context.get()));
  if (src_file_path.empty())
    return base::File::FILE_ERROR_INVALID_OPERATION;

  base::FilePath dest_file_path;
  base::File::Error error =
      GetFilteredLocalFilePath(context.get(), dest_url, &dest_file_path);
  if (error != base::File::FILE_OK)
    return error;
  return storage::NativeFileUtil::CopyOrMoveFile(
      src_file_path, dest_file_path,
      storage::FileSystemOperation::CopyOrMoveOptionSet(),
      storage::NativeFileUtil::CopyOrMoveModeForDestination(dest_url,
                                                            true /* copy */));
}

base::File::Error NativeMediaFileUtil::Core::DeleteFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::File::Info file_info;
  base::FilePath file_path;
  base::File::Error error =
      GetFileInfoSync(context.get(), url, &file_info, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  if (file_info.is_directory)
    return base::File::FILE_ERROR_NOT_A_FILE;
  return storage::NativeFileUtil::DeleteFile(file_path);
}

base::File::Error NativeMediaFileUtil::Core::DeleteDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context.get(), url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  return storage::NativeFileUtil::DeleteDirectory(file_path);
}

void NativeMediaFileUtil::Core::GetFileInfoOnTaskRunnerThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    GetFileInfoCallback callback) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::File::Info file_info;
  base::File::Error error =
      GetFileInfoSync(context.get(), url, &file_info, nullptr);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), error, file_info));
}

void NativeMediaFileUtil::Core::ReadDirectoryOnTaskRunnerThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    ReadDirectoryCallback callback) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(IsOnTaskRunnerThread(context.get()));
  EntryList entry_list;
  base::File::Error error = ReadDirectorySync(context.get(), url, &entry_list);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), error, entry_list,
                                false /* has_more */));
}

void NativeMediaFileUtil::Core::CreateSnapshotFileOnTaskRunnerThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    CreateSnapshotFileCallback callback) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::File::Info file_info;
  base::FilePath platform_path;
  scoped_refptr<storage::ShareableFileReference> file_ref;
  base::File::Error error = CreateSnapshotFileSync(
      context.get(), url, &file_info, &platform_path, &file_ref);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), error, file_info,
                                platform_path, file_ref));
}

base::File::Error NativeMediaFileUtil::Core::GetFileInfoSync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url,
    base::File::Info* file_info,
    base::FilePath* platform_path) {
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  if (base::IsLink(file_path))
    return base::File::FILE_ERROR_NOT_FOUND;
  error = storage::NativeFileUtil::GetFileInfo(file_path, file_info);
  if (error != base::File::FILE_OK)
    return error;

  if (platform_path)
    *platform_path = file_path;
  if (file_info->is_directory || media_path_filter_.Match(file_path)) {
    return base::File::FILE_OK;
  }
  return base::File::FILE_ERROR_NOT_FOUND;
}

base::File::Error NativeMediaFileUtil::Core::ReadDirectorySync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url,
    EntryList* file_list) {
  base::File::Info file_info;
  base::FilePath dir_path;
  base::File::Error error =
      GetFileInfoSync(context, url, &file_info, &dir_path);

  if (error != base::File::FILE_OK)
    return error;

  if (!file_info.is_directory)
    return base::File::FILE_ERROR_NOT_A_DIRECTORY;

  base::FileEnumerator file_enum(
      dir_path,
      false /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath enum_path = file_enum.Next();
       !enum_path.empty();
       enum_path = file_enum.Next()) {
    // Skip symlinks.
    if (base::IsLink(enum_path))
      continue;

    base::FileEnumerator::FileInfo info = file_enum.GetInfo();

    // NativeMediaFileUtil skip criteria.
    if (MediaPathFilter::ShouldSkip(enum_path))
      continue;
    if (!info.IsDirectory() && !media_path_filter_.Match(enum_path))
      continue;

    file_list->emplace_back(enum_path.BaseName(), info.GetName(),
                            info.IsDirectory()
                                ? filesystem::mojom::FsFileType::DIRECTORY
                                : filesystem::mojom::FsFileType::REGULAR_FILE);
  }

  return base::File::FILE_OK;
}

base::File::Error NativeMediaFileUtil::Core::CreateSnapshotFileSync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url,
    base::File::Info* file_info,
    base::FilePath* platform_path,
    scoped_refptr<storage::ShareableFileReference>* file_ref) {
  base::File::Error error =
      GetFileInfoSync(context, url, file_info, platform_path);
  if (error == base::File::FILE_OK && file_info->is_directory)
    error = base::File::FILE_ERROR_NOT_A_FILE;
  if (error == base::File::FILE_OK)
    error = NativeMediaFileUtil::IsMediaFile(*platform_path);

  // We're just returning the local file information.
  *file_ref = scoped_refptr<storage::ShareableFileReference>();

  return error;
}

base::File::Error NativeMediaFileUtil::Core::GetLocalFilePath(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url,
    base::FilePath* local_file_path) {
  DCHECK(url.is_valid());
  if (url.path().empty()) {
    // Root direcory case, which should not be accessed.
    return base::File::FILE_ERROR_ACCESS_DENIED;
  }
  *local_file_path = url.path();
  return base::File::FILE_OK;
}

base::File::Error NativeMediaFileUtil::Core::GetFilteredLocalFilePath(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& file_system_url,
    base::FilePath* local_file_path) {
  base::FilePath file_path;
  base::File::Error error =
      GetLocalFilePath(context, file_system_url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  if (!media_path_filter_.Match(file_path))
    return base::File::FILE_ERROR_SECURITY;

  *local_file_path = file_path;
  return base::File::FILE_OK;
}

base::File::Error
NativeMediaFileUtil::Core::GetFilteredLocalFilePathForExistingFileOrDirectory(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& file_system_url,
    base::File::Error failure_error,
    base::FilePath* local_file_path) {
  base::FilePath file_path;
  base::File::Error error =
      GetLocalFilePath(context, file_system_url, &file_path);
  if (error != base::File::FILE_OK)
    return error;

  if (!base::PathExists(file_path))
    return failure_error;
  base::File::Info file_info;
  if (!base::GetFileInfo(file_path, &file_info))
    return base::File::FILE_ERROR_FAILED;

  if (!file_info.is_directory && !media_path_filter_.Match(file_path)) {
    return failure_error;
  }

  *local_file_path = file_path;
  return base::File::FILE_OK;
}
