// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MIME_HANDLERS_DISPATCH_MIME_HANDLER_EVENT_H_
#define CHROME_BROWSER_EXTENSIONS_API_MIME_HANDLERS_DISPATCH_MIME_HANDLER_EVENT_H_

#include <memory>
#include <string>

#include "content/public/browser/frame_tree_node_id.h"
#include "extensions/common/extension_id.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"

namespace extensions::mime_handlers {

// Dispatches the MIME type handler event to `extension_id`. `mime_type` is
// used to resolve the handler URL from the extension manifest, including
// per-type URLs for generic (third-party) handlers. A non-empty `stream_id`
// identifies the created stream for legacy GuestView-based handlers.
// `embedded` indicates whether the document is embedded, and
// `frame_tree_node_id` identifies the target frame.
void SendExecuteMimeTypeHandlerEvent(
    const ExtensionId& extension_id,
    const std::string& stream_id,
    bool embedded,
    content::FrameTreeNodeId frame_tree_node_id,
    blink::mojom::TransferrableURLLoaderPtr transferrable_loader,
    const GURL& original_url,
    const std::string& internal_id,
    const std::string& mime_type);

}  // namespace extensions::mime_handlers

#endif  // CHROME_BROWSER_EXTENSIONS_API_MIME_HANDLERS_DISPATCH_MIME_HANDLER_EVENT_H_
