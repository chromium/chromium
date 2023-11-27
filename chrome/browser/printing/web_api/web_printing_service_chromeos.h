// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_SERVICE_CHROMEOS_H_
#define CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_SERVICE_CHROMEOS_H_

#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace printing {

class WebPrintingServiceChromeOS
    : public content::DocumentService<blink::mojom::WebPrintingService>,
      public blink::mojom::WebPrinter {
 public:
  WebPrintingServiceChromeOS(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebPrintingService> receiver);
  ~WebPrintingServiceChromeOS() override;

  // blink::mojom::WebPrintingService:
  void GetPrinters(GetPrintersCallback callback) override;

  // blink::mojom::WebPrinter:
  void FetchAttributes(FetchAttributesCallback callback) override;

 private:
  using PrinterId = base::StrongAlias<class PrinterId, std::string>;

  void OnPrintersRetrieved(
      GetPrintersCallback callback,
      std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers);

  // Stores browser-side endpoints for blink-side Printer objects.
  mojo::ReceiverSet<blink::mojom::WebPrinter, PrinterId> printers_;

  base::WeakPtrFactory<WebPrintingServiceChromeOS> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_SERVICE_CHROMEOS_H_
