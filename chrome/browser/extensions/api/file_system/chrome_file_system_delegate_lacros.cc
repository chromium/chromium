// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate_lacros.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/file_system/consent_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace file_system = api::file_system;

using extensions::app_file_handler_util::CreateFileEntryWithPermissions;
using file_system_api::ConsentProvider;
using file_system_api::ConsentProviderDelegate;

namespace {

const char kApiUnavailableError[] = "API unavailable.";
const char kProfileGoneError[] = "Profile gone.";
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

  // TODO(crbug.com/1351493): Simplify usage for IsGrantable().
  ConsentProviderDelegate consent_provider_delegate(
      Profile::FromBrowserContext(browser_context));
  ConsentProvider consent_provider(&consent_provider_delegate);

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
    if (!consent_provider.IsGrantable(*extension))
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

namespace {

/******** RequestFileSystemExecutor ********/

// Executor for chrome.requestFileSystem(), with async steps:
// 1. Crosapi call to get volume info.
// 2. (Potentially) request consent via dialog.
// Sources of complexity:
// * Lifetime: Instances are ref counted, and are kept alive via callback
//   binding.
// * Profile: (2) requires |profile_|, which may disappear while awaiting (1)!
//   This is handled by observing |profile_|: If it is destroyed then abort
//   before (2); else proceeds with (2) and unobserve ASAP.
// * Fulfillment: To ensure the request is fulfilled, one of |success_callback|
//   or |error_callback| gets called eventually (via FinishWith*()).
class RequestFileSystemExecutor
    : public base::RefCountedThreadSafe<RequestFileSystemExecutor>,
      public ProfileObserver {
 public:
  RequestFileSystemExecutor(
      Profile* profile,
      scoped_refptr<ExtensionFunction> requester,
      const std::string& volume_id,
      bool writable,
      ChromeFileSystemDelegate::FileSystemCallback success_callback,
      ChromeFileSystemDelegate::ErrorCallback error_callback);
  RequestFileSystemExecutor(const RequestFileSystemExecutor&) = delete;
  RequestFileSystemExecutor& operator=(const RequestFileSystemExecutor&) =
      delete;

  // Entry point for executor flow.
  void Run(chromeos::LacrosService* lacros_service);

 private:
  friend class base::RefCountedThreadSafe<RequestFileSystemExecutor>;
  ~RequestFileSystemExecutor() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Callback for (1), on receiving volume info from crosapi.
  void OnCrosapiGetVolumeMountInfo(crosapi::mojom::VolumePtr crosapi_volume);

  // Callback for (2), on consent granting or denial.
  void OnConsentReceived(base::FilePath mount_path,
                         ConsentProvider::Consent result);

  // Consumes |error_callback_| to pass |error| on error.
  void FinishWithError(const std::string& error);

  // Consumes |success_callback_| to pass results on success.
  void FinishWithResponse(const std::string& filesystem_id,
                          const std::string& registered_name);

  // |profile_| can be a raw pointer since its destruction is observed.
  base::raw_ptr<Profile> profile_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  scoped_refptr<ExtensionFunction> requester_;
  const std::string volume_id_;
  const bool want_writable_;
  ChromeFileSystemDelegate::FileSystemCallback success_callback_;
  ChromeFileSystemDelegate::ErrorCallback error_callback_;
};

RequestFileSystemExecutor::RequestFileSystemExecutor(
    Profile* profile,
    scoped_refptr<ExtensionFunction> requester,
    const std::string& volume_id,
    bool want_writable,
    ChromeFileSystemDelegate::FileSystemCallback success_callback,
    ChromeFileSystemDelegate::ErrorCallback error_callback)
    : profile_(profile),
      requester_(requester),
      volume_id_(volume_id),
      want_writable_(want_writable),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)) {
  profile_observation_.Observe(profile_);
}

void RequestFileSystemExecutor::Run(chromeos::LacrosService* lacros_service) {
  // All code path from here must lead to either |success_callback_| or
  // |error_callback_| getting called.
  lacros_service->GetRemote<crosapi::mojom::VolumeManager>()
      ->GetVolumeMountInfo(
          volume_id_,
          base::BindOnce(
              &RequestFileSystemExecutor::OnCrosapiGetVolumeMountInfo, this));
}

