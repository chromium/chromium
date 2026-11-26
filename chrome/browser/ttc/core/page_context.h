// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_H_
#define CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_H_

#include <optional>

#include "base/types/expected.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/ttc/app/public/page_context_types.h"

namespace page_content_annotations {
enum class FetchPageContextError;
}

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

// Describes why fetching a `PageContext` failed.
struct PageContextError {
  static PageContextError FetchFailed();
  static PageContextError Restricted(PageRestrictionReason reason);

  // Maps this error to `FetchPageContextError` for debug tools.
  page_content_annotations::FetchPageContextError ToFetchPageContextError()
      const;

  // Set when page context is withheld due to a restriction; `std::nullopt` when
  // extraction failed transiently or produced no APC on an unrestricted page.
  std::optional<PageRestrictionReason> restriction;
};

// The result of fetching a `PageContext`; on failure, why context could not be
// provided.
using PageContextResult = base::expected<PageContext, PageContextError>;

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_PAGE_CONTEXT_H_
