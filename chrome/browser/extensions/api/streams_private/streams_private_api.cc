// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"

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
#include "extensions/common/manifest_handlers/mime_types_handler.h"

namespace extensions {

void StreamsPrivateAPI::SendExecuteMimeTypeHandlerEvent(
    const std::string& extension_id,
    const std::string& stream_id,
    bool embedded,
    int frame_tree_node_id,
    blink::mojom::TransferrableURLLoaderPtr transferrable_loader,
    const GURL& original_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return;

  // If the request was for NoStatePrefetch, abort the prefetcher and do not
  // continue. This is because plugins cancel NoStatePrefetch, see
  // http://crbug.com/343590.
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
  if (!extension)
    return;

  MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  if (!handler->HasPlugin())
    return;

  // If the mime handler uses MimeHandlerViewGuest, the MimeHandlerViewGuest
  // will take ownership of the stream.
  GURL handler_url(Extension::GetBaseURLFromExtensionId(extension_id).spec() +
                   handler->handler_url());

  // If this is an inner contents, then (a) it's a guest view and doesn't have a
  // tab id anyway, or (b) it's a portal. In the portal case, providing a
  // distinct tab id breaks the pdf viewer / extension APIs. For now we just
  // indicate that a portal contents has no tab id. Unfortunately, this will
  // still be broken in subtle ways once the portal is activated (e.g. some
  // forms of zooming won't work).
  // TODO(1042323): Present a coherent representation of a tab id for portal
  // contents.
  int tab_id = web_contents->GetOuterWebContents()
                   ? SessionID::InvalidValue().id()
                   : ExtensionTabUtil::GetTabId(web_contents);

  std::unique_ptr<StreamContainer> stream_container(
      new StreamContainer(tab_id, embedded, handler_url, extension_id,
                          std::move(transferrable_loader), original_url));
  MimeHandlerStreamManager::Get(browser_context)
      ->AddStream(stream_id, std::move(stream_container), frame_tree_node_id);
}

}  // namespace extensions
