// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_pointer/get_pointer_result_builder.h"

#include <stdint.h>

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/actor/tool_result_data.h"
#include "chrome/browser/actor/tools/get_pointer/get_pointer_tool_result.h"
#include "chrome/browser/actor/tools/get_pointer/pointer_context.h"
#include "chrome/browser/actor/tools/tool_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace actor {
namespace {

using base::test::IsJson;
using Location = PointerLocation;

// With the selection reported when the user is not pointing.
constexpr GetPointerResultOptions kUseSelection = {.use_selection = true};

// The dict of `result`, which must not be an error.
const base::DictValue& Dict(const ToolResult& result) {
  return result.data()->As<GetPointerToolResult>()->dict;
}

// The image data of `result`, which must not be an error. Empty if it has no
// image.
const std::vector<uint8_t>& ImageData(const ToolResult& result) {
  return result.data()->As<GetPointerToolResult>()->image_data;
}

// Sets up `attributes` as hit-testable content of `type` with `bounds`, in
// device pixels. A higher `z_order` is in front.
void InitAttributes(optimization_guide::proto::ContentAttributes* attributes,
                    optimization_guide::proto::ContentAttributeType type,
                    const gfx::Rect& bounds,
                    int z_order) {
  attributes->set_attribute_type(type);
  auto* box = attributes->mutable_geometry()->mutable_visible_bounding_box();
  box->set_x(bounds.x());
  box->set_y(bounds.y());
  box->set_width(bounds.width());
  box->set_height(bounds.height());
  attributes->mutable_interaction_info()->set_document_scoped_z_order(z_order);
}

// Adds to the page of `context` a frame at `bounds`, in device pixels, in
// front of everything else, whose document has another origin than the page's.
void AddCrossOriginFrame(PointerContext& context, const gfx::Rect& bounds) {
  context.page_content.mutable_main_frame_data()
      ->mutable_security_origin()
      ->set_value("https://page.test");
  auto* frame = context.page_content.mutable_root_node()
                    ->add_children_nodes()
                    ->mutable_content_attributes();
  InitAttributes(frame, optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME,
                 bounds, /*z_order=*/10);
  frame->mutable_iframe_data()
      ->mutable_frame_data()
      ->mutable_security_origin()
      ->set_value("https://other.test");
}

// What is captured of a 1000x600 DIP viewport on a 2x display, whose page has
// one element: an image at (400,200) of 200x200 DIP.
PointerContext MakeContext() {
  PointerContext context;
  SkBitmap screenshot;
  screenshot.allocN32Pixels(2000, 1200);
  screenshot.eraseColor(SK_ColorWHITE);
  context.screenshot =
      gfx::JPEGCodec::Encode(screenshot, /*quality=*/90).value();
  context.scale_factor = 2.0f;

  auto* root = context.page_content.mutable_root_node();
  InitAttributes(root->mutable_content_attributes(),
                 optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT,
                 gfx::Rect(0, 0, 2000, 1200), 1);
  auto* image = root->add_children_nodes()->mutable_content_attributes();
  InitAttributes(image, optimization_guide::proto::CONTENT_ATTRIBUTE_IMAGE,
                 gfx::Rect(800, 400, 400, 400), 2);
  image->set_common_ancestor_dom_node_id(57);
  image->set_label("Book cover");
  return context;
}

TEST(GetPointerResultBuilderTest, ShowsAndDescribesWhereTheUserPointed) {
  const std::vector<Location> locations = {
      {.point = gfx::PointF(500, 300), .is_dwell = true}};
  const ToolResult result = BuildGetPointerResult(MakeContext(), locations,
                                                  GetPointerResultOptions());
  ASSERT_TRUE(result.Ok());

  // The element is described without its DOM node and document ids.
  EXPECT_THAT(Dict(result), IsJson(R"({
    "status": "ok",
    "points": [{
      "index": 1, "x": 500, "y": 300, "kind": "dwell",
      "node": {
        "type": "image",
        "label": "Book cover",
        "bounds": {"x": 400, "y": 200, "width": 200, "height": 200}
      }
    }],
    "crop_region": {"x": 250, "y": 150, "width": 500, "height": 300},
    "viewport": {"width": 1000, "height": 600}
  })"));

  // The image is of the crop region, in device pixels.
  const std::optional<ToolResultImage> jpeg = result.data()->GetImage();
  ASSERT_TRUE(jpeg);
  EXPECT_EQ(jpeg->mime_type, "image/jpeg");
  const SkBitmap image = gfx::JPEGCodec::Decode(jpeg->data);
  EXPECT_EQ(image.width(), 1000);
  EXPECT_EQ(image.height(), 600);
}

// The element's identity is only reported if asked for.
TEST(GetPointerResultBuilderTest, NodeIdentity) {
  PointerContext context = MakeContext();
  context.page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main-document");
  const std::vector<Location> locations = {{.point = gfx::PointF(500, 300)}};
  const ToolResult result = BuildGetPointerResult(
      context, locations, {.include_node_identity = true});
  ASSERT_TRUE(result.Ok());
  const base::DictValue* node =
      Dict(result).FindList("points")->front().GetDict().FindDict("node");
  ASSERT_TRUE(node);
  EXPECT_EQ(node->FindInt("dom_node_id"), 57);
  EXPECT_THAT(node->FindString("document_id"),
              testing::Pointee(std::string("main-document")));
}

// Points are reported in the order the user pointed, numbered from 1. One
// outside the viewport is left out, and where there is only the page's
// background there is no node.
TEST(GetPointerResultBuilderTest, SeveralPoints) {
  const std::vector<Location> locations = {
      {.point = gfx::PointF(700, 300)},
      {.point = gfx::PointF(-5, 10)},
      {.point = gfx::PointF(300, 300), .is_press = true, .is_dwell = true}};
  const ToolResult result = BuildGetPointerResult(MakeContext(), locations,
                                                  GetPointerResultOptions());
  ASSERT_TRUE(result.Ok());
  EXPECT_THAT(Dict(result), IsJson(R"({
    "status": "ok",
    "points": [
      {"index": 1, "x": 300, "y": 300, "kind": "press"},
      {"index": 2, "x": 700, "y": 300, "kind": "moving"}
    ],
    "crop_region": {"x": 150, "y": 150, "width": 700, "height": 300},
    "viewport": {"width": 1000, "height": 600}
  })"));
  EXPECT_FALSE(ImageData(result).empty());
}

// The model is not shown frames of other origins than the page's.
TEST(GetPointerResultBuilderTest, FramesOfOtherOriginsArePaintedOver) {
  PointerContext context = MakeContext();
  // At (450,250) of 50x50 DIP.
  AddCrossOriginFrame(context, gfx::Rect(900, 500, 100, 100));

  // The user points into the frame.
  const std::vector<Location> locations = {{.point = gfx::PointF(475, 275)}};
  const ToolResult result =
      BuildGetPointerResult(context, locations, GetPointerResultOptions());
  ASSERT_TRUE(result.Ok());
  // What is in the frame is not described.
  const base::ListValue* points = Dict(result).FindList("points");
  ASSERT_TRUE(points);
  ASSERT_EQ(points->size(), 1u);
  EXPECT_FALSE(points->front().GetDict().FindDict("node"));

  // The image is of the 500x300 DIP at (225,125), in device pixels: the frame
  // is at (450,250) in it, of 100x100, and the page is white beside it. The
  // crosshair, whose arms reach 48 pixels from the frame's middle, is drawn
  // after the frame is painted over.
  ASSERT_FALSE(ImageData(result).empty());
  const SkBitmap image = gfx::JPEGCodec::Decode(ImageData(result));
  ASSERT_EQ(image.width(), 1000);
  ASSERT_EQ(image.height(), 600);
  EXPECT_LT(SkColorGetG(image.getColor(460, 260)), 0x20u);
  EXPECT_GT(SkColorGetR(image.getColor(540, 300)), 0x60u);
  EXPECT_GT(SkColorGetG(image.getColor(440, 260)), 0xE0u);
}

TEST(GetPointerResultBuilderTest, NoPointInTheViewport) {
  const std::vector<Location> outside = {
      {.point = gfx::PointF(-1, 300)},
      // The viewport's far edges are outside it.
      {.point = gfx::PointF(1000, 300)},
      {.point = gfx::PointF(500, 600)},
      {.point = gfx::PointF(std::numeric_limits<float>::quiet_NaN(), 300)},
      {.point = gfx::PointF(500, std::numeric_limits<float>::infinity())}};
  for (const std::vector<Location>& locations :
       {std::vector<Location>(), outside}) {
    const ToolResult result = BuildGetPointerResult(MakeContext(), locations,
                                                    GetPointerResultOptions());
    ASSERT_TRUE(result.Ok());
    EXPECT_THAT(Dict(result), IsJson(R"({"status": "no_active_pointer"})"));
    EXPECT_TRUE(ImageData(result).empty());
  }
}

class GetPointerResultBuilderSelectionTest : public testing::Test {
 protected:
  // MakeContext() with a paragraph at (50,50) of 200x20 DIP, whose text the
  // user has selected.
  static PointerContext MakeContextWithSelection() {
    PointerContext context = MakeContext();
    auto* paragraph = context.page_content.mutable_root_node()
                          ->add_children_nodes()
                          ->mutable_content_attributes();
    InitAttributes(paragraph, optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT,
                   gfx::Rect(100, 100, 400, 40), 3);
    paragraph->set_common_ancestor_dom_node_id(60);
    auto* selection = context.page_content.mutable_main_frame_data()
                          ->mutable_frame_interaction_info()
                          ->mutable_selection();
    selection->set_start_node_id(60);
    selection->set_end_node_id(60);
    selection->set_selected_text("quick brown");
    return context;
  }
};

TEST_F(GetPointerResultBuilderSelectionTest, ShowsSelectionWhenNotPointing) {
  const ToolResult result =
      BuildGetPointerResult(MakeContextWithSelection(), {}, kUseSelection);
  ASSERT_TRUE(result.Ok());
  EXPECT_THAT(Dict(result), IsJson(R"({
    "status": "ok",
    "selection": {
      "text": "quick brown",
      "bounds": {"x": 50, "y": 50, "width": 200, "height": 20}
    },
    "crop_region": {"x": 0, "y": 0, "width": 500, "height": 320},
    "viewport": {"width": 1000, "height": 600}
  })"));
  EXPECT_FALSE(ImageData(result).empty());
}

// The image of a selection does not show frames of other origins either.
TEST_F(GetPointerResultBuilderSelectionTest,
       FramesOfOtherOriginsArePaintedOver) {
  PointerContext context = MakeContextWithSelection();
  // At (300,100) of 50x50 DIP, below the selected paragraph.
  AddCrossOriginFrame(context, gfx::Rect(600, 200, 100, 100));

  const ToolResult result = BuildGetPointerResult(context, {}, kUseSelection);
  ASSERT_TRUE(result.Ok());
  ASSERT_FALSE(ImageData(result).empty());
  // The image starts at the viewport's origin, so the frame is where it is in
  // the screenshot, and the page is white beside it.
  const SkBitmap image = gfx::JPEGCodec::Decode(ImageData(result));
  ASSERT_GT(image.width(), 650);
  ASSERT_GT(image.height(), 250);
  EXPECT_LT(SkColorGetG(image.getColor(650, 250)), 0x20u);
  EXPECT_GT(SkColorGetG(image.getColor(500, 250)), 0xE0u);
}

// The selection runs from its start node to the end of its end node.
TEST_F(GetPointerResultBuilderSelectionTest, SelectionOverSeveralNodes) {
  PointerContext context = MakeContextWithSelection();
  // A list at (50,100) with two items of 200x20 DIP, and a paragraph below.
  auto* list = context.page_content.mutable_root_node()->add_children_nodes();
  InitAttributes(list->mutable_content_attributes(),
                 optimization_guide::proto::CONTENT_ATTRIBUTE_UNORDERED_LIST,
                 gfx::Rect(100, 200, 400, 80), 4);
  list->mutable_content_attributes()->set_common_ancestor_dom_node_id(61);
  for (int i = 0; i < 2; ++i) {
    auto* item = list->add_children_nodes()->mutable_content_attributes();
    InitAttributes(item, optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT,
                   gfx::Rect(100, 200 + 40 * i, 400, 40), 5 + i);
    item->set_common_ancestor_dom_node_id(62 + i);
  }
  auto* below = context.page_content.mutable_root_node()
                    ->add_children_nodes()
                    ->mutable_content_attributes();
  InitAttributes(below, optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT,
                 gfx::Rect(100, 400, 400, 40), 7);
  below->set_common_ancestor_dom_node_id(64);
  auto* selection = context.page_content.mutable_main_frame_data()
                        ->mutable_frame_interaction_info()
                        ->mutable_selection();
  selection->set_end_node_id(61);

  ToolResult result = BuildGetPointerResult(context, {}, kUseSelection);
  ASSERT_TRUE(result.Ok());
  const base::DictValue* bounds =
      Dict(result).FindDictByDottedPath("selection.bounds");
  ASSERT_TRUE(bounds);
  EXPECT_THAT(*bounds,
              IsJson(R"({"x": 50, "y": 50, "width": 200, "height": 90})"));

  // A selection from inside an element to its end ends at the element, which
  // comes before the start node in the page content.
  selection->set_start_node_id(63);
  result = BuildGetPointerResult(context, {}, kUseSelection);
  ASSERT_TRUE(result.Ok());
  bounds = Dict(result).FindDictByDottedPath("selection.bounds");
  ASSERT_TRUE(bounds);
  EXPECT_THAT(*bounds,
              IsJson(R"({"x": 50, "y": 120, "width": 200, "height": 20})"));
}

// A selection with no text of its own, only line breaks or spaces, is of the
// media in it: here the image between two paragraphs, as when the user drags
// from the end of the first to the start of the second.
TEST_F(GetPointerResultBuilderSelectionTest, SelectionOfAnImage) {
  PointerContext context = MakeContextWithSelection();
  context.page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main-document");
  auto* root = context.page_content.mutable_root_node();
  // Put the paragraph (60) before the image (57), and another below them.
  root->mutable_children_nodes()->SwapElements(0, 1);
  root->mutable_children_nodes(1)
      ->mutable_content_attributes()
      ->mutable_image_data()
      ->set_image_caption("A red book");
  auto* below = root->add_children_nodes()->mutable_content_attributes();
  InitAttributes(below, optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT,
                 gfx::Rect(100, 900, 400, 40), 4);
  below->set_common_ancestor_dom_node_id(64);
  auto* selection = context.page_content.mutable_main_frame_data()
                        ->mutable_frame_interaction_info()
                        ->mutable_selection();
  selection->set_end_node_id(64);
  // With a no-break space, as an empty paragraph often has.
  selection->set_selected_text("\n\u00A0\n");

  const ToolResult result = BuildGetPointerResult(context, {}, kUseSelection);
  ASSERT_TRUE(result.Ok());
  const base::DictValue* description = Dict(result).FindDict("selection");
  ASSERT_TRUE(description);
  // The paragraphs at the ends of the range are not part of what is selected.
  EXPECT_THAT(*description, IsJson(R"({
    "nodes": [{
      "type": "image",
      "label": "Book cover",
      "text": "A red book",
      "bounds": {"x": 400, "y": 200, "width": 200, "height": 200}
    }],
    "bounds": {"x": 400, "y": 200, "width": 200, "height": 200}
  })"));
  EXPECT_FALSE(Dict(result).contains("points"));
  EXPECT_FALSE(ImageData(result).empty());

  // The image's identity is not reported even when identities are asked for:
  // the server only maps those of the elements pointed at for the model.
  const ToolResult with_identity = BuildGetPointerResult(
      context, {}, {.include_node_identity = true, .use_selection = true});
  ASSERT_TRUE(with_identity.Ok());
  const base::ListValue* nodes =
      Dict(with_identity).FindListByDottedPath("selection.nodes");
  ASSERT_TRUE(nodes);
  ASSERT_EQ(nodes->size(), 1u);
  const base::DictValue& node = nodes->front().GetDict();
  EXPECT_FALSE(node.contains("dom_node_id"));
  EXPECT_FALSE(node.contains("document_id"));

  // If text is selected as well, only the text is reported. The bounds are of
  // everything in the range: the two paragraphs and the image.
  selection->set_selected_text("brown\n\nfox");
  const ToolResult with_text =
      BuildGetPointerResult(context, {}, kUseSelection);
  ASSERT_TRUE(with_text.Ok());
  const base::DictValue* text_description =
      Dict(with_text).FindDict("selection");
  ASSERT_TRUE(text_description);
  EXPECT_THAT(*text_description, IsJson(R"({
    "text": "brown\n\nfox",
    "bounds": {"x": 50, "y": 50, "width": 550, "height": 420}
  })"));
}

// Only the first few of many selected images are described. The bounds are of
// them all.
TEST_F(GetPointerResultBuilderSelectionTest, SelectionOfManyImages) {
  PointerContext context = MakeContextWithSelection();
  // A row of six images of 20x20 DIP below the paragraph, in an element that
  // the selection ends in.
  auto* row = context.page_content.mutable_root_node()->add_children_nodes();
  InitAttributes(row->mutable_content_attributes(),
                 optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER,
                 gfx::Rect(100, 200, 400, 40), 4);
  row->mutable_content_attributes()->set_common_ancestor_dom_node_id(61);
  for (int i = 0; i < 6; ++i) {
    auto* image = row->add_children_nodes()->mutable_content_attributes();
    InitAttributes(image, optimization_guide::proto::CONTENT_ATTRIBUTE_IMAGE,
                   gfx::Rect(100 + 50 * i, 200, 40, 40), 5 + i);
    image->set_common_ancestor_dom_node_id(70 + i);
  }
  auto* selection = context.page_content.mutable_main_frame_data()
                        ->mutable_frame_interaction_info()
                        ->mutable_selection();
  selection->set_end_node_id(61);
  selection->set_selected_text(" ");

  const ToolResult result = BuildGetPointerResult(context, {}, kUseSelection);
  ASSERT_TRUE(result.Ok());
  const base::ListValue* nodes =
      Dict(result).FindListByDottedPath("selection.nodes");
  ASSERT_TRUE(nodes);
  EXPECT_EQ(nodes->size(), 5u);
  const base::DictValue* bounds =
      Dict(result).FindDictByDottedPath("selection.bounds");
  ASSERT_TRUE(bounds);
  EXPECT_THAT(*bounds,
              IsJson(R"({"x": 50, "y": 100, "width": 145, "height": 20})"));
}

TEST_F(GetPointerResultBuilderSelectionTest, PointingWinsOverSelection) {
  const std::vector<Location> locations = {
      {.point = gfx::PointF(500, 300), .is_dwell = true}};
  const ToolResult result = BuildGetPointerResult(MakeContextWithSelection(),
                                                  locations, kUseSelection);
  ASSERT_TRUE(result.Ok());
  EXPECT_TRUE(Dict(result).contains("points"));
  EXPECT_FALSE(Dict(result).contains("selection"));
}

// A selection that cannot be placed in the viewport does not count, e.g. one
// inside a text field, whose text is not a node of the page content. Nor does
// one of neither text nor media.
TEST_F(GetPointerResultBuilderSelectionTest, SelectionThatCannotBeShown) {
  PointerContext no_text = MakeContextWithSelection();
  no_text.page_content.mutable_main_frame_data()
      ->mutable_frame_interaction_info()
      ->mutable_selection()
      ->clear_selected_text();
  PointerContext only_whitespace = MakeContextWithSelection();
  only_whitespace.page_content.mutable_main_frame_data()
      ->mutable_frame_interaction_info()
      ->mutable_selection()
      ->set_selected_text("\n \u00A0");
  PointerContext unknown_node = MakeContextWithSelection();
  unknown_node.page_content.mutable_main_frame_data()
      ->mutable_frame_interaction_info()
      ->mutable_selection()
      ->set_end_node_id(99);
  // Nodes without an id, like the root here, must not pass for the selection's
  // when it has no node ids.
  PointerContext no_node_ids = MakeContextWithSelection();
  no_node_ids.page_content.mutable_main_frame_data()
      ->mutable_frame_interaction_info()
      ->mutable_selection()
      ->clear_start_node_id();
  no_node_ids.page_content.mutable_main_frame_data()
      ->mutable_frame_interaction_info()
      ->mutable_selection()
      ->clear_end_node_id();
  // The image (57) comes before the paragraph (60).
  PointerContext end_before_start = MakeContextWithSelection();
  end_before_start.page_content.mutable_main_frame_data()
      ->mutable_frame_interaction_info()
      ->mutable_selection()
      ->set_end_node_id(57);
  PointerContext offscreen = MakeContextWithSelection();
  offscreen.page_content.mutable_root_node()
      ->mutable_children_nodes(1)
      ->mutable_content_attributes()
      ->mutable_geometry()
      ->mutable_visible_bounding_box()
      ->set_width(0);
  for (const PointerContext* context :
       {&no_text, &only_whitespace, &unknown_node, &no_node_ids,
        &end_before_start, &offscreen}) {
    const ToolResult result =
        BuildGetPointerResult(*context, {}, kUseSelection);
    ASSERT_TRUE(result.Ok());
    EXPECT_THAT(Dict(result), IsJson(R"({"status": "no_active_pointer"})"));
  }
}

// The selection is not used unless it is asked for.
TEST(GetPointerResultBuilderTest, SelectionIgnoredByDefault) {
  PointerContext context = MakeContext();
  auto* selection = context.page_content.mutable_main_frame_data()
                        ->mutable_frame_interaction_info()
                        ->mutable_selection();
  selection->set_start_node_id(57);
  selection->set_end_node_id(57);
  selection->set_selected_text("quick brown");
  const ToolResult result =
      BuildGetPointerResult(context, {}, GetPointerResultOptions());
  ASSERT_TRUE(result.Ok());
  EXPECT_THAT(Dict(result), IsJson(R"({"status": "no_active_pointer"})"));
}

// A screenshot that gives no viewport is an error, not an absent pointer.
TEST(GetPointerResultBuilderTest, UnusableScreenshot) {
  const std::vector<Location> locations = {{.point = gfx::PointF(500, 300)}};
  PointerContext not_an_image = MakeContext();
  not_an_image.screenshot = {1, 2, 3};
  PointerContext no_scale_factor = MakeContext();
  no_scale_factor.scale_factor = 0;
  PointerContext infinite_scale_factor = MakeContext();
  infinite_scale_factor.scale_factor = std::numeric_limits<float>::infinity();
  for (const PointerContext* context :
       {&not_an_image, &no_scale_factor, &infinite_scale_factor}) {
    const ToolResult result =
        BuildGetPointerResult(*context, locations, GetPointerResultOptions());
    ASSERT_FALSE(result.Ok());
    EXPECT_EQ(result.error()->code,
              actor::mojom::ActionResultCode::kScreenshotCaptureFailed);
  }
}

}  // namespace
}  // namespace actor
