// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_SERVICE_BINDER_H_
#define CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_SERVICE_BINDER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom-forward.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace printing {

// Creates a WebPrintingService for the given frame if all necessary conditions
// are met.
void CreateWebPrintingServiceForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebPrintingService> receiver);

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_SERVICE_BINDER_H_
