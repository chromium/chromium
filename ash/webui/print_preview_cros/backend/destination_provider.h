// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PRINT_PREVIEW_CROS_BACKEND_DESTINATION_PROVIDER_H_
#define ASH_WEBUI_PRINT_PREVIEW_CROS_BACKEND_DESTINATION_PROVIDER_H_

#include <string>

#include "ash/webui/print_preview_cros/backend/pdf_printer_handler.h"
#include "ash/webui/print_preview_cros/mojom/destination_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/mojom/print.mojom.h"

namespace ash::printing::print_preview {

class DestinationProvider : public mojom::DestinationProvider {
 public:
  DestinationProvider();
  DestinationProvider(const DestinationProvider&) = delete;
  DestinationProvider& operator=(const DestinationProvider&) = delete;
  ~DestinationProvider() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::DestinationProvider> pending_receiver);

  // print_preview_cros::mojom::DestinationProvider:
  void FetchCapabilities(const std::string& destination_id,
                         ::printing::mojom::PrinterType type,
                         FetchCapabilitiesCallback callback) override;

 private:
  // Receives and dispatches method calls to this implementation of the
  // mojom::DestinationProvider interface.
  mojo::Receiver<mojom::DestinationProvider> receiver_{this};

  // Handles PDF type print requests.
  PdfPrinterHandler pdf_printer_handler_;
};

}  // namespace ash::printing::print_preview

#endif  // ASH_WEBUI_PRINT_PREVIEW_CROS_BACKEND_DESTINATION_PROVIDER_H_
