// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_preview_cros/backend/destination_provider.h"

#include <string>
#include <utility>

#include "ash/webui/print_preview_cros/mojom/destination_provider.mojom.h"
#include "ash/webui/print_preview_cros/mojom/printer_capabilities.mojom.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "printing/mojom/print.mojom.h"

namespace ash::printing::print_preview {

DestinationProvider::DestinationProvider() {}
DestinationProvider::~DestinationProvider() = default;

void DestinationProvider::BindInterface(
    mojo::PendingReceiver<mojom::DestinationProvider> pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

void DestinationProvider::FetchCapabilities(
    const std::string& destination_id,
    ::printing::mojom::PrinterType printerType,
    FetchCapabilitiesCallback callback) {
  if (printerType != ::printing::mojom::PrinterType::kPdf) {
    mojo::ReportBadMessage("No support for non-PDF destinations");
    return;
  }

  mojom::CapabilitiesPtr caps =
      pdf_printer_handler_.FetchCapabilities(destination_id);
  std::move(callback).Run(std::move(caps));
}

}  // namespace ash::printing::print_preview
