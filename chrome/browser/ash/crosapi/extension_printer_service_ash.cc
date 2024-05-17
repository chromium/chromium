// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/extension_printer_service_ash.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/extension_printer.mojom.h"

namespace crosapi {

ExtensionPrinterServiceAsh::ExtensionPrinterServiceAsh() = default;

ExtensionPrinterServiceAsh::~ExtensionPrinterServiceAsh() = default;

void ExtensionPrinterServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ExtensionPrinterService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

bool ExtensionPrinterServiceAsh::HasProvider() const {
  return service_provider_.is_bound() && service_provider_.is_connected();
}

void ExtensionPrinterServiceAsh::ClearPendingRequests() {
  // Clear pending get printers requests if any.
  pending_printers_added_callbacks_.clear();
  pending_get_printers_done_callbacks_.clear();

  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExtensionPrinterServiceAsh::RegisterServiceProvider(
    mojo::PendingRemote<mojom::ExtensionPrinterServiceProvider> provider) {
  VLOG(1) << "ExtensionPrinterServiceAsh::RegisterServiceProvider()";
  service_provider_ =
      mojo::Remote<mojom::ExtensionPrinterServiceProvider>(std::move(provider));
  service_provider_.set_disconnect_handler(base::BindOnce(
      &ExtensionPrinterServiceAsh::ExtensionPrinterServiceProviderDisconnected,
      weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionPrinterServiceAsh::PrintersAdded(
    const base::UnguessableToken& request_id,
    base::Value::List printers,
    bool is_done) {
  VLOG(1) << "ExtensionPrinterServiceAsh::PrintersAdded():" << " request_id="
          << request_id.ToString() << " printers.size()=" << printers.size()
          << " done=" << is_done;

  if (!printers.empty() &&
      pending_printers_added_callbacks_.contains(request_id)) {
    pending_printers_added_callbacks_[request_id].Run(std::move(printers));
  }

  if (!is_done) {
    return;
  }
  // Calls the done callback and clear the caches for the request_id.
  if (pending_get_printers_done_callbacks_.contains(request_id)) {
    std::move(pending_get_printers_done_callbacks_[request_id]).Run();
    pending_get_printers_done_callbacks_.erase(request_id);
  }
  pending_printers_added_callbacks_.erase(request_id);
}

void ExtensionPrinterServiceAsh::ExtensionPrinterServiceProviderDisconnected() {
  VLOG(1) << "ExtensionPrinterServiceProviderDisconnected()";

  ClearPendingRequests();
}

void ExtensionPrinterServiceAsh::StartGetPrinters(
    AddedPrintersCallback added_printers_callback,
    GetPrintersDoneCallback done_callback) {
  // Checks whether there is any ExtensionPrinterServiceProvider registered.
  if (!HasProvider()) {
    LOG(WARNING) << "StartGetPrinters: none ExtensionPrinterServiceProvider";
    std::move(done_callback).Run();
    return;
  }

  // Generates a request_id and caches the callbacks.
  base::UnguessableToken request_id = base::UnguessableToken::Create();
  pending_printers_added_callbacks_[request_id] =
      std::move(added_printers_callback);
  pending_get_printers_done_callbacks_[request_id] = std::move(done_callback);

  VLOG(1) << "ExtensionPrinterServiceAsh::StartGetPrinters():" << " request_id="
          << request_id.ToString();
  service_provider_->DispatchGetPrintersRequest(request_id);
}

}  // namespace crosapi
