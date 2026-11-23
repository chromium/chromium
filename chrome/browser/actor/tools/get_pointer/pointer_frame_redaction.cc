// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_pointer/pointer_frame_redaction.h"

#include <string>
#include <vector>

#include "chrome/browser/actor/tools/get_pointer/pointer_hit_test.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"

namespace actor {

using optimization_guide::proto::AnnotatedPageContent;
using optimization_guide::proto::BoundingRect;
using optimization_guide::proto::ContentNode;

namespace {

void PaintOver(const BoundingRect& box, SkBitmap& screenshot) {
  screenshot.erase(SK_ColorBLACK, SkIRect::MakeXYWH(box.x(), box.y(),
                                                    box.width(), box.height()));
}

// Whether the popup of `page_content` was opened by the page's main document.
bool IsPopupOfMainDocument(const AnnotatedPageContent& page_content) {
  const std::string& opener =
      page_content.popup_window().opener_document_id().serialized_token();
  return !opener.empty() && opener == page_content.main_frame_data()
                                          .document_identifier()
                                          .serialized_token();
}

}  // namespace

void PaintOverCrossOriginFrames(const AnnotatedPageContent& page_content,
                                SkBitmap& screenshot) {
  // Iterative because the tree comes from a renderer and may be deep.
  std::vector<const ContentNode*> stack = {&page_content.root_node()};
  while (!stack.empty()) {
    const ContentNode* node = stack.back();
    stack.pop_back();
    if (IsCrossOriginFrame(*node, page_content)) {
      // What is in the frame is inside its box, so there is no need to look
      // further down.
      PaintOver(node->content_attributes().geometry().visible_bounding_box(),
                screenshot);
      continue;
    }
    for (const ContentNode& child : node->children_nodes()) {
      stack.push_back(&child);
    }
  }

  // The page content is extracted while the screenshot is taken, not in the
  // same instant, so a frame may be elsewhere in the screenshot than its box
  // above says, e.g. after a scroll. PageContextFetcher also records where
  // each iframe is in the very frame that the screenshot is of, with the
  // origin of its document, or without one if the frame was gone by then.
  for (const auto& iframe : page_content.gemini_in_chrome_page_metadata()
                                .screenshot_info()
                                .iframe_info()) {
    if (!IsSameSecurityOrigin(
            iframe.security_origin(),
            page_content.main_frame_data().security_origin())) {
      PaintOver(iframe.bounding_box(), screenshot);
    }
  }

  // An open popup, e.g. the list of a <select>, is drawn into the screenshot
  // on top of the page, and is not clipped to the frame that opened it. One
  // that the page's main document opened is the page's own content. One
  // opened from a frame might be of another origin: DescribeNodeAtPoint()
  // leaves it alone, and it is not for the model to see either.
  if (page_content.has_popup_window() && !IsPopupOfMainDocument(page_content)) {
    PaintOver(page_content.popup_window().visible_bounding_box(), screenshot);
  }
}

}  // namespace actor
