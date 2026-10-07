// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/page_context.h"

#include <utility>

#include "base/check.h"

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

}  // namespace ttc
