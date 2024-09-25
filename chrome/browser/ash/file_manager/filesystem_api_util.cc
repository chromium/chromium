// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/filesystem_api_util.h"

#include <memory>
#include <utility>

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/drive/file_errors.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/common/task_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/file_system/file_system_context.h"
#include "url/gurl.h"

namespace file_manager {
namespace util {
namespace {

void GetMimeTypeAfterGetMetadata(
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK || !metadata ||
      metadata->content_mime_type.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(std::move(metadata->content_mime_type));
}

// Helper function used to implement GetNonNativeLocalPathMimeType. It extracts
// the mime type from the passed metadata from a providing extension.
void GetMimeTypeAfterGetMetadataForProvidedFileSystem(
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    std::unique_ptr<ash::file_system_provider::EntryMetadata> metadata,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != base::File::FILE_OK || !metadata->mime_type.get()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(*metadata->mime_type);
}

// Helper function used to implement GetNonNativeLocalPathMimeType. It passes
// the returned mime type to the callback.
void GetMimeTypeAfterGetMimeTypeForArcContentFileSystem(
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    const std::optional<std::string>& mime_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (mime_type.has_value()) {
    std::move(callback).Run(mime_type.value());
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void OnResolveToContentUrl(
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    Profile* profile,
    const base::FilePath& path,
    const GURL& content_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  if (content_url.is_valid()) {
    auto* runner =
        arc::ArcFileSystemOperationRunner::GetForBrowserContext(profile);
    if (!runner) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    runner->GetMimeType(
        content_url,
        base::BindOnce(&GetMimeTypeAfterGetMimeTypeForArcContentFileSystem,
                       std::move(callback)));
    return;
  }

  // If |content url| could not be parsed from documents provider special
  // path (i.e. absolute path is obfuscated), then lookup by extension in the
  // |kAndroidMimeTypeMappings| as a backup method.
  if (path.empty()) {
    LOG(ERROR) << "File path is empty";
    std::move(callback).Run(std::nullopt);
    return;
  }
  base::FilePath::StringType extension =
      base::ToLowerASCII(path.FinalExtension());
  if (extension.empty()) {
    LOG(ERROR) << "File name is missing extension for path: " << path;
    std::move(callback).Run(std::nullopt);
    return;
  }
  extension = extension.substr(1);  // Strip the leading dot.
  const std::string mime_type = arc::FindArcMimeTypeFromExtension(extension);
  if (mime_type.empty()) {
    LOG(ERROR) << "Could not find ARC mime type from extension: " << extension;
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(mime_type);
}

// Helper function to converts a callback that takes boolean value to that takes
// File::Error, by regarding FILE_OK as the only successful value.
void BoolCallbackAsFileErrorCallback(base::OnceCallback<void(bool)> callback,
                                     base::File::Error error) {
  return std::move(callback).Run(error == base::File::FILE_OK);
}

// Part of PrepareFileOnIOThread. It tries to create a new file if the given
// |url| is not already inhabited.
void PrepareFileAfterCheckExistOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    storage::FileSystemOperation::StatusCallback callback,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (error != base::File::FILE_ERROR_NOT_FOUND) {
    std::move(callback).Run(error);
    return;
  }

  // Call with the second argument |exclusive| set to false, meaning that it
  // is not an error even if the file already exists (it can happen if the file
  // is created after the previous FileExists call and before this CreateFile.)
  //
  // Note that the preceding call to FileExists is necessary for handling
  // read only filesystems that blindly rejects handling CreateFile().
  file_system_context->operation_runner()->CreateFile(url, false,
                                                      std::move(callback));
}

// Checks whether a file exists at the given |url|, and try creating it if it
// is not already there.
void PrepareFileOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto* const operation_runner = file_system_context->operation_runner();
  operation_runner->FileExists(
      url, base::BindOnce(&PrepareFileAfterCheckExistOnIOThread,
                          std::move(file_system_context), url,
                          base::BindOnce(&BoolCallbackAsFileErrorCallback,
                                         std::move(callback))));
}

}  // namespace

bool IsUnderNonNativeLocalPath(Profile* profile, const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GURL url;
  if (!util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, path, util::GetFileManagerURL(), &url)) {
    return false;
  }

  storage::FileSystemURL filesystem_url =
      GetFileSystemContextForSourceURL(profile, GetFileManagerURL())
          ->CrackURLInFirstPartyContext(url);
  if (!filesystem_url.is_valid()) {
    return false;
  }

  return !filesystem_url.TypeImpliesPathIsReal();
}

bool IsDriveLocalPath(Profile* profile, const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GURL url;
  if (!util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, path, util::GetFileManagerURL(), &url)) {
    return false;
  }

