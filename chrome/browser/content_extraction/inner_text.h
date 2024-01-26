// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_EXTRACTION_INNER_TEXT_H_
#define CHROME_BROWSER_CONTENT_EXTRACTION_INNER_TEXT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "third_party/blink/public/mojom/content_extraction/inner_text.mojom-forward.h"

namespace content {
class RenderFrameHost;
}

namespace content_extraction {

struct InnerTextResult {
  // The combined inner-text. See comments in GetInnerText() for details.
  std::string inner_text;

  // Offset of the supplied node in `inner_text`. Only set if a node-id is
  // supplied to GetInnerText() and a matching node was found.
  std::optional<unsigned> node_offset;
};

using InnerTextCallback =
    base::OnceCallback<void(std::unique_ptr<InnerTextResult> result)>;

// Requests the inner-text for the specified `host` as well as all local
// same-origin iframes. The returned inner-text contains the combined inner-text
// of all suitable iframes. Only the inner-text of the first body or frameset
// is used. The text is combined as the iframes are encountered. For example,
// the following structure:
// <body>
//   A <iframe src="a.html></iframe>
//   B <iframe src="b.html></iframe>
// </body>
// results in the string "A <a-inner-text> B <b-inner-test>" where a-inner-text
// and b-inner-text are replaced with the inner-text of a.html and b.html.
//
// You may also supply a DomNodeId. The callback will be called with the offset
// of the start of the node in the text.
//
// If querying the inner-text fails (renderer crash, or page shutdown during
// request) then null is supplied to the callback.
//
// NOTE: This function services the request as soon as called, it does not wait
// for the page to finish loading.
void GetInnerText(content::RenderFrameHost& host,
                  std::optional<int> node_id,
                  InnerTextCallback callback);

// Exposed for testing.
namespace internal {

// Returns true if `frame` frame valid.
bool IsInnerTextFrameValid(const blink::mojom::InnerTextFramePtr& frame);

// Converts `frame` into an InnerTextResult.
std::unique_ptr<InnerTextResult> CreateInnerTextResult(
    const blink::mojom::InnerTextFrame& frame);

}  // namespace internal
}  // namespace content_extraction

#endif  // CHROME_BROWSER_CONTENT_EXTRACTION_INNER_TEXT_H_
