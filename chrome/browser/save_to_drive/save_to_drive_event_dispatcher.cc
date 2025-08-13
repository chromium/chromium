// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_event_dispatcher.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/constants.h"
#include "pdf/pdf_features.h"

namespace save_to_drive {
namespace {

GURL GetStreamUrl(content::RenderFrameHost* render_frame_host) {
  base::WeakPtr<extensions::StreamContainer> stream;
  if (chrome_pdf::features::IsOopifPdfEnabled()) {
    content::RenderFrameHost* embedder_host = render_frame_host->GetParent();
    if (embedder_host) {
      auto* pdf_viewer_stream_manager =
          pdf::PdfViewerStreamManager::FromRenderFrameHost(embedder_host);
      if (pdf_viewer_stream_manager) {
        stream = pdf_viewer_stream_manager->GetStreamContainer(embedder_host);
      }
    }
  } else {
    auto* guest_view = extensions::MimeHandlerViewGuest::FromRenderFrameHost(
        render_frame_host);
    if (guest_view) {
      stream = guest_view->GetStreamWeakPtr();
    }
  }
  if (!stream) {
    return GURL();
  }
  return stream->stream_url();
}

}  // namespace

// static
std::unique_ptr<SaveToDriveEventDispatcher> SaveToDriveEventDispatcher::Create(
    content::RenderFrameHost* render_frame_host) {
  const GURL stream_url = GetStreamUrl(render_frame_host);
  if (stream_url.spec().empty()) {
    return nullptr;
  }
  return base::WrapUnique(
      new SaveToDriveEventDispatcher(render_frame_host, stream_url));
}

SaveToDriveEventDispatcher::~SaveToDriveEventDispatcher() = default;

void SaveToDriveEventDispatcher::Notify(
    const extensions::api::pdf_viewer_private::SaveToDriveProgress& progress) {
  base::Value::List args;
  args.Append(stream_url_.spec());
  args.Append(progress.ToValue());
  auto event = std::make_unique<extensions::Event>(
      extensions::events::PDF_VIEWER_PRIVATE_ON_SAVE_TO_DRIVE_PROGRESS,
      extensions::api::pdf_viewer_private::OnSaveToDriveProgress::kEventName,
      std::move(args), browser_context_);
  auto* event_router = extensions::EventRouter::Get(browser_context_);
  event_router->DispatchEventToExtension(extension_misc::kPdfExtensionId,
                                         std::move(event));
}

SaveToDriveEventDispatcher::SaveToDriveEventDispatcher(
    content::RenderFrameHost* render_frame_host,
    const GURL& stream_url)
    : browser_context_(render_frame_host->GetBrowserContext()),
      stream_url_(stream_url) {}

}  // namespace save_to_drive
