// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_utils.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "pdf/pdf_features.h"

namespace save_to_drive {

base::WeakPtr<extensions::StreamContainer> GetStreamWeakPtr(
    content::RenderFrameHost* render_frame_host) {
  if (!chrome_pdf::features::IsOopifPdfEnabled()) {
    auto* guest_view = extensions::MimeHandlerViewGuest::FromRenderFrameHost(
        render_frame_host);
    return guest_view ? guest_view->GetStreamWeakPtr() : nullptr;
  }
  if (!render_frame_host) {
    return nullptr;
  }
  content::RenderFrameHost* embedder_host = render_frame_host->GetParent();
  auto* manager =
      pdf::PdfViewerStreamManager::FromRenderFrameHost(embedder_host);
  return manager ? manager->GetStreamContainer(embedder_host) : nullptr;
}

}  // namespace save_to_drive
