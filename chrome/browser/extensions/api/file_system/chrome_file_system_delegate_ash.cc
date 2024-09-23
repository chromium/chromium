// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate_ash.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/api/file_system/consent_provider.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace extensions {

namespace file_system = api::file_system;

namespace {

// Fills a list of volumes mounted in the system.
void FillVolumeList(content::BrowserContext* browser_context,
                    std::vector<file_system::Volume>* result) {
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(browser_context);
  DCHECK(volume_manager);

  const auto& volume_list = volume_manager->GetVolumeList();
  // Convert volume_list to result_volume_list.
  for (const auto& volume : volume_list) {
    file_system::Volume result_volume;
    result_volume.volume_id = volume->volume_id();
    result_volume.writable = !volume->is_read_only();
    result->push_back(std::move(result_volume));
  }
}

// Callback called when consent is granted or denied.
void OnConsentReceived(content::BrowserContext* browser_context,
                       scoped_refptr<ExtensionFunction> requester,
                       FileSystemDelegate::FileSystemCallback success_callback,
                       FileSystemDelegate::ErrorCallback error_callback,
                       const url::Origin& origin,
                       const base::WeakPtr<file_manager::Volume>& volume,
                       bool writable,
                       ConsentProvider::Consent result) {
  using file_manager::Volume;
  using file_manager::VolumeManager;

  // Render frame host can be gone before this callback method is executed.
  if (!requester->render_frame_host()) {
    std::move(error_callback).Run(std::string());
    return;
  }

  const char* consent_err_msg = file_system_api::ConsentResultToError(result);
  if (consent_err_msg) {
    std::move(error_callback).Run(consent_err_msg);
    return;
  }

  if (!volume.get()) {
    std::move(error_callback).Run(file_system_api::kVolumeNotFoundError);
    return;
  }

  DCHECK_EQ(origin.scheme(), kExtensionScheme);
  scoped_refptr<storage::FileSystemContext> file_system_context =
      util::GetStoragePartitionForExtensionId(origin.host(), browser_context)
          ->GetFileSystemContext();
  auto* const backend = ash::FileSystemBackend::Get(*file_system_context);
  DCHECK(backend);

  base::FilePath virtual_path;
  if (!backend->GetVirtualPath(volume->mount_path(), &virtual_path)) {
    std::move(error_callback).Run(file_system_api::kSecurityError);
    return;
  }

  storage::IsolatedContext* const isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  const storage::FileSystemURL original_url =
      file_system_context->CreateCrackedFileSystemURL(
          blink::StorageKey::CreateFirstParty(origin),
          storage::kFileSystemTypeExternal, virtual_path);

  // Set a fixed register name, as the automatic one would leak the mount point
  // directory.
  std::string register_name = "fs";
  const storage::IsolatedContext::ScopedFSHandle file_system =
      isolated_context->RegisterFileSystemForPath(
          storage::kFileSystemTypeLocalForPlatformApp,
          std::string() /* file_system_id */, original_url.path(),
          &register_name);
  if (!file_system.is_valid()) {
    std::move(error_callback).Run(file_system_api::kSecurityError);
    return;
  }

  backend->GrantFileAccessToOrigin(origin, virtual_path);

  // Grant file permissions to the renderer hosting component.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  DCHECK(policy);

  const auto process_id = requester->source_process_id();
  // Read-only permisisons.
  policy->GrantReadFile(process_id, volume->mount_path());
  policy->GrantReadFileSystem(process_id, file_system.id());

  // Additional write permissions.
  if (writable) {
    policy->GrantCreateReadWriteFile(process_id, volume->mount_path());
    policy->GrantCopyInto(process_id, volume->mount_path());
    policy->GrantWriteFileSystem(process_id, file_system.id());
    policy->GrantDeleteFromFileSystem(process_id, file_system.id());
    policy->GrantCreateFileForFileSystem(process_id, file_system.id());
  }

  std::move(success_callback).Run(file_system.id(), register_name);
}

}  // namespace

