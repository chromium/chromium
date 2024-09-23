// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_async_file_util.h"

#include <utility>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_size_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_file_system_url_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"

using content::BrowserThread;

namespace arc {

namespace {

void OnGetFileInfoOnUIThread(
    ArcDocumentsProviderRoot::GetFileInfoCallback callback,
    base::File::Error result,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result, info));
}

void OnReadDirectoryOnUIThread(
    storage::AsyncFileUtil::ReadDirectoryCallback callback,
    base::File::Error result,
    std::vector<ArcDocumentsProviderRoot::ThinFileInfo> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::AsyncFileUtil::EntryList entries;
  entries.reserve(files.size());
  for (const auto& file : files) {
    entries.emplace_back(base::FilePath(file.name), base::FilePath(),
                         file.is_directory
                             ? filesystem::mojom::FsFileType::DIRECTORY
                             : filesystem::mojom::FsFileType::REGULAR_FILE);
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result, entries,
                                false /* has_more */));
}

void OnCreateFileOnUIThread(
    ArcDocumentsProviderAsyncFileUtil::EnsureFileExistsCallback callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::File::Error result_to_report = result;
  bool created = false;
  if (result == base::File::FILE_OK) {
    created = true;
  } else if (result == base::File::FILE_ERROR_EXISTS) {
    result_to_report = base::File::FILE_OK;
  }
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), result_to_report, created));
}

void OnStatusCallbackOnUIThread(storage::AsyncFileUtil::StatusCallback callback,
                                base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void GetFileInfoOnUIThread(
    const storage::FileSystemURL& url,
    storage::FileSystemOperation::GetMetadataFieldSet fields,
    ArcDocumentsProviderRoot::GetFileInfoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnGetFileInfoOnUIThread(std::move(callback),
                            base::File::FILE_ERROR_SECURITY,
                            base::File::Info());
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnGetFileInfoOnUIThread(std::move(callback),
                            base::File::FILE_ERROR_NOT_FOUND,
                            base::File::Info());
    return;
  }

  root->GetFileInfo(
      path, fields,
      base::BindOnce(&OnGetFileInfoOnUIThread, std::move(callback)));
}

void ReadDirectoryOnUIThread(
    const storage::FileSystemURL& url,
    storage::AsyncFileUtil::ReadDirectoryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnReadDirectoryOnUIThread(std::move(callback),
                              base::File::FILE_ERROR_SECURITY, {});
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnReadDirectoryOnUIThread(std::move(callback),
                              base::File::FILE_ERROR_NOT_FOUND, {});
    return;
  }

  root->ReadDirectory(
      path, base::BindOnce(&OnReadDirectoryOnUIThread, std::move(callback)));
}

void DeleteFileOnUIThread(const storage::FileSystemURL& url,
                          storage::AsyncFileUtil::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnStatusCallbackOnUIThread(std::move(callback),
                               base::File::FILE_ERROR_SECURITY);
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnStatusCallbackOnUIThread(std::move(callback),
                               base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  root->DeleteFile(
      path, base::BindOnce(&OnStatusCallbackOnUIThread, std::move(callback)));
}

void CreateFileOnUIThread(
    const storage::FileSystemURL& url,
    ArcDocumentsProviderAsyncFileUtil::EnsureFileExistsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnCreateFileOnUIThread(std::move(callback),
                           base::File::FILE_ERROR_SECURITY);
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnCreateFileOnUIThread(std::move(callback),
                           base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  root->CreateFile(
      path, base::BindOnce(&OnCreateFileOnUIThread, std::move(callback)));
}

void CreateDirectoryOnUIThread(
    const storage::FileSystemURL& url,
    storage::AsyncFileUtil::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnStatusCallbackOnUIThread(std::move(callback),
                               base::File::FILE_ERROR_SECURITY);
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnStatusCallbackOnUIThread(std::move(callback),
                               base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  root->CreateDirectory(
      path, base::BindOnce(&OnStatusCallbackOnUIThread, std::move(callback)));
}

void CopyFileLocalOnUIThread(const storage::FileSystemURL& src_url,
                             const storage::FileSystemURL& dest_url,
                             storage::AsyncFileUtil::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnStatusCallbackOnUIThread(std::move(callback),
                               base::File::FILE_ERROR_SECURITY);
    return;
  }

  base::FilePath src_path;
  base::FilePath dest_path;
  ArcDocumentsProviderRoot* src_root =
      roots->ParseAndLookup(src_url, &src_path);
  ArcDocumentsProviderRoot* dest_root =
      roots->ParseAndLookup(dest_url, &dest_path);
  if (!src_root || !dest_root) {
    OnStatusCallbackOnUIThread(std::move(callback),
                               base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (src_root != dest_root) {
    // TODO(fukino): We should fall back to a stream copy. crbug.com/945695.
    OnStatusCallbackOnUIThread(std::move(callback),
                               base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  src_root->CopyFileLocal(
      src_path, dest_path,
      base::BindOnce(&OnStatusCallbackOnUIThread, std::move(callback)));
}

void MoveFileLocalOnUIThread(const storage::FileSystemURL& src_url,
                             const storage::FileSystemURL& dest_url,
                             storage::AsyncFileUtil::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnStatusCallbackOnUIThread(std::move(callback),
                               base::File::FILE_ERROR_SECURITY);
    return;
  }

  base::FilePath src_path;
  base::FilePath dest_path;
  ArcDocumentsProviderRoot* src_root =
      roots->ParseAndLookup(src_url, &src_path);
  ArcDocumentsProviderRoot* dest_root =
      roots->ParseAndLookup(dest_url, &dest_path);
  if (!src_root || !dest_root) {
    OnStatusCallbackOnUIThread(std::move(callback),
                               base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (src_root != dest_root) {
    // TODO(fukino): We should fall back to a stream move. crbug.com/945695.
    OnStatusCallbackOnUIThread(std::move(callback),
                               base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  src_root->MoveFileLocal(
      src_path, dest_path,
      base::BindOnce(&OnStatusCallbackOnUIThread, std::move(callback)));
}

}  // namespace

ArcDocumentsProviderAsyncFileUtil::ArcDocumentsProviderAsyncFileUtil() =
    default;

ArcDocumentsProviderAsyncFileUtil::~ArcDocumentsProviderAsyncFileUtil() =
    default;

void ArcDocumentsProviderAsyncFileUtil::CreateOrOpen(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    uint32_t file_flags,
    CreateOrOpenCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(nya): Implement this function if it is ever called.
  NOTIMPLEMENTED();
  std::move(callback).Run(base::File(base::File::FILE_ERROR_INVALID_OPERATION),
                          base::OnceClosure());
}

void ArcDocumentsProviderAsyncFileUtil::EnsureFileExists(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    EnsureFileExistsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, url.type());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateFileOnUIThread, url, std::move(callback)));
}

void ArcDocumentsProviderAsyncFileUtil::CreateDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, url.type());
  DCHECK(!recursive);  // Files app doesn't create directory with |recursive|.

  // Even when |exclusive| is false, we report File::FILE_ERROR_EXISTS when a
  // directory already exists at |url| for simpler ArcDocumentsProviderRoot
  // implementation. Chances of this case are small, since Files app
  // de-duplicate the new directory name to avoid conflicting with existing one.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateDirectoryOnUIThread, url, std::move(callback)));
}

void ArcDocumentsProviderAsyncFileUtil::GetFileInfo(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    GetMetadataFieldSet fields,
    GetFileInfoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, url.type());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetFileInfoOnUIThread, url, fields, std::move(callback)));
}

