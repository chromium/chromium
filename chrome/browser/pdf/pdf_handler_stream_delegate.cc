// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_handler_stream_delegate.h"

#include "base/check.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "extensions/browser/mime_handler/stream_info.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace pdf {

PdfHandlerStreamDelegate::PdfHandlerStreamDelegate() = default;
PdfHandlerStreamDelegate::~PdfHandlerStreamDelegate() = default;

void PdfHandlerStreamDelegate::OnExtensionFrameFinished(
    content::NavigationHandle* navigation_handle,
    extensions::StreamInfo* stream_info) {
  CHECK(navigation_handle);
  CHECK(stream_info);

  // Setup zoom level for the PDF extension. Zoom level 0 corresponds
  // to zoom factor of 1, or 100%. This is done so the PDF viewer UI
  // does not change if the page zoom does. This is analogous to page
  // zoom not affecting the browser UI.
  const GURL pdf_extension_url = stream_info->stream()->handler_url();
  auto* render_frame_host = navigation_handle->GetRenderFrameHost();
  CHECK(render_frame_host);
  content::HostZoomMap::Get(render_frame_host->GetSiteInstance())
      ->SetZoomLevelForHostAndScheme(pdf_extension_url.GetScheme(),
                                     pdf_extension_url.GetHost(), 0);

  // Set ZoomController on the extension host.
  zoom::ZoomController::CreateForWebContentsAndRenderFrameHost(
      navigation_handle->GetWebContents(), render_frame_host->GetGlobalId());
}

void PdfHandlerStreamDelegate::OnStreamClaimed(
    content::RenderFrameHost* embedder_host,
    extensions::StreamInfo* stream_info) {
  CHECK(embedder_host);
  CHECK(stream_info);

  mojo::AssociatedRemote<extensions::mojom::MimeHandlerViewContainerManager>
      container_manager;
  embedder_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &container_manager);
  container_manager->SetInternalId(stream_info->internal_id());
}

bool PdfHandlerStreamDelegate::PluginCanSave() const {
  return plugin_can_save_;
}

void PdfHandlerStreamDelegate::SetPluginCanSave(bool plugin_can_save) {
  plugin_can_save_ = plugin_can_save;
}

}  // namespace pdf
