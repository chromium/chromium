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

// Send the onExecuteMimeTypeHandler event to `extension_id`. A non-empty
// `stream_id` will be used to identify the created stream during
// MimeHandlerViewGuest creation. `embedded` should be set to whether the
// document is embedded within another document. The `frame_tree_node_id`
// parameter is used for the top level plugins case. (PDF, etc).
void SendExecuteMimeTypeHandlerEvent(
    const ExtensionId& extension_id,
    const std::string& stream_id,
    bool embedded,
    content::FrameTreeNodeId frame_tree_node_id,
    blink::mojom::TransferrableURLLoaderPtr transferrable_loader,
    const GURL& original_url,
    const std::string& internal_id);

}  // namespace extensions::mime_handlers

#endif  // CHROME_BROWSER_EXTENSIONS_API_MIME_HANDLERS_DISPATCH_MIME_HANDLER_EVENT_H_
