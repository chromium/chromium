// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/mime_handlers/dispatch_mime_handler_event.h"

#include <utility>

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_stream_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/mime_handler/generic_mime_handler_stream_delegate.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_handler_stream_delegate.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace extensions::mime_handlers {

void SendExecuteMimeTypeHandlerEvent(
    const ExtensionId& extension_id,
    const std::string& stream_id,
    bool embedded,
    content::FrameTreeNodeId frame_tree_node_id,
    blink::mojom::TransferrableURLLoaderPtr transferrable_loader,
    const GURL& original_url,
    const std::string& internal_id,
    const std::string& mime_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents) {
    return;
  }

  // If the request was for NoStatePrefetch, abort the prefetcher and do not
  // continue. This is because plugins cancel NoStatePrefetch, see
  // http://crbug.com/40351590.
  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents);
  if (no_state_prefetch_contents) {
    no_state_prefetch_contents->Destroy(prerender::FINAL_STATUS_DOWNLOAD);
    return;
  }

  auto* browser_context = web_contents->GetBrowserContext();

  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension) {
    return;
  }

  const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
  if (!handler) {
    return;
  }
  const GURL handler_url = handler->GetHandlerUrl(mime_type);
  if (!handler_url.is_valid()) {
    return;
  }
  CHECK(handler_url.SchemeIs(kExtensionScheme));
  CHECK_EQ(handler_url.host(), extension_id);

  const bool is_generic_handler = !handler->IsPluginExtension();

  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  std::unique_ptr<StreamContainer> stream_container(
      new StreamContainer(tab_id, embedded, handler_url, extension_id,
                          std::move(transferrable_loader), original_url));

#if BUILDFLAG(ENABLE_PDF)
  if (chrome_pdf::features::IsOopifPdfEnabled() &&
      extension_id == extension_misc::kPdfExtensionId) {
    extensions::mime_handler::MimeHandlerStreamManager::Create(web_contents);
    extensions::mime_handler::MimeHandlerStreamManager::FromWebContents(
        web_contents)
        ->AddStreamContainer(frame_tree_node_id, internal_id,
                             std::move(stream_container),
                             std::make_unique<pdf::PdfHandlerStreamDelegate>());
    return;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  // Generic MIME handlers (third-party extensions) use the OOPIF path
  // with the generic delegate.
  if (is_generic_handler) {
    extensions::mime_handler::MimeHandlerStreamManager::Create(web_contents);
    extensions::mime_handler::MimeHandlerStreamManager::FromWebContents(
        web_contents)
        ->AddStreamContainer(
            frame_tree_node_id, internal_id, std::move(stream_container),
            std::make_unique<
                extensions::mime_handler::GenericMimeHandlerStreamDelegate>());
    return;
  }

  // Legacy GuestView path for allowlisted extensions.
  MimeHandlerStreamManager::Get(browser_context)
      ->AddStream(stream_id, std::move(stream_container), frame_tree_node_id);
}

}  // namespace extensions::mime_handlers
