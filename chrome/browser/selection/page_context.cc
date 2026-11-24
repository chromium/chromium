// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/page_context.h"

#include <utility>

#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"

namespace selection {

PageContext::EncodedScreenshot::EncodedScreenshot() = default;
PageContext::EncodedScreenshot::EncodedScreenshot(const EncodedScreenshot&) =
    default;
PageContext::EncodedScreenshot& PageContext::EncodedScreenshot::operator=(
    const EncodedScreenshot&) = default;
PageContext::EncodedScreenshot::EncodedScreenshot(EncodedScreenshot&&) =
    default;
PageContext::EncodedScreenshot& PageContext::EncodedScreenshot::operator=(
    EncodedScreenshot&&) = default;
PageContext::EncodedScreenshot::~EncodedScreenshot() = default;

PageContext::PdfDocumentData::PdfDocumentData() = default;
PageContext::PdfDocumentData::PdfDocumentData(const PdfDocumentData&) = default;
PageContext::PdfDocumentData& PageContext::PdfDocumentData::operator=(
    const PdfDocumentData&) = default;
PageContext::PdfDocumentData::PdfDocumentData(PdfDocumentData&&) = default;
PageContext::PdfDocumentData& PageContext::PdfDocumentData::operator=(
    PdfDocumentData&&) = default;
PageContext::PdfDocumentData::~PdfDocumentData() = default;

PageContext::PageContext() = default;

PageContext::PageContext(const PageContext& other)
    : apc(other.apc),
      metadata(other.metadata.Clone()),
      screenshot(other.screenshot),
      encoded_screenshot(other.encoded_screenshot),
      pdf_data(other.pdf_data) {}

PageContext& PageContext::operator=(const PageContext& other) {
  if (this != &other) {
    apc = other.apc;
    metadata = other.metadata.Clone();
    screenshot = other.screenshot;
    encoded_screenshot = other.encoded_screenshot;
    pdf_data = other.pdf_data;
  }
  return *this;
}

PageContext::PageContext(PageContext&&) = default;
PageContext& PageContext::operator=(PageContext&&) = default;
PageContext::~PageContext() = default;

PageContextWithSelection::PageContextWithSelection() = default;
PageContextWithSelection::PageContextWithSelection(
    PageContext page_context,
    AreaOfInterest area_of_interest)
    : page_context(std::move(page_context)),
      area_of_interest(std::move(area_of_interest)) {}
PageContextWithSelection::PageContextWithSelection(
    const PageContextWithSelection&) = default;
PageContextWithSelection& PageContextWithSelection::operator=(
    const PageContextWithSelection&) = default;
PageContextWithSelection::PageContextWithSelection(PageContextWithSelection&&) =
    default;
PageContextWithSelection& PageContextWithSelection::operator=(
    PageContextWithSelection&&) = default;
PageContextWithSelection::~PageContextWithSelection() = default;

}  // namespace selection