RequestFileSystemExecutor::~RequestFileSystemExecutor() = default;

void RequestFileSystemExecutor::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile_, profile);
  profile_observation_.Reset();
  profile_ = nullptr;
}

void RequestFileSystemExecutor::OnCrosapiGetVolumeMountInfo(
    crosapi::mojom::VolumePtr crosapi_volume) {
  // Profile can be gone before this callback executes, while awaiting crosapi.
  if (!profile_) {
    FinishWithError(kProfileGoneError);
    return;
  }
  if (!crosapi_volume || !crosapi_volume->is_available_to_lacros) {
    FinishWithError(file_system_api::kVolumeNotFoundError);
    return;
  }
  if (want_writable_ && !crosapi_volume->writable) {
    FinishWithError(file_system_api::kSecurityError);
    return;
  }

  // TODO(crbug.com/1351493): Simplify usage for RequestConsent().
  ConsentProviderDelegate consent_provider_delegate(profile_);
  ConsentProvider consent_provider(&consent_provider_delegate);

  ConsentProvider::ConsentCallback callback =
      base::BindOnce(&RequestFileSystemExecutor::OnConsentReceived, this,
                     crosapi_volume->mount_path);

  consent_provider.RequestConsent(
      requester_->render_frame_host(), *requester_->extension(),
      crosapi_volume->volume_id, crosapi_volume->volume_label, want_writable_,
      std::move(callback));

  // Done with |profile_|, so stop observing.
  profile_observation_.Reset();
  profile_ = nullptr;
}

void RequestFileSystemExecutor::OnConsentReceived(
    base::FilePath mount_path,
    ConsentProvider::Consent result) {
  // Render frame host can be gone before this callback executes.
  if (!requester_->render_frame_host()) {
    FinishWithError(kRenderFrameHostGoneError);
    return;
  }

  const char* consent_err_msg = file_system_api::ConsentResultToError(result);
  if (consent_err_msg) {
    FinishWithError(consent_err_msg);
    return;
  }

  const auto process_id = requester_->source_process_id();
  extensions::GrantedFileEntry granted_file_entry =
      CreateFileEntryWithPermissions(process_id, mount_path,
                                     /*can_write=*/want_writable_,
                                     /*can_create=*/want_writable_,
                                     /*can_delete=*/want_writable_);
  FinishWithResponse(granted_file_entry.filesystem_id,
                     granted_file_entry.registered_name);
}

void RequestFileSystemExecutor::FinishWithError(const std::string& error) {
  std::move(error_callback_).Run(error);
}

void RequestFileSystemExecutor::FinishWithResponse(
    const std::string& filesystem_id,
    const std::string& registered_name) {
  std::move(success_callback_).Run(filesystem_id, registered_name);
}

}  // namespace

/******** ChromeFileSystemDelegateLacros ********/

ChromeFileSystemDelegateLacros::ChromeFileSystemDelegateLacros() = default;

ChromeFileSystemDelegateLacros::~ChromeFileSystemDelegateLacros() = default;

void ChromeFileSystemDelegateLacros::RequestFileSystem(
    content::BrowserContext* browser_context,
    scoped_refptr<ExtensionFunction> requester,
    const Extension& extension,
    std::string volume_id,
    bool writable,
    FileSystemCallback success_callback,
    ErrorCallback error_callback) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // TODO(crbug.com/1351493): Simplify usage for IsGrantable().
  ConsentProviderDelegate consent_provider_delegate(profile);
  ConsentProvider consent_provider(&consent_provider_delegate);

  if (writable &&
      !app_file_handler_util::HasFileSystemWritePermission(&extension)) {
    std::move(error_callback)
        .Run(file_system_api::kRequiresFileSystemWriteError);
    return;
  }

  if (!consent_provider.IsGrantable(extension)) {
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

  // The executor object is kept alive by its presence in callbacks, and
  // deleted when callbacks are invoked or cleared.
  scoped_refptr<RequestFileSystemExecutor> executor =
      new RequestFileSystemExecutor(profile, requester, volume_id, writable,
                                    std::move(success_callback),
                                    std::move(error_callback));
  executor->Run(lacros_service);
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
