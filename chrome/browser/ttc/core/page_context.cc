// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/page_context.h"

#include <utility>

#include "base/check.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"

namespace ttc {

PageContext::PageContext() = default;
PageContext::PageContext(
    page_content_annotations::RefCountedAnnotatedPageContentPtr
        annotated_page_content)
    : annotated_page_content(std::move(annotated_page_content)) {
  CHECK(this->annotated_page_content);
}
PageContext::PageContext(const PageContext&) = default;
PageContext& PageContext::operator=(const PageContext&) = default;
PageContext::PageContext(PageContext&&) = default;
PageContext& PageContext::operator=(PageContext&&) = default;
PageContext::~PageContext() = default;

// static
PageContextError PageContextError::FetchFailed() {
  return PageContextError{};
}

// static
PageContextError PageContextError::Restricted(PageRestrictionReason reason) {
  return PageContextError{.restriction = reason};
}

page_content_annotations::FetchPageContextError
PageContextError::ToFetchPageContextError() const {
  return restriction.has_value()
             ? page_content_annotations::FetchPageContextError::
                   kPageContextNotEligible
             : page_content_annotations::FetchPageContextError::kUnknown;
}

}  // namespace ttc
