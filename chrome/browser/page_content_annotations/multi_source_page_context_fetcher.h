// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MULTI_SOURCE_PAGE_CONTEXT_FETCHER_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MULTI_SOURCE_PAGE_CONTEXT_FETCHER_H_

#include "components/page_content_annotations/content/page_context_fetcher.h"

namespace content {
class WebContents;
}

namespace page_content_annotations {

void FetchPageContext(
    content::WebContents& web_contents,
    const FetchPageContextOptions& options,
    std::unique_ptr<FetchPageProgressListener> progress_listener,
    FetchPageContextResultCallback callback);

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MULTI_SOURCE_PAGE_CONTEXT_FETCHER_H_
