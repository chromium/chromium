// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/event_dispatcher_impl.h"

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/file_system_provider_service_ash.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "extensions/browser/event_router.h"
#include "url/gurl.h"

namespace ash::file_system_provider {

EventDispatcherImpl::EventDispatcherImpl(
    const extensions::ExtensionId& extension_id,
    extensions::EventRouter* event_router,
    RequestManager* request_manager)
    : extension_id_(extension_id),
      event_router_(event_router),
      request_manager_(request_manager) {}

EventDispatcherImpl::~EventDispatcherImpl() = default;

bool EventDispatcherImpl::DispatchEvent(
    int request_id,
    absl::optional<std::string> file_system_id,
    std::unique_ptr<extensions::Event> event) {
  // If ash has a matching extension, forward the event. This should not be
  // needed once Lacros is the only browser on all devices.
  if (event_router_->ExtensionHasEventListener(extension_id_,
                                               event->event_name)) {
    event_router_->DispatchEventToExtension(extension_id_, std::move(event));
    return true;
  }

  if (extension_id_ == guest_os::kTerminalSystemAppId) {
    GURL terminal(chrome::kChromeUIUntrustedTerminalURL);
    if (event_router_->URLHasEventListener(terminal, event->event_name)) {
      event_router_->DispatchEventToURL(terminal, std::move(event));
      return true;
    }
  }

  // If there are any Lacros remotes, forward the message to the first one. This
  // does not support multiple remotes.
  auto& remotes = crosapi::CrosapiManager::Get()
                      ->crosapi_ash()
                      ->file_system_provider_service_ash()
                      ->remotes();
  if (!remotes.empty()) {
    auto remote = remotes.begin();
    auto callback = base::BindOnce(&EventDispatcherImpl::OperationForwarded,
                                   weak_ptr_factory_.GetWeakPtr(), request_id);
    (*remote)->ForwardOperation(
        extension_id_, static_cast<int32_t>(event->histogram_value),
        std::move(event->event_name), std::move(event->event_args),
        std::move(callback));
  }
  return !remotes.empty();
}

void EventDispatcherImpl::OperationForwarded(int request_id,
                                             bool delivery_failure) {
  // Successful deliveries will get a response through the FileSystemProvider
  // mojom path.
  if (!delivery_failure) {
    return;
  }
  request_manager_->RejectRequest(request_id, std::make_unique<RequestValue>(),
                                  base::File::FILE_ERROR_FAILED);
}

}  // namespace ash::file_system_provider
