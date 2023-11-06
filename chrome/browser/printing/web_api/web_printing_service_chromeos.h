// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WEB_API_PRINTING_SERVICE_CHROMEOS_H_
#define CHROME_BROWSER_PRINTING_WEB_API_PRINTING_SERVICE_CHROMEOS_H_

#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace printing {

class WebPrintingServiceChromeOS
    : public content::DocumentService<blink::mojom::WebPrintingService> {
 public:
  WebPrintingServiceChromeOS(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebPrintingService> receiver);
  ~WebPrintingServiceChromeOS() override;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_WEB_API_PRINTING_SERVICE_CHROMEOS_H_
