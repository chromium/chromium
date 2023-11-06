// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_service_chromeos.h"

#include <utility>

#include "content/public/browser/render_frame_host.h"

namespace printing {

WebPrintingServiceChromeOS::WebPrintingServiceChromeOS(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebPrintingService> receiver)
    : DocumentService(*render_frame_host, std::move(receiver)) {}

WebPrintingServiceChromeOS::~WebPrintingServiceChromeOS() = default;

}  // namespace printing
