// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/operation.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/file_system_provider_service_ash.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension_id.h"

namespace ash {
namespace file_system_provider {
namespace operations {
namespace {

// This method is only used when Lacros is enabled. It's a callback from Lacros
// indicating whether the operation was successfully forwarded. If the operation
// could not be forwarded then the file system request manager must be informed.
void OperationForwarded(ash::file_system_provider::ProviderId provider_id,
                        const std::string& file_system_id,
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
  ProvidedFileSystemInterface* const file_system =
      service->GetProvidedFileSystem(provider_id, file_system_id);
  if (!file_system)
    return;
  file_system->GetRequestManager()->DestroyRequest(request_id);
}

// Default implementation for dispatching an event. Can be replaced for unit
// tests by Operation::SetDispatchEventImplForTest().
bool DispatchEventImpl(extensions::EventRouter* event_router,
                       const extensions::ExtensionId& extension_id,
                       ProviderId provider_id,
                       const std::string& file_system_id,
                       int request_id,

                       extensions::events::HistogramValue histogram_value,
                       const std::string& event_name,
                       std::vector<base::Value> event_args) {
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
    auto callback = base::BindOnce(&OperationForwarded, provider_id,
                                   file_system_id, request_id);
    (*remote)->ForwardOperation(
        extension_id, static_cast<int32_t>(histogram_value), event_name,
        std::move(event_args), std::move(callback));
  }
  return !remotes.empty();
}

}  // namespace

Operation::Operation(extensions::EventRouter* event_router,
                     const ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info),
      dispatch_event_impl_(base::BindRepeating(
          &DispatchEventImpl,
          event_router,
          file_system_info_.provider_id().GetExtensionId())) {}

Operation::~Operation() {
}

void Operation::SetDispatchEventImplForTesting(
    const DispatchEventImplCallback& callback) {
  auto wrapped_callback = base::BindRepeating(
      [](const DispatchEventImplCallback& callback, ProviderId provider_id,
         const std::string& file_system_id, int request_id,
         extensions::events::HistogramValue histogram_value,
         const std::string& event_name, std::vector<base::Value> event_args) {
        auto event = std::make_unique<extensions::Event>(
            histogram_value, event_name, std::move(event_args));
        return callback.Run(std::move(event));
      },
      callback);
  dispatch_event_impl_ = wrapped_callback;
}

bool Operation::SendEvent(int request_id,
                          extensions::events::HistogramValue histogram_value,
                          const std::string& event_name,
                          std::vector<base::Value> event_args) {
  return dispatch_event_impl_.Run(
      file_system_info_.provider_id(), file_system_info_.file_system_id(),
      request_id, histogram_value, event_name, std::move(event_args));
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace ash
