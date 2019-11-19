// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {
namespace internal {
namespace {

// Executes GetFileInfo on the UI thread.
void GetFileInfoOnUIThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int fields,
    ProvidedFileSystemInterface::GetMetadataCallback callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    std::move(callback).Run(base::WrapUnique<EntryMetadata>(NULL),
                            base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  int fsp_fields = 0;
  if (fields & storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY)
    fsp_fields |= ProvidedFileSystemInterface::METADATA_FIELD_IS_DIRECTORY;
  if (fields & storage::FileSystemOperation::GET_METADATA_FIELD_SIZE)
    fsp_fields |= ProvidedFileSystemInterface::METADATA_FIELD_SIZE;
  if (fields & storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED)
    fsp_fields |= ProvidedFileSystemInterface::METADATA_FIELD_MODIFICATION_TIME;

  parser.file_system()->GetMetadata(parser.file_path(), fsp_fields,
                                    std::move(callback));
}

// Routes the response of GetFileInfo back to the IO thread with a type
// conversion.
void OnGetFileInfo(int fields,
                   storage::AsyncFileUtil::GetFileInfoCallback callback,
                   std::unique_ptr<EntryMetadata> metadata,
                   base::File::Error result) {
  if (result != base::File::FILE_OK) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(std::move(callback), result, base::File::Info()));
    return;
  }

  DCHECK(metadata.get());
  base::File::Info file_info;

  if (fields & storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY)
    file_info.is_directory = *metadata->is_directory;
  if (fields & storage::FileSystemOperation::GET_METADATA_FIELD_SIZE)
    file_info.size = *metadata->size;

  if (fields & storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED) {
    file_info.last_modified = *metadata->modification_time;
    // TODO(mtomasz): Add support for last modified time and creation time.
    // See: crbug.com/388540.
    file_info.last_accessed = *metadata->modification_time;  // Not supported.
    file_info.creation_time = *metadata->modification_time;  // Not supported.
  }

  file_info.is_symbolic_link = false;  // Not supported.

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(std::move(callback), base::File::FILE_OK, file_info));
}

// Executes ReadDirectory on the UI thread.
void ReadDirectoryOnUIThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    storage::AsyncFileUtil::ReadDirectoryCallback callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION,
                 storage::AsyncFileUtil::EntryList(),
                 false /* has_more */);
    return;
  }

  parser.file_system()->ReadDirectory(parser.file_path(), callback);
}

// Routes the response of ReadDirectory back to the IO thread.
void OnReadDirectory(storage::AsyncFileUtil::ReadDirectoryCallback callback,
                     base::File::Error result,
                     storage::AsyncFileUtil::EntryList entry_list,
                     bool has_more) {
  auto new_end_it =
      std::remove_if(entry_list.begin(), entry_list.end(),
                     [](const filesystem::mojom::DirectoryEntry& entry) {
                       if (!filesystem::mojom::IsKnownEnumValue(entry.type)) {
                         return true;
                       }
                       if (entry.name.empty() || entry.name.value() == "." ||
                           entry.name.value() == ".." ||
                           base::Contains(entry.name.value(), '\0') ||
                           base::Contains(entry.name.value(), '/') ||
                           base::Contains(entry.name.value(), '\\')) {
                         return true;
                       }
                       return false;
                     });
  entry_list.erase(new_end_it, entry_list.end());

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(callback, result, std::move(entry_list), has_more));
}

// Executes CreateDirectory on the UI thread.
void CreateDirectoryOnUIThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->CreateDirectory(parser.file_path(), recursive,
                                        std::move(callback));
}

// Routes the response of CreateDirectory back to the IO thread.
void OnCreateDirectory(bool exclusive,
                       storage::AsyncFileUtil::StatusCallback callback,
                       base::File::Error result) {
  // If the directory already existed and the operation wasn't exclusive, then
  // return success anyway, since it is not an error.
  const base::File::Error error =
      (result == base::File::FILE_ERROR_EXISTS && !exclusive)
          ? base::File::FILE_OK
          : result;

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), error));
}

// Executes DeleteEntry on the UI thread.
void DeleteEntryOnUIThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->DeleteEntry(parser.file_path(), recursive,
                                    std::move(callback));
}

// Routes the response of DeleteEntry back to the IO thread.
void OnDeleteEntry(storage::AsyncFileUtil::StatusCallback callback,
                   base::File::Error result) {
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), result));
}

// Executes CreateFile on the UI thread.
void CreateFileOnUIThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    storage::AsyncFileUtil::StatusCallback callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->CreateFile(parser.file_path(), std::move(callback));
}

// Routes the response of CreateFile to a callback of EnsureFileExists() on the
// IO thread.
void OnCreateFileForEnsureFileExists(
    storage::AsyncFileUtil::EnsureFileExistsCallback callback,
    base::File::Error result) {
  const bool created = result == base::File::FILE_OK;

  // If the file already existed, then return success anyway, since it is not
  // an error.
  const base::File::Error error =
      result == base::File::FILE_ERROR_EXISTS ? base::File::FILE_OK : result;

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), error, created));
}

