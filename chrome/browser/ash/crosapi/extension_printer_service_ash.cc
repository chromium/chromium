// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/extension_printer_service_ash.h"

#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/extension_printer.mojom.h"

namespace crosapi {

namespace {

void ReportNumberOfLacrosExtensionPrinters(size_t number) {
  base::UmaHistogramCounts100000(
      "Printing.LacrosExtensions.FromAsh.NumberOfPrinters", number);
}

}  // namespace

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
  // In theory, pending requests should not exist when there is no provider.
  if (!HasProvider() && HasAnyPendingGetPrintersRequests()) {
    LOG(WARNING) << "ExtensionPrinterServiceAsh::ClearPendingRequests():none "
                    "ExtensionPrinterServiceProvider available";
  }
  // Clear pending get printers requests if any.
  pending_printers_added_callbacks_.clear();
  pending_get_printers_done_callbacks_.clear();
  total_printers_so_far_.clear();

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
    total_printers_so_far_[request_id] += printers.size();
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

  // Record number of printers if any from all printing extensions.
  if (total_printers_so_far_[request_id] > 0) {
    ReportNumberOfLacrosExtensionPrinters(total_printers_so_far_[request_id]);
    total_printers_so_far_.erase(request_id);
  }
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
    LOG(WARNING) << "ExtensionPrinterServiceAsh::StartGetPrinters: none "
                    "ExtensionPrinterServiceProvider available";
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

void ExtensionPrinterServiceAsh::Reset() {
  VLOG(1) << "ExtensionPrinterServiceAsh::Reset():";
  // Clears local states.
  ClearPendingRequests();
  // Asks downstream to clear states.
  if (HasProvider()) {
    service_provider_->DispatchResetRequest();
  }
}

void ExtensionPrinterServiceAsh::StartGetCapability(
    const std::string& destination_id,
    GetCapabilityCallback callback) {
  VLOG(1) << "ExtensionPrinterServiceAsh::StartGetCapability():"
          << " destination_id=" << destination_id;
  if (!HasProvider()) {
    LOG(WARNING) << "ExtensionPrinterServiceAsh::StartGetCapability(): none "
                    "ExtensionPrinterServiceProvider available";
    std::move(callback).Run(base::Value::Dict());
    return;
  }
  service_provider_->DispatchStartGetCapability(destination_id,
                                                std::move(callback));
}

bool ExtensionPrinterServiceAsh::HasAnyPendingGetPrintersRequests() const {
  return !pending_get_printers_done_callbacks_.empty() ||
         !pending_printers_added_callbacks_.empty();
}

void ExtensionPrinterServiceAsh::StartPrint(
    const std::u16string& job_title,
    base::Value::Dict settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    StartPrintCallback callback) {
  VLOG(1) << "ExtensionPrinterServiceAsh::StartPrint():" << " job_title="
          << job_title;
  if (!HasProvider()) {
    LOG(WARNING) << "ExtensionPrinterServiceAsh::StartPrint(): none "
                    "ExtensionPrinterServiceProvider available";
    std::move(callback).Run(crosapi::mojom::StartPrintStatus::kFailed);
    return;
  }
  service_provider_->DispatchStartPrint(job_title, std::move(settings),
                                        print_data, std::move(callback));
}

void ExtensionPrinterServiceAsh::StartGrantPrinterAccess(
    const std::string& printer_id,
    GetPrinterInfoCallback callback) {
  VLOG(1) << "ExtensionPrinterServiceAsh::StartGrantPrinterAccess():"
          << " printer_id=" << printer_id;
  if (!HasProvider()) {
    LOG(WARNING)
        << "ExtensionPrinterServiceAsh::StartGrantPrinterAccess(): none "
           "ExtensionPrinterServiceProvider available";
    std::move(callback).Run(base::Value::Dict());
    return;
  }
  service_provider_->DispatchStartGrantPrinterAccess(printer_id,
                                                     std::move(callback));
}

bool ExtensionPrinterServiceAsh::HasPendingGetPrintersRequestForTesting(
    base::UnguessableToken& request_id) const {
  CHECK_IS_TEST();
  return pending_get_printers_done_callbacks_.contains(request_id) &&
         pending_printers_added_callbacks_.contains(request_id);
}

bool ExtensionPrinterServiceAsh::HasProviderForTesting() const {
  CHECK_IS_TEST();
  return HasProvider();
}

}  // namespace crosapi
