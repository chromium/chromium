// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/mount_request_handler.h"

#include <utility>

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/file_system_provider_service_ash.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "extensions/browser/event_router.h"

namespace ash::file_system_provider {
namespace {

// This method is only used when Lacros is enabled. It's a callback from Lacros
// indicating whether the mount request was successfully forwarded.
void OperationForwarded(ash::file_system_provider::ProviderId provider_id,
                        int request_id,
                        bool delivery_failure) {
  using ash::file_system_provider::Service;
  // Successful deliveries will go through the FileSystemProvider mojom path.
  if (!delivery_failure)
    return;
  // When Lacros is enabled the primary profile is the only profile.
  Service* const service =
      Service::Get(ProfileManager::GetPrimaryUserProfile());
  DCHECK(service);
  ProviderInterface* const provider = service->GetProvider(provider_id);
  if (!provider)
    return;
  provider->GetRequestManager()->RejectRequest(request_id,
                                               std::make_unique<RequestValue>(),
                                               base::File::FILE_ERROR_FAILED);
}

// Implementation for dispatching an event.
bool DispatchEventImpl(extensions::EventRouter* event_router,
                       ProviderId provider_id,
                       int request_id) {
  base::Value::List event_args;
  event_args.reserve(1);
  event_args.Append(base::Value(request_id));

  const extensions::ExtensionId extension_id = provider_id.GetExtensionId();
  extensions::events::HistogramValue histogram_value =
      extensions::events::FILE_SYSTEM_PROVIDER_ON_MOUNT_REQUESTED;
  const std::string event_name =
      extensions::api::file_system_provider::OnMountRequested::kEventName;

  // If ash has a matching extension, forward the event. This should not be
  // needed once Lacros is the only browser on all devices.
  if (event_router->ExtensionHasEventListener(extension_id, event_name)) {
    event_router->DispatchEventToExtension(
        extension_id, std::make_unique<extensions::Event>(
                          histogram_value, event_name, std::move(event_args)));
    return true;
  }

  // If there are any Lacros remotes, forward the message to the first one. This
  // does not support multiple remotes.
  auto& remotes = crosapi::CrosapiManager::Get()
                      ->crosapi_ash()
                      ->file_system_provider_service_ash()
                      ->remotes();
  if (!remotes.empty()) {
    auto remote = remotes.begin();
    auto callback =
        base::BindOnce(&OperationForwarded, provider_id, request_id);
    (*remote)->ForwardOperation(
        extension_id, static_cast<int32_t>(histogram_value), event_name,
        std::move(event_args), std::move(callback));
  }
  return !remotes.empty();
}

}  // namespace

MountRequestHandler::MountRequestHandler(extensions::EventRouter* event_router,
                                         ProviderId provider_id,
                                         RequestMountCallback callback)
    : dispatch_event_impl_(
          base::BindRepeating(&DispatchEventImpl, event_router, provider_id)),
      callback_(std::move(callback)) {}

MountRequestHandler::~MountRequestHandler() = default;

bool MountRequestHandler::Execute(int request_id) {
  return dispatch_event_impl_.Run(request_id);
}

void MountRequestHandler::OnSuccess(int /* request_id */,
                                    std::unique_ptr<RequestValue> /* result */,
                                    bool has_more) {
  // File handle is the same as request id of the OpenFile operation.
  DCHECK(callback_);
  std::move(callback_).Run(base::File::FILE_OK);
}

void MountRequestHandler::OnError(int /* request_id */,
                                  std::unique_ptr<RequestValue> /* result */,
                                  base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(error);
}

}  // namespace ash::file_system_provider
