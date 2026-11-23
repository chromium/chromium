// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_pointer/pointer_frame_redaction.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace actor {
namespace {

using optimization_guide::proto::AnnotatedPageContent;
using optimization_guide::proto::BoundingRect;
using optimization_guide::proto::ContentNode;

constexpr char kPageOrigin[] = "https://page.test";
constexpr char kOtherOrigin[] = "https://other.test";
constexpr char kMainDocument[] = "main-document";

void SetBox(BoundingRect* proto, const gfx::Rect& box) {
  proto->set_x(box.x());
  proto->set_y(box.y());
  proto->set_width(box.width());
  proto->set_height(box.height());
}

// A page of kPageOrigin whose main document is kMainDocument.
AnnotatedPageContent MakePage() {
  AnnotatedPageContent page;
  page.mutable_main_frame_data()->mutable_security_origin()->set_value(
      kPageOrigin);
  page.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(kMainDocument);
  return page;
}

// A white screenshot of 400x300.
SkBitmap MakeScreenshot() {
  SkBitmap screenshot;
  screenshot.allocN32Pixels(400, 300);
  screenshot.eraseColor(SK_ColorWHITE);
  return screenshot;
}

// Adds to `parent` an iframe at `box`, in device pixels, whose document is of
// `origin`; if there is none, the frame's content was left out.
ContentNode* AddFrame(ContentNode* parent,
                      std::optional<std::string_view> origin,
                      const gfx::Rect& box) {
  ContentNode* frame = parent->add_children_nodes();
  auto* attributes = frame->mutable_content_attributes();
  attributes->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  SetBox(attributes->mutable_geometry()->mutable_visible_bounding_box(), box);
  if (origin) {
    attributes->mutable_iframe_data()
        ->mutable_frame_data()
        ->mutable_security_origin()
        ->set_value(std::string(*origin));
  } else {
    attributes->mutable_iframe_data()->mutable_redacted_frame_metadata();
  }
  return frame;
}

TEST(PointerFrameRedactionTest, PaintsOverFramesOfOtherOrigins) {
  AnnotatedPageContent page = MakePage();
  ContentNode* root = page.mutable_root_node();
  // A frame of the page's origin, with one of another origin inside it.
  ContentNode* same_origin =
      AddFrame(root, kPageOrigin, gfx::Rect(10, 10, 100, 100));
  AddFrame(same_origin->add_children_nodes(), kOtherOrigin,
           gfx::Rect(20, 20, 30, 30));
  // One of another origin, one that runs off the screenshot, one whose content
  // was left out, one whose origin is opaque, one that nothing is known of,
  // one whose origin was not recorded, and one that does not show.
  AddFrame(root, kOtherOrigin, gfx::Rect(200, 50, 80, 40));
  AddFrame(root, kOtherOrigin, gfx::Rect(350, 250, 100, 100));
  AddFrame(root, std::nullopt, gfx::Rect(10, 200, 50, 50));
  AddFrame(root, kPageOrigin, gfx::Rect(100, 200, 50, 50))
      ->mutable_content_attributes()
      ->mutable_iframe_data()
      ->mutable_frame_data()
      ->mutable_security_origin()
      ->set_opaque(true);
  AddFrame(root, kPageOrigin, gfx::Rect(200, 200, 50, 50))
      ->mutable_content_attributes()
      ->clear_iframe_data();
  AddFrame(root, kPageOrigin, gfx::Rect(300, 150, 40, 40))
      ->mutable_content_attributes()
      ->mutable_iframe_data()
      ->mutable_frame_data()
      ->clear_security_origin();
  AddFrame(root, kOtherOrigin, gfx::Rect(300, 20, 0, 0));

  SkBitmap screenshot = MakeScreenshot();
  PaintOverCrossOriginFrames(page, screenshot);

  for (const auto& [x, y] : {std::pair{35, 35},
                             // The corners of the 80x40 frame.
                             {200, 50},
                             {279, 50},
                             {200, 89},
                             {279, 89},
                             {399, 299},
                             {30, 220},
                             {120, 220},
                             {220, 220},
                             {320, 170}}) {
    EXPECT_EQ(screenshot.getColor(x, y), SK_ColorBLACK) << x << "," << y;
  }
  // The rest is untouched: the frame of the page's origin around the nested
  // one, the page between the frames, just outside the 80x40 frame on each
  // side, and where the empty frame is.
  for (const auto& [x, y] : {std::pair{15, 15},
                             {150, 150},
                             {199, 50},
                             {280, 50},
                             {200, 49},
                             {200, 90},
                             {280, 90},
                             {300, 20}}) {
    EXPECT_EQ(screenshot.getColor(x, y), SK_ColorWHITE) << x << "," << y;
  }
}

// PageContextFetcher records where each iframe is in the screenshot, which
// may not be where the page content has it.
TEST(PointerFrameRedactionTest, PaintsOverFramesWhereTheScreenshotHasThem) {
  AnnotatedPageContent page = MakePage();
  auto* screenshot_info =
      page.mutable_gemini_in_chrome_page_metadata()->mutable_screenshot_info();
  // A frame of another origin, one of the page's origin, and one that was
  // gone when its origin was looked up.
  auto* other = screenshot_info->add_iframe_info();
  SetBox(other->mutable_bounding_box(), gfx::Rect(10, 10, 60, 30));
  other->mutable_security_origin()->set_value(kOtherOrigin);
  auto* same = screenshot_info->add_iframe_info();
  SetBox(same->mutable_bounding_box(), gfx::Rect(100, 10, 60, 30));
  same->mutable_security_origin()->set_value(kPageOrigin);
  SetBox(screenshot_info->add_iframe_info()->mutable_bounding_box(),
         gfx::Rect(200, 10, 60, 30));

  SkBitmap screenshot = MakeScreenshot();
  PaintOverCrossOriginFrames(page, screenshot);

  for (const auto& [x, y] : {std::pair{10, 10}, {69, 39}, {220, 20}}) {
    EXPECT_EQ(screenshot.getColor(x, y), SK_ColorBLACK) << x << "," << y;
  }
  for (const auto& [x, y] : {std::pair{70, 10}, {10, 40}, {120, 20}}) {
    EXPECT_EQ(screenshot.getColor(x, y), SK_ColorWHITE) << x << "," << y;
  }
}

// A popup is painted over unless the page's main document opened it.
TEST(PointerFrameRedactionTest, PaintsOverPopupsOfFrames) {
  const struct {
    std::optional<std::string_view> opener;
    bool painted_over;
  } kCases[] = {
      {"frame-document", true}, {std::nullopt, true}, {kMainDocument, false}};
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.opener.value_or("no opener"));
    AnnotatedPageContent page = MakePage();
    auto* popup = page.mutable_popup_window();
    SetBox(popup->mutable_visible_bounding_box(), gfx::Rect(50, 60, 100, 80));
    if (test_case.opener) {
      popup->mutable_opener_document_id()->set_serialized_token(
          std::string(*test_case.opener));
    }

    SkBitmap screenshot = MakeScreenshot();
    PaintOverCrossOriginFrames(page, screenshot);

    EXPECT_EQ(screenshot.getColor(50, 60),
              test_case.painted_over ? SK_ColorBLACK : SK_ColorWHITE);
    EXPECT_EQ(screenshot.getColor(149, 139),
              test_case.painted_over ? SK_ColorBLACK : SK_ColorWHITE);
    EXPECT_EQ(screenshot.getColor(150, 140), SK_ColorWHITE);
  }
}

}  // namespace
}  // namespace actor
