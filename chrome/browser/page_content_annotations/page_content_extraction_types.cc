// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_extraction_types.h"

namespace page_content_annotations {

ExtractedPageContentResult::ExtractedPageContentResult() = default;
ExtractedPageContentResult::~ExtractedPageContentResult() = default;

ExtractedPageContentResult::ExtractedPageContentResult(
    optimization_guide::proto::AnnotatedPageContent page_content,
    base::Time extraction_timestamp,
    bool is_eligible_for_server_upload,
    std::vector<uint8_t> screenshot_data)
    : page_content(std::move(page_content)),
      extraction_timestamp(extraction_timestamp),
      is_eligible_for_server_upload(is_eligible_for_server_upload),
      screenshot_data(std::move(screenshot_data)) {}

ExtractedPageContentResult::ExtractedPageContentResult(
    const ExtractedPageContentResult& other) {
  page_content.CopyFrom(other.page_content);
  extraction_timestamp = other.extraction_timestamp;
  is_eligible_for_server_upload = other.is_eligible_for_server_upload;
  screenshot_data = other.screenshot_data;
}

ExtractedPageContentResult& ExtractedPageContentResult::operator=(
    const ExtractedPageContentResult& other) {
  page_content.CopyFrom(other.page_content);
  extraction_timestamp = other.extraction_timestamp;
  is_eligible_for_server_upload = other.is_eligible_for_server_upload;
  screenshot_data = other.screenshot_data;
  return *this;
}

ExtractedPageContentResult::ExtractedPageContentResult(
    ExtractedPageContentResult&&) = default;
ExtractedPageContentResult& ExtractedPageContentResult::operator=(
    ExtractedPageContentResult&&) = default;

}  // namespace page_content_annotations
