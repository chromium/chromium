// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_extraction/inner_text.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/inner_text.mojom.h"

namespace content_extraction {

using internal::CreateInnerTextResult;
using internal::IsInnerTextFrameValid;

TEST(InnerTextTest, IsInnerTextFrameValid) {
  // Null is not valid.
  EXPECT_FALSE(IsInnerTextFrameValid(nullptr));

  // All segments must be non-null.
  auto frame = blink::mojom::InnerTextFrame::New();
  frame->segments.push_back(nullptr);
  EXPECT_FALSE(IsInnerTextFrameValid(frame));

  // Empty segment is valid.
  frame->segments.clear();
  EXPECT_TRUE(IsInnerTextFrameValid(frame));

  // Single text segment is valid.
  frame->segments.push_back(blink::mojom::InnerTextSegment::NewText("a"));
  EXPECT_TRUE(IsInnerTextFrameValid(frame));

  // Single frame is valid.
  frame->segments.clear();
  frame->segments.push_back(blink::mojom::InnerTextSegment::NewFrame(
      blink::mojom::InnerTextFrame::New()));
  EXPECT_TRUE(IsInnerTextFrameValid(frame));
}

TEST(InnerTextTest, CreateInnerTextResult) {
  auto frame = blink::mojom::InnerTextFrame::New();
  frame->segments.push_back(blink::mojom::InnerTextSegment::NewText("abc"));

  auto child_frame = blink::mojom::InnerTextFrame::New();
  child_frame->segments.push_back(
      blink::mojom::InnerTextSegment::NewText("def"));
  child_frame->segments.push_back(
      blink::mojom::InnerTextSegment::NewNodeLocation(
          blink::mojom::NodeLocationType::kStart));
  child_frame->segments.push_back(
      blink::mojom::InnerTextSegment::NewText("ghi"));
  child_frame->segments.push_back(
      blink::mojom::InnerTextSegment::NewText("jkl"));
  frame->segments.push_back(
      blink::mojom::InnerTextSegment::NewFrame(std::move(child_frame)));

  auto result = CreateInnerTextResult(*frame);
  EXPECT_EQ("abcdefghijkl", result->inner_text);
  ASSERT_TRUE(result->node_offset.has_value());
  EXPECT_EQ(6, result->node_offset);
}

}  // namespace content_extraction
