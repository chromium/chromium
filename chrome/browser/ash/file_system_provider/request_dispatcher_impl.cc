// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/request_dispatcher_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"
#include "chrome/browser/ash/file_system_provider/service_worker_lifetime_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "extensions/browser/event_router.h"
#include "url/gurl.h"

namespace ash::file_system_provider {

RequestDispatcherImpl::RequestDispatcherImpl(
    const extensions::ExtensionId& extension_id,
    extensions::EventRouter* event_router,
    ServiceWorkerLifetimeManager* sw_lifetime_manager)
    : extension_id_(extension_id),
      event_router_(event_router),
      sw_lifetime_manager_(sw_lifetime_manager) {}

RequestDispatcherImpl::~RequestDispatcherImpl() = default;

bool RequestDispatcherImpl::DispatchRequest(
    int request_id,
    std::optional<std::string> file_system_id,
    std::unique_ptr<extensions::Event> event) {
  if (chromeos::features::IsUploadOfficeToCloudEnabled()) {
    DCHECK(!event->did_dispatch_callback);
    RequestKey request_key{extension_id_, file_system_id.value_or(""),
                           request_id};
    event->did_dispatch_callback =
        sw_lifetime_manager_->CreateDispatchCallbackForRequest(request_key);
  }

  // If ash has a matching extension, forward the event.
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
  // Attempt to cancel. Checking if an extension is listening could lead
  // to a situation where an extension stops listening between the time
  // a request is sent and the time it's cancelled, so cancellation could
  // fail and the extension's service worker could be left running
  // permanently.
  sw_lifetime_manager_->FinishRequest(
      {extension_id_, file_system_id.value_or(""), request_id});
}

}  // namespace ash::file_system_provider
