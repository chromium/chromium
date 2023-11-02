// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate_lacros.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/api/file_system/consent_provider.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace file_system = api::file_system;

using extensions::app_file_handler_util::CreateFileEntryWithPermissions;

namespace {

const char kApiUnavailableError[] = "API unavailable.";
const char kRenderFrameHostGoneError[] = "Render frame host gone.";

// Volume list converter that excludes volumes unsupported by lacros-chrome.
void ConvertAndFilterMojomToVolumeList(
    const std::vector<crosapi::mojom::VolumePtr>& src_volume_list,
    std::vector<file_system::Volume>* dst_volume_list) {
  DCHECK(dst_volume_list->empty());
  for (auto& src_volume : src_volume_list) {
    if (src_volume->is_available_to_lacros) {
      file_system::Volume dst_volume;
      dst_volume.volume_id = src_volume->volume_id;
      dst_volume.writable = src_volume->writable;
      dst_volume_list->emplace_back(std::move(dst_volume));
    }
  }
}

}  // namespace

namespace file_system_api {

void DispatchVolumeListChangeEventLacros(
    content::BrowserContext* browser_context,
    const std::vector<crosapi::mojom::VolumePtr>& volume_list) {
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
  // Note: Events are still fired even if:
  // * The *filtered* volume list does not change.
  // * The filtered volume list is empty.
  // This is done for simplicy: Detecting change in filtered volume list will
  // requires caching volume list on Lacros side; preventing empty filtered
  // volume list from triggering an event will lead to inconsistencies compared
  // to polling via getVolumeList().
  ConvertAndFilterMojomToVolumeList(volume_list, &event_args.volumes);
  for (const auto& extension : registry->enabled_extensions()) {
    if (!consent_provider->IsGrantable(*extension))
      continue;

    event_router->DispatchEventToExtension(
        extension->id(),
        std::make_unique<Event>(
            events::FILE_SYSTEM_ON_VOLUME_LIST_CHANGED,
            file_system::OnVolumeListChanged::kEventName,
            file_system::OnVolumeListChanged::Create(event_args)));
  }
}

}  // namespace file_system_api

/******** ChromeFileSystemDelegateLacros ********/

namespace {

void OnConsentReceived(
    scoped_refptr<ExtensionFunction> requester,
    bool want_writable,
    ChromeFileSystemDelegate::FileSystemCallback success_callback,
    ChromeFileSystemDelegate::ErrorCallback error_callback,
    base::FilePath mount_path,
    ConsentProvider::Consent result) {
  // Render frame host can be gone before this callback executes.
  if (!requester->render_frame_host()) {
    std::move(error_callback).Run(kRenderFrameHostGoneError);
    return;
  }

  const char* consent_err_msg = file_system_api::ConsentResultToError(result);
  if (consent_err_msg) {
    std::move(error_callback).Run(consent_err_msg);
    return;
  }

  const auto process_id = requester->source_process_id();
  extensions::GrantedFileEntry granted_file_entry =
      CreateFileEntryWithPermissions(process_id, mount_path,
                                     /*can_write=*/want_writable,
                                     /*can_create=*/want_writable,
                                     /*can_delete=*/want_writable);
  std::move(success_callback)
      .Run(granted_file_entry.filesystem_id,
           granted_file_entry.registered_name);
}

void OnCrosapiGetVolumeMountInfo(
    scoped_refptr<ExtensionFunction> requester,
    ConsentProvider* consent_provider,
    bool want_writable,
    ChromeFileSystemDelegate::FileSystemCallback success_callback,
    ChromeFileSystemDelegate::ErrorCallback error_callback,
    crosapi::mojom::VolumePtr crosapi_volume) {
  if (!crosapi_volume || !crosapi_volume->is_available_to_lacros) {
    std::move(error_callback).Run(file_system_api::kVolumeNotFoundError);
    return;
  }
  if (want_writable && !crosapi_volume->writable) {
    std::move(error_callback).Run(file_system_api::kSecurityError);
    return;
  }

  ConsentProvider::ConsentCallback callback = base::BindOnce(
      &OnConsentReceived, requester, want_writable, std::move(success_callback),
      std::move(error_callback), crosapi_volume->mount_path);

  consent_provider->RequestConsent(
      requester->render_frame_host(), *requester->extension(),
      crosapi_volume->volume_id, crosapi_volume->volume_label, want_writable,
      std::move(callback));
}

}  // namespace

ChromeFileSystemDelegateLacros::ChromeFileSystemDelegateLacros() = default;

ChromeFileSystemDelegateLacros::~ChromeFileSystemDelegateLacros() = default;

void ChromeFileSystemDelegateLacros::RequestFileSystem(
    content::BrowserContext* /*browser_context*/,
    scoped_refptr<ExtensionFunction> requester,
    ConsentProvider* consent_provider,
    const Extension& extension,
    std::string volume_id,
    bool writable,
    FileSystemCallback success_callback,
    ErrorCallback error_callback) {
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

  auto* lacros_service = chromeos::LacrosService::Get();
  DCHECK(lacros_service);
  if (!lacros_service->IsAvailable<crosapi::mojom::VolumeManager>()) {
    std::move(error_callback).Run(kApiUnavailableError);
    return;
  }

  // |consent_provider| lives at least as long as |requester|. One way to do
  // this is through composition: |requester| is ref counted, keeping
  // |consent_provider| alive after the crosapi call below.
  lacros_service->GetRemote<crosapi::mojom::VolumeManager>()
      ->GetVolumeMountInfo(
          volume_id, base::BindOnce(&OnCrosapiGetVolumeMountInfo, requester,
                                    consent_provider, writable,
                                    std::move(success_callback),
                                    std::move(error_callback)));
}

void ChromeFileSystemDelegateLacros::GetVolumeList(
    content::BrowserContext* /*browser_context*/,
    VolumeListCallback success_callback,
    ErrorCallback error_callback) {
  auto* lacros_service = chromeos::LacrosService::Get();
  DCHECK(lacros_service);
  if (!lacros_service->IsAvailable<crosapi::mojom::VolumeManager>()) {
    std::move(error_callback).Run(kApiUnavailableError);
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::VolumeManager>()->GetFullVolumeList(
      base::BindOnce(
          [](VolumeListCallback success_callback,
             std::vector<crosapi::mojom::VolumePtr> src_volume_list) {
            std::vector<file_system::Volume> filtered_volume_list;
            ConvertAndFilterMojomToVolumeList(src_volume_list,
                                              &filtered_volume_list);
            std::move(success_callback).Run(filtered_volume_list);
          },
          std::move(success_callback)));
}

}  // namespace extensions
