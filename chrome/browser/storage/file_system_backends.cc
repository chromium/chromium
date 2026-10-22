// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/file_system_backends.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"
#include "storage/browser/file_system/file_system_backend.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/check.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_backend_delegate.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_backend_delegate.h"
#include "chrome/browser/ash/drive/fileapi/drivefs_file_system_backend_delegate.h"
#include "chrome/browser/ash/file_system_provider/fileapi/backend_delegate.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/fileapi/mtp_file_system_backend_delegate.h"
#include "chrome/browser/ash/smb_client/fileapi/smbfs_file_system_backend_delegate.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/common/file_system/file_system_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

void GetAdditionalFileSystemBackends(
    content::BrowserContext* browser_context,
    const base::FilePath& storage_partition_path,
    std::vector<std::unique_ptr<storage::FileSystemBackend>>*
        additional_backends) {
  // A BrowserContext without a path keeps all its storage in memory, and has
  // nowhere for these backends to put their files.
  if (browser_context->GetPath().empty()) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  storage::ExternalMountPoints* external_mount_points =
      browser_context->GetMountPoints();
  CHECK(external_mount_points);
  auto backend = std::make_unique<ash::FileSystemBackend>(
      Profile::FromBrowserContext(browser_context),
      ash::file_system_provider::BackendDelegate::MakeUnique(),
      std::make_unique<ash::MTPFileSystemBackendDelegate>(
          storage_partition_path),
      std::make_unique<arc::ArcContentFileSystemBackendDelegate>(),
      std::make_unique<arc::ArcDocumentsProviderBackendDelegate>(),
      std::make_unique<drive::DriveFsFileSystemBackendDelegate>(
          Profile::FromBrowserContext(browser_context)),
      std::make_unique<ash::smb_client::SmbFsFileSystemBackendDelegate>(
          Profile::FromBrowserContext(browser_context)),
      external_mount_points, storage::ExternalMountPoints::GetSystemInstance());
  backend->AddSystemMountPoints();
  CHECK(backend->CanHandleType(storage::kFileSystemTypeExternal));
  additional_backends->push_back(std::move(backend));

  additional_backends->push_back(
      std::make_unique<MediaFileSystemBackend>(storage_partition_path));
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  additional_backends->push_back(
      std::make_unique<sync_file_system::SyncFileSystemBackend>(
          Profile::FromBrowserContext(browser_context)));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}