namespace file_system_api {

void DispatchVolumeListChangeEventAsh(
    content::BrowserContext* browser_context) {
  DCHECK(browser_context);
  EventRouter* const event_router = EventRouter::Get(browser_context);
  if (!event_router)  // Possible on shutdown.
    return;

  ExtensionRegistry* const registry = ExtensionRegistry::Get(browser_context);
  if (!registry)  // Possible on shutdown.
    return;

  std::unique_ptr<ConsentProvider> consent_provider =
      ExtensionsAPIClient::Get()->CreateConsentProvider(browser_context);

  file_system::VolumeListChangedEvent event_args;
  FillVolumeList(browser_context, &event_args.volumes);
  for (const auto& extension : registry->enabled_extensions()) {
    if (!consent_provider->IsGrantable(*extension.get())) {
      continue;
    }

    event_router->DispatchEventToExtension(
        extension->id(),
        std::make_unique<Event>(
            events::FILE_SYSTEM_ON_VOLUME_LIST_CHANGED,
            file_system::OnVolumeListChanged::kEventName,
            file_system::OnVolumeListChanged::Create(event_args)));
  }
}

}  // namespace file_system_api

/******** ChromeFileSystemDelegateAsh ********/

ChromeFileSystemDelegateAsh::ChromeFileSystemDelegateAsh() = default;

ChromeFileSystemDelegateAsh::~ChromeFileSystemDelegateAsh() = default;

void ChromeFileSystemDelegateAsh::RequestFileSystem(
    content::BrowserContext* browser_context,
    scoped_refptr<ExtensionFunction> requester,
    ConsentProvider* consent_provider,
    const Extension& extension,
    std::string volume_id,
    bool writable,
    FileSystemCallback success_callback,
    ErrorCallback error_callback) {
  using file_manager::Volume;
  using file_manager::VolumeManager;
  VolumeManager* const volume_manager = VolumeManager::Get(browser_context);
  DCHECK(volume_manager);

  if (writable &&
      !app_file_handler_util::HasFileSystemWritePermission(&extension)) {
    std::move(error_callback)
        .Run(file_system_api::kRequiresFileSystemWriteError);
    return;
  }

  if (!consent_provider->IsGrantable(extension)) {
    std::move(error_callback)
        .Run(file_system_api::kNotSupportedOnNonKioskSessionError);
    return;
  }

  base::WeakPtr<file_manager::Volume> volume =
      volume_manager->FindVolumeById(volume_id);
  if (!volume.get()) {
    std::move(error_callback).Run(file_system_api::kVolumeNotFoundError);
    return;
  }

  scoped_refptr<storage::FileSystemContext> file_system_context =
      util::GetStoragePartitionForExtensionId(extension.id(), browser_context)
          ->GetFileSystemContext();
  auto* const backend = ash::FileSystemBackend::Get(*file_system_context);
  DCHECK(backend);

  base::FilePath virtual_path;
  if (!backend->GetVirtualPath(volume->mount_path(), &virtual_path)) {
    std::move(error_callback).Run(file_system_api::kSecurityError);
    return;
  }

  if (writable && (volume->is_read_only())) {
    std::move(error_callback).Run(file_system_api::kSecurityError);
    return;
  }

  ConsentProvider::ConsentCallback callback =
      base::BindOnce(&OnConsentReceived, browser_context, requester,
                     std::move(success_callback), std::move(error_callback),
                     extension.origin(), volume, writable);

  consent_provider->RequestConsent(requester->render_frame_host(), extension,
                                   volume->volume_id(), volume->volume_label(),
                                   writable, std::move(callback));
}

void ChromeFileSystemDelegateAsh::GetVolumeList(
    content::BrowserContext* browser_context,
    VolumeListCallback success_callback,
    ErrorCallback /*error_callback*/) {
  std::vector<file_system::Volume> result_volume_list;
  FillVolumeList(browser_context, &result_volume_list);

  std::move(success_callback).Run(result_volume_list);
}

}  // namespace extensions