void ArcDocumentsProviderAsyncFileUtil::ReadDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    ReadDirectoryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, url.type());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadDirectoryOnUIThread, url, std::move(callback)));
}

void ArcDocumentsProviderAsyncFileUtil::Touch(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Touch operation is not supported by documents providers.
  // The failure on touch operation will just be ignored and preceding operation
  // like copy, move, will succeed.
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void ArcDocumentsProviderAsyncFileUtil::Truncate(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    int64_t length,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Truncate() doesn't work well on ARC P/R container. It works on ARCVM R+
  // because the mojo proxy for ARCVM implements the feature.
  // TODO(b/223247850) Fix this.
  if (!IsArcVmEnabled()) {
    // HACK: Return FILE_OK even though we do nothing here.
    // This is a really dirty hack which can result in corrupting file contents.
    NOTIMPLEMENTED();
    std::move(callback).Run(base::File::FILE_OK);
    return;
  }
  // Call TruncateOnIOThread() after ResolveToContentUrlOnIOThread().
  ResolveToContentUrlOnIOThread(
      url, base::BindOnce(
               [](int64_t length, StatusCallback callback, const GURL& url) {
                 TruncateOnIOThread(url, length, std::move(callback));
               },
               length, std::move(callback)));
}

void ArcDocumentsProviderAsyncFileUtil::CopyFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    CopyFileProgressCallback progress_callback,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, src_url.type());
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, dest_url.type());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CopyFileLocalOnUIThread, src_url, dest_url,
                                std::move(callback)));
}

void ArcDocumentsProviderAsyncFileUtil::MoveFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, src_url.type());
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, dest_url.type());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MoveFileLocalOnUIThread, src_url, dest_url,
                                std::move(callback)));
}

void ArcDocumentsProviderAsyncFileUtil::CopyInForeignFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTREACHED_IN_MIGRATION();  // Read-only file system.
  std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED);
}

void ArcDocumentsProviderAsyncFileUtil::DeleteFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, url.type());

  // TODO(fukino): Report an error if the document at |url| is not a file.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteFileOnUIThread, url, std::move(callback)));
}

void ArcDocumentsProviderAsyncFileUtil::DeleteDirectory(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, url.type());

  // TODO(fukino): Report an error if the document at |url| is not a directory.
  // TODO(fukino): Report an error if the document at |url| is a directory which
  // is not empty.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteFileOnUIThread, url, std::move(callback)));
}

void ArcDocumentsProviderAsyncFileUtil::DeleteRecursively(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeArcDocumentsProvider, url.type());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteFileOnUIThread, url, std::move(callback)));
}

void ArcDocumentsProviderAsyncFileUtil::CreateSnapshotFile(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    CreateSnapshotFileCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  NOTIMPLEMENTED();  // TODO(crbug.com/40496703): Implement this function.
  std::move(callback).Run(base::File::FILE_ERROR_FAILED, base::File::Info(),
                          base::FilePath(),
                          scoped_refptr<storage::ShareableFileReference>());
}

}  // namespace arc
