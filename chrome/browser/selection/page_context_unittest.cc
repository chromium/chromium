// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/page_context.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/test/protobuf_matchers.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace selection {
namespace {

using base::test::EqualsProto;

TEST(PageContextTest, CopyAndAssignCopiesAllFields) {
  PageContext original;
  original.apc.set_version(
      optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  original.apc.mutable_root_node()
      ->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(42);

  original.screenshot.allocN32Pixels(4, 3);
  original.screenshot.eraseColor(SK_ColorBLUE);

  PageContext::EncodedScreenshot encoded;
  encoded.data = std::vector<uint8_t>{0xFF, 0xD8, 0xFF};
  encoded.mime_type = "image/jpeg";
  encoded.size = gfx::Size(4, 3);
  original.encoded_screenshot = std::move(encoded);

  original.metadata = blink::mojom::PageMetadata::New();
  auto frame = blink::mojom::FrameMetadata::New();
  frame->url = GURL("https://example.com/");
  original.metadata->frame_metadata.push_back(std::move(frame));

  PageContext::PdfDocumentData pdf;
  pdf.origin = url::Origin::Create(GURL("https://example.com/"));
  pdf.bytes = std::vector<uint8_t>{1, 2, 3};
  pdf.size_limit_exceeded = false;
  original.pdf_data = std::move(pdf);

  PageContext copy(original);
  EXPECT_THAT(copy.apc, EqualsProto(original.apc));
  EXPECT_EQ(copy.screenshot.width(), 4);
  EXPECT_EQ(copy.screenshot.height(), 3);
  EXPECT_EQ(copy.screenshot.getColor(0, 0), SK_ColorBLUE);
  EXPECT_EQ(copy.encoded_screenshot, original.encoded_screenshot);
  ASSERT_TRUE(copy.metadata);
  ASSERT_EQ(copy.metadata->frame_metadata.size(), 1u);
  EXPECT_EQ(copy.metadata->frame_metadata[0]->url,
            GURL("https://example.com/"));
  EXPECT_EQ(copy.pdf_data, original.pdf_data);

  PageContext assigned;
  assigned = original;
  EXPECT_THAT(assigned.apc, EqualsProto(original.apc));
  EXPECT_EQ(assigned.screenshot.width(), 4);
  EXPECT_EQ(assigned.screenshot.height(), 3);
  EXPECT_EQ(assigned.screenshot.getColor(0, 0), SK_ColorBLUE);
  EXPECT_EQ(assigned.encoded_screenshot, original.encoded_screenshot);
  ASSERT_TRUE(assigned.metadata);
  ASSERT_EQ(assigned.metadata->frame_metadata.size(), 1u);
  EXPECT_EQ(assigned.metadata->frame_metadata[0]->url,
            GURL("https://example.com/"));
  EXPECT_EQ(assigned.pdf_data, original.pdf_data);
}

}  // namespace
}  // namespace selection