// Executes CopyEntry on the UI thread.
void CopyEntryOnUIThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& target_url,
    storage::AsyncFileUtil::StatusCallback callback) {
  util::FileSystemURLParser source_parser(source_url);
  util::FileSystemURLParser target_parser(target_url);

  if (!source_parser.Parse() || !target_parser.Parse() ||
      source_parser.file_system() != target_parser.file_system()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  target_parser.file_system()->CopyEntry(source_parser.file_path(),
                                         target_parser.file_path(),
                                         std::move(callback));
}

// Routes the response of CopyEntry to a callback of CopyLocalFile() on the
// IO thread.
void OnCopyEntry(storage::AsyncFileUtil::StatusCallback callback,
                 base::File::Error result) {
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), result));
}

// Executes MoveEntry on the UI thread.
void MoveEntryOnUIThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& target_url,
    storage::AsyncFileUtil::StatusCallback callback) {
  util::FileSystemURLParser source_parser(source_url);
  util::FileSystemURLParser target_parser(target_url);

  if (!source_parser.Parse() || !target_parser.Parse() ||
      source_parser.file_system() != target_parser.file_system()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  target_parser.file_system()->MoveEntry(source_parser.file_path(),
                                         target_parser.file_path(),
                                         std::move(callback));
}

// Routes the response of CopyEntry to a callback of MoveLocalFile() on the
// IO thread.
void OnMoveEntry(storage::AsyncFileUtil::StatusCallback callback,
                 base::File::Error result) {
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), result));
}

// Executes Truncate on the UI thread.
void TruncateOnUIThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int64_t length,
    storage::AsyncFileUtil::StatusCallback callback) {
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->Truncate(parser.file_path(), length,
                                 std::move(callback));
}

// Routes the response of Truncate back to the IO thread.
void OnTruncate(storage::AsyncFileUtil::StatusCallback callback,
                base::File::Error result) {
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), result));
}

}  // namespace

ProviderAsyncFileUtil::ProviderAsyncFileUtil() {}

ProviderAsyncFileUtil::~ProviderAsyncFileUtil() {}

void ProviderAsyncFileUtil::CreateOrOpen(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int file_flags,
    CreateOrOpenCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if ((file_flags & base::File::FLAG_CREATE) ||
      (file_flags & base::File::FLAG_OPEN_ALWAYS) ||
      (file_flags & base::File::FLAG_CREATE_ALWAYS) ||
      (file_flags & base::File::FLAG_OPEN_TRUNCATED)) {
    std::move(callback).Run(base::File(base::File::FILE_ERROR_ACCESS_DENIED),
                            base::Closure());
    return;
  }

  NOTIMPLEMENTED();
  std::move(callback).Run(base::File(base::File::FILE_ERROR_INVALID_OPERATION),
                          base::Closure());
}

void ProviderAsyncFileUtil::EnsureFileExists(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    EnsureFileExistsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CreateFileOnUIThread, base::Passed(&context), url,
                     base::BindOnce(&OnCreateFileForEnsureFileExists,
                                    std::move(callback))));
}

void ProviderAsyncFileUtil::CreateDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &CreateDirectoryOnUIThread, base::Passed(&context), url, exclusive,
          recursive,
          base::BindOnce(&OnCreateDirectory, exclusive, std::move(callback))));
}

void ProviderAsyncFileUtil::GetFileInfo(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int fields,
    GetFileInfoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &GetFileInfoOnUIThread, base::Passed(&context), url, fields,
          base::Bind(&OnGetFileInfo, fields, base::Passed(&callback))));
}

void ProviderAsyncFileUtil::ReadDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    ReadDirectoryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ReadDirectoryOnUIThread, std::move(context), url,
                     base::BindRepeating(&OnReadDirectory, callback)));
}

void ProviderAsyncFileUtil::Touch(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::Truncate(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int64_t length,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&TruncateOnUIThread, base::Passed(&context), url, length,
                     base::BindOnce(&OnTruncate, std::move(callback))));
}

void ProviderAsyncFileUtil::CopyFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    CopyFileProgressCallback progress_callback,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mtomasz): Consier adding support for options (preserving last modified
  // time) as well as the progress callback.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CopyEntryOnUIThread, base::Passed(&context), src_url,
                     dest_url,
                     base::BindOnce(&OnCopyEntry, std::move(callback))));
}

void ProviderAsyncFileUtil::MoveFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mtomasz): Consier adding support for options (preserving last modified
  // time) as well as the progress callback.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&MoveEntryOnUIThread, base::Passed(&context), src_url,
                     dest_url,
                     base::BindOnce(&OnMoveEntry, std::move(callback))));
}

void ProviderAsyncFileUtil::CopyInForeignFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ProviderAsyncFileUtil::DeleteFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DeleteEntryOnUIThread, base::Passed(&context), url,
                     false,  // recursive
                     base::BindOnce(&OnDeleteEntry, std::move(callback))));
}

void ProviderAsyncFileUtil::DeleteDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DeleteEntryOnUIThread, base::Passed(&context), url,
                     false,  // recursive
                     base::BindOnce(&OnDeleteEntry, std::move(callback))));
}

void ProviderAsyncFileUtil::DeleteRecursively(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DeleteEntryOnUIThread, base::Passed(&context), url,
                     true,  // recursive
                     base::BindOnce(&OnDeleteEntry, std::move(callback))));
}

void ProviderAsyncFileUtil::CreateSnapshotFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    CreateSnapshotFileCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTIMPLEMENTED();
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION,
                          base::File::Info(), base::FilePath(),
                          scoped_refptr<storage::ShareableFileReference>());
}

}  // namespace internal
}  // namespace file_system_provider
}  // namespace chromeos
