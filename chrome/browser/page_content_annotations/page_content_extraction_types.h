// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_TYPES_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_TYPES_H_

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace page_content_annotations {

struct ExtractedPageContentResult {
  // The AnnotatedPageContent proto that represents the page content.
  optimization_guide::proto::AnnotatedPageContent page_content;

  // Whether the content is eligible for server upload.
  bool is_eligible_for_server_upload = false;
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_EXTRACTION_TYPES_H_
