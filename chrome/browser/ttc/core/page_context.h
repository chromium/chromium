// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_H_
#define CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"

namespace ttc {

// Represents context extracted from a page.
struct PageContext {
  PageContext();
  ~PageContext();

  // Move-only
  PageContext(const PageContext&) = delete;
  PageContext& operator=(const PageContext&) = delete;
  PageContext(PageContext&&);
  PageContext& operator=(PageContext&&);

  base::expected<optimization_guide::AIPageContentResult,
                 page_content_annotations::FetchPageContextError>
      ai_page_content = base::unexpected(
          page_content_annotations::FetchPageContextError::kUnknown);
};

// The result of fetching a PageContext; on failure, the reason the fetch
// couldn't be performed at all.
using PageContextResult =
    base::expected<PageContext,
                   page_content_annotations::FetchPageContextError>;

using FetchCompleteCallback = base::OnceCallback<void(PageContextResult)>;

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_H_
