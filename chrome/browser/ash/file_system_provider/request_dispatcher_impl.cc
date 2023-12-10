// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/request_dispatcher_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/file_system_provider_service_ash.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/chromeos/extensions/file_system_provider/service_worker_lifetime_manager.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "extensions/browser/event_router.h"
#include "url/gurl.h"

namespace ash::file_system_provider {

RequestDispatcherImpl::RequestDispatcherImpl(
    const extensions::ExtensionId& extension_id,
    extensions::EventRouter* event_router,
    ForwardResultCallback forward_result_callback,
    extensions::file_system_provider::ServiceWorkerLifetimeManager*
        sw_lifetime_manager)
    : extension_id_(extension_id),
      event_router_(event_router),
      forward_result_callback_(forward_result_callback),
      sw_lifetime_manager_(sw_lifetime_manager) {}

RequestDispatcherImpl::~RequestDispatcherImpl() = default;

bool RequestDispatcherImpl::DispatchRequest(
    int request_id,
    std::optional<std::string> file_system_id,
    std::unique_ptr<extensions::Event> event) {
  if (chromeos::features::IsUploadOfficeToCloudEnabled()) {
    DCHECK(!event->did_dispatch_callback);
    extensions::file_system_provider::RequestKey request_key{
        extension_id_, file_system_id.value_or(""), request_id};
    event->did_dispatch_callback =
        sw_lifetime_manager_->CreateDispatchCallbackForRequest(request_key);
  }

  // If ash has a matching extension, forward the event. This should not be
  // needed once Lacros is the only browser on all devices.
  if (event_router_->ExtensionHasEventListener(extension_id_,
                                               event->event_name)) {
    event_router_->DispatchEventToExtension(extension_id_, std::move(event));
    if (chromeos::features::IsUploadOfficeToCloudEnabled()) {
      sw_lifetime_manager_->StartRequest(
          {extension_id_, file_system_id.value_or(""), request_id});
    }
    return true;
  }

  if (extension_id_ == guest_os::kTerminalSystemAppId) {
    GURL terminal(chrome::kChromeUIUntrustedTerminalURL);
    if (event_router_->URLHasEventListener(terminal, event->event_name)) {
      event_router_->DispatchEventToURL(terminal, std::move(event));
      if (chromeos::features::IsUploadOfficeToCloudEnabled()) {
        sw_lifetime_manager_->StartRequest(
            {extension_id_, file_system_id.value_or(""), request_id});
      }
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
    if (chromeos::features::IsUploadOfficeToCloudEnabled()) {
      auto callback =
          base::BindOnce(&RequestDispatcherImpl::OperationForwarded,
                         weak_ptr_factory_.GetWeakPtr(), request_id);
      (*remote)->ForwardRequest(extension_id_, file_system_id, request_id,
                                static_cast<int32_t>(event->histogram_value),
                                std::move(event->event_name),
                                std::move(event->event_args),
                                std::move(callback));
    } else {
      auto callback =
          base::BindOnce(&RequestDispatcherImpl::OperationForwardedDeprecated,
                         weak_ptr_factory_.GetWeakPtr(), request_id);
      (*remote)->ForwardOperation(
          extension_id_, static_cast<int32_t>(event->histogram_value),
          std::move(event->event_name), std::move(event->event_args),
          std::move(callback));
    }
    return true;
  }
  // Unable to dispatch the request to the target extension in either ash or
  // lacros.
  LOG(ERROR) << "Unable to dispatch request to target extension: "
             << extension_id_;
  return false;
}

void RequestDispatcherImpl::CancelRequest(
    int request_id,
    std::optional<std::string> file_system_id) {
  if (!chromeos::features::IsUploadOfficeToCloudEnabled()) {
    return;
  }
  // Don't bother checking if the original request was sent locally or to Lacros
  // by checking if an extension is listening, attempt to cancel both locally
  // AND in Lacros: one of the calls will be a no-op. Checking if an extension
  // is listening could lead to a situation where an extension stops listening
  // between the time a request is sent and the time it's cancelled, so
  // cancellation could fail and the extension's service worker could be left
  // running permanently.
  sw_lifetime_manager_->FinishRequest(
      {extension_id_, file_system_id.value_or(""), request_id});
  auto& remotes = crosapi::CrosapiManager::Get()
                      ->crosapi_ash()
                      ->file_system_provider_service_ash()
                      ->remotes();
  if (!remotes.empty()) {
    auto remote = remotes.begin();
    (*remote)->CancelRequest(extension_id_, std::move(file_system_id),
                             request_id);
  }
}

void RequestDispatcherImpl::OperationForwarded(
    int request_id,
    crosapi::mojom::FSPForwardResult result) {
  switch (result) {
    case crosapi::mojom::FSPForwardResult::kSuccess:
      // Successful deliveries will get a response through the
      // FileSystemProvider mojom path.
      break;
    case crosapi::mojom::FSPForwardResult::kInternalError:
    case crosapi::mojom::FSPForwardResult::kUnknown:
      forward_result_callback_.Run(request_id, base::File::FILE_ERROR_FAILED);
      break;
    case crosapi::mojom::FSPForwardResult::kNoListener:
      forward_result_callback_.Run(request_id, base::File::FILE_ERROR_SECURITY);
      break;
  }
}

void RequestDispatcherImpl::OperationForwardedDeprecated(
    int request_id,
    bool delivery_failure) {
  // Successful deliveries will get a response through the FileSystemProvider
  // mojom path.
  if (!delivery_failure) {
    return;
  }
  forward_result_callback_.Run(request_id, base::File::FILE_ERROR_FAILED);
}

}  // namespace ash::file_system_provider
