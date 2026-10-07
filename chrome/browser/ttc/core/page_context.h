// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_H_
#define CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_H_

#include "base/types/expected.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"

namespace ttc {

// Represents context extracted from a page. Cheap to copy: the extracted
// content is shared, immutable and ref-counted.
struct PageContext {
  PageContext();
  explicit PageContext(
      page_content_annotations::RefCountedAnnotatedPageContentPtr
          annotated_page_content);
  ~PageContext();

  PageContext(const PageContext&);
  PageContext& operator=(const PageContext&);
  PageContext(PageContext&&);
  PageContext& operator=(PageContext&&);

  // The annotated page content extracted from the page. Never null.
  page_content_annotations::RefCountedAnnotatedPageContentPtr
      annotated_page_content;
};

// The result of fetching a PageContext; on failure, the reason the fetch
// couldn't be performed at all.
using PageContextResult =
    base::expected<PageContext,
                   page_content_annotations::FetchPageContextError>;

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_H_
