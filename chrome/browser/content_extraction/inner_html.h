// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_EXTRACTION_INNER_HTML_H_
#define CHROME_BROWSER_CONTENT_EXTRACTION_INNER_HTML_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "third_party/blink/public/mojom/content_extraction/inner_html.mojom-forward.h"

namespace content {
class RenderFrameHost;
}

namespace content_extraction {

using InnerHtmlCallback =
    base::OnceCallback<void(const std::optional<std::string>& result)>;

// Requests the inner-html for the specified `host`.
//
// If querying inner-html fails (renderer crash, or page shutdown during
// request) then the result does not contain a value.
//
// NOTE: This function services the request as soon as called, it does not wait
// for the page to finish loading.
void GetInnerHtml(content::RenderFrameHost& host, InnerHtmlCallback callback);

}  // namespace content_extraction

#endif  // CHROME_BROWSER_CONTENT_EXTRACTION_INNER_HTML_H_
