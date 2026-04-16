// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_handler_stream_delegate.h"

#include "base/check.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/mime_handler/stream_info.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace pdf {

PdfHandlerStreamDelegate::PdfHandlerStreamDelegate() = default;
PdfHandlerStreamDelegate::~PdfHandlerStreamDelegate() = default;

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
