// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_service_chromeos.h"

#include <utility>

#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#include "content/public/browser/render_frame_host.h"

namespace printing {

WebPrintingServiceChromeOS::WebPrintingServiceChromeOS(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebPrintingService> receiver)
    : DocumentService(*render_frame_host, std::move(receiver)) {}

WebPrintingServiceChromeOS::~WebPrintingServiceChromeOS() = default;

void WebPrintingServiceChromeOS::GetPrinters(GetPrintersCallback callback) {
  GetLocalPrinterInterface()->GetPrinters(
      base::BindOnce(&WebPrintingServiceChromeOS::OnPrintersRetrieved,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebPrintingServiceChromeOS::OnPrintersRetrieved(
    GetPrintersCallback callback,
    std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) {
  // TODO(b/302505962): Figure out the correct permissions UX.
  std::vector<blink::mojom::WebPrinterInfoPtr> web_printers;
  for (const auto& printer : printers) {
    mojo::PendingRemote<blink::mojom::WebPrinter> printer_remote;
    printers_.Add(this, printer_remote.InitWithNewPipeAndPassReceiver(),
                  PrinterId(printer->id));

    auto printer_info = blink::mojom::WebPrinterInfo::New();
    printer_info->printer_name = printer->name;
    printer_info->printer_remote = std::move(printer_remote);
    web_printers.push_back(std::move(printer_info));
  }
  std::move(callback).Run(std::move(web_printers));
}

}  // namespace printing