  storage::FileSystemURL filesystem_url =
      GetFileSystemContextForSourceURL(profile, GetFileManagerURL())
          ->CrackURLInFirstPartyContext(url);
  if (!filesystem_url.is_valid()) {
    return false;
  }

  return filesystem_url.type() == storage::kFileSystemTypeDriveFs;
}

bool HasNonNativeMimeTypeProvider(Profile* profile,
                                  const base::FilePath& path) {
  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  return (drive_integration_service &&
          drive_integration_service->GetMountPointPath().IsParent(path)) ||
         IsUnderNonNativeLocalPath(profile, path);
}

void GetNonNativeLocalPathMimeType(
    Profile* profile,
    const base::FilePath& path,
    base::OnceCallback<void(const std::optional<std::string>&)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(HasNonNativeMimeTypeProvider(profile, path));

  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  base::FilePath drive_relative_path;
  if (drive_integration_service &&
      drive_integration_service->GetRelativeDrivePath(path,
                                                      &drive_relative_path)) {
    if (auto* drivefs = drive_integration_service->GetDriveFsInterface()) {
      drivefs->GetMetadata(
          drive_relative_path,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              base::BindOnce(&GetMimeTypeAfterGetMetadata, std::move(callback)),
              drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr));

      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  if (ash::file_system_provider::util::IsFileSystemProviderLocalPath(path)) {
    ash::file_system_provider::util::LocalPathParser parser(profile, path);
    if (!parser.Parse()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    parser.file_system()->GetMetadata(
        parser.file_path(),
        ash::file_system_provider::ProvidedFileSystemInterface::
            METADATA_FIELD_MIME_TYPE,
        base::BindOnce(&GetMimeTypeAfterGetMetadataForProvidedFileSystem,
                       std::move(callback)));
    return;
  }

  if (arc::IsArcAllowedForProfile(profile)) {
    if (base::FilePath(arc::kContentFileSystemMountPointPath).IsParent(path)) {
      const GURL arc_url = arc::PathToArcUrl(path);
      if (!arc_url.is_valid()) {
        LOG(ERROR) << "ARC URL is invalid for path: " << path;
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
        return;
      }

      auto* runner =
          arc::ArcFileSystemOperationRunner::GetForBrowserContext(profile);
      if (!runner) {
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
        return;
      }
      runner->GetMimeType(
          arc_url,
          base::BindOnce(&GetMimeTypeAfterGetMimeTypeForArcContentFileSystem,
                         std::move(callback)));
      return;
    } else if (base::FilePath(arc::kDocumentsProviderMountPointPath)
                   .IsParent(path)) {
      auto* root_map =
          arc::ArcDocumentsProviderRootMap::GetForArcBrowserContext();
      if (!root_map) {
        LOG(ERROR) << "Could not find root map from ARC browser context";
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
        return;
      }

      std::string authority;
      std::string root_id;
      if (!arc::ParseDocumentsProviderPath(path, &authority, &root_id)) {
        LOG(ERROR) << "Failed to parse documents provider path: " << path;
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
        return;
      }
      auto* root = root_map->Lookup(authority, root_id);
      if (!root) {
        LOG(ERROR) << "No root found for authority: " << authority
                   << " document_id: " << root_id;
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
        return;
      }
      root->ResolveToContentUrl(
          path, base::BindOnce(&OnResolveToContentUrl, std::move(callback),
                               profile, path));
      return;
    }
  }

  // We don't have a way to obtain metadata other than drive and FSP. Returns an
  // error with empty MIME type, that leads fallback guessing mime type from
  // file extensions.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

void IsNonNativeLocalPathDirectory(Profile* profile,
                                   const base::FilePath& path,
                                   base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsUnderNonNativeLocalPath(profile, path));

  util::CheckIfDirectoryExists(
      GetFileManagerFileSystemContext(profile), path,
      base::BindOnce(&BoolCallbackAsFileErrorCallback, std::move(callback)));
}

void PrepareNonNativeLocalFileForWritableApp(
    Profile* profile,
    const base::FilePath& path,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsUnderNonNativeLocalPath(profile, path));

  GURL url;
  if (!util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, path, util::GetFileManagerURL(), &url)) {
    // Posting to the current thread, so that we always call back asynchronously
    // independent from whether or not the operation succeeds.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  scoped_refptr<storage::FileSystemContext> const file_system_context =
      GetFileManagerFileSystemContext(profile);
  DCHECK(file_system_context);
  auto* const backend = ash::FileSystemBackend::Get(*file_system_context);
  DCHECK(backend);
  const storage::FileSystemURL internal_url =
      backend->CreateInternalURL(file_system_context.get(), path);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PrepareFileOnIOThread, file_system_context, internal_url,
                     google_apis::CreateRelayCallback(std::move(callback))));
}

}  // namespace util
}  // namespace file_manager
