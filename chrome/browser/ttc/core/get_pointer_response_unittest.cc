// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/get_pointer_response.h"

#include <limits>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/pointer_context_capturer.h"
#include "chrome/browser/ttc/core/pointer_tracker.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace ttc {
namespace {

using base::test::IsJson;
using Location = PointerTracker::Location;

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

TEST(GetPointerResponseTest, ShowsAndDescribesWhereTheUserPointed) {
  const std::vector<Location> locations = {
      {.point = gfx::PointF(500, 300), .is_dwell = true}};
  const ToolResponse response = BuildGetPointerResponse(
      MakeContext(), locations, /*include_node_identity=*/false);
  ASSERT_TRUE(response.Ok());

  // The element is described without its DOM node and document ids.
  EXPECT_THAT(response.GetResult(), IsJson(R"({
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
  ASSERT_TRUE(response.visual_content());
  EXPECT_EQ(response.visual_content()->mime_type, "image/jpeg");
  const SkBitmap image =
      gfx::JPEGCodec::Decode(response.visual_content()->image_data);
  EXPECT_EQ(image.width(), 1000);
  EXPECT_EQ(image.height(), 600);
}

// The element's identity is only reported if asked for.
TEST(GetPointerResponseTest, NodeIdentity) {
  PointerContext context = MakeContext();
  context.page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main-document");
  const std::vector<Location> locations = {{.point = gfx::PointF(500, 300)}};
  const ToolResponse response = BuildGetPointerResponse(
      context, locations, /*include_node_identity=*/true);
  ASSERT_TRUE(response.Ok());
  const base::DictValue* node =
      response.GetResult().FindList("points")->front().GetDict().FindDict(
          "node");
  ASSERT_TRUE(node);
  EXPECT_EQ(node->FindInt("dom_node_id"), 57);
  EXPECT_THAT(node->FindString("document_id"),
              testing::Pointee(std::string("main-document")));
}

// Points are reported in the order the user pointed, numbered from 1. One
// outside the viewport is left out, and where there is only the page's
// background there is no node.
TEST(GetPointerResponseTest, SeveralPoints) {
  const std::vector<Location> locations = {
      {.point = gfx::PointF(700, 300)},
      {.point = gfx::PointF(-5, 10)},
      {.point = gfx::PointF(300, 300), .is_press = true, .is_dwell = true}};
  const ToolResponse response = BuildGetPointerResponse(
      MakeContext(), locations, /*include_node_identity=*/false);
  ASSERT_TRUE(response.Ok());
  EXPECT_THAT(response.GetResult(), IsJson(R"({
    "status": "ok",
    "points": [
      {"index": 1, "x": 300, "y": 300, "kind": "press"},
      {"index": 2, "x": 700, "y": 300, "kind": "moving"}
    ],
    "crop_region": {"x": 150, "y": 150, "width": 700, "height": 300},
    "viewport": {"width": 1000, "height": 600}
  })"));
  EXPECT_TRUE(response.visual_content());
}

TEST(GetPointerResponseTest, NoPointInTheViewport) {
  const std::vector<Location> outside = {
      {.point = gfx::PointF(-1, 300)},
      // The viewport's far edges are outside it.
      {.point = gfx::PointF(1000, 300)},
      {.point = gfx::PointF(500, 600)},
      {.point = gfx::PointF(std::numeric_limits<float>::quiet_NaN(), 300)},
      {.point = gfx::PointF(500, std::numeric_limits<float>::infinity())}};
  for (const std::vector<Location>& locations :
       {std::vector<Location>(), outside}) {
    const ToolResponse response = BuildGetPointerResponse(
        MakeContext(), locations, /*include_node_identity=*/false);
    ASSERT_TRUE(response.Ok());
    EXPECT_THAT(response.GetResult(),
                IsJson(R"({"status": "no_active_pointer"})"));
    EXPECT_FALSE(response.visual_content());
  }
}

class GetPointerResponseSelectionTest : public testing::Test {
 public:
  GetPointerResponseSelectionTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kTtcGetPointer, {{"use_selection", "true"}});
  }

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GetPointerResponseSelectionTest, ShowsSelectionWhenNotPointing) {
  const ToolResponse response =
      BuildGetPointerResponse(MakeContextWithSelection(), {},
                              /*include_node_identity=*/false);
  ASSERT_TRUE(response.Ok());
  EXPECT_THAT(response.GetResult(), IsJson(R"({
    "status": "ok",
    "selection": {
      "text": "quick brown",
      "bounds": {"x": 50, "y": 50, "width": 200, "height": 20}
    },
    "crop_region": {"x": 0, "y": 0, "width": 500, "height": 320},
    "viewport": {"width": 1000, "height": 600}
  })"));
  EXPECT_TRUE(response.visual_content());
}

// The selection runs from its start node to the end of its end node.
TEST_F(GetPointerResponseSelectionTest, SelectionOverSeveralNodes) {
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

  ToolResponse response =
      BuildGetPointerResponse(context, {}, /*include_node_identity=*/false);
  ASSERT_TRUE(response.Ok());
  const base::DictValue* bounds =
      response.GetResult().FindDictByDottedPath("selection.bounds");
  ASSERT_TRUE(bounds);
  EXPECT_THAT(*bounds,
              IsJson(R"({"x": 50, "y": 50, "width": 200, "height": 90})"));

  // A selection from inside an element to its end ends at the element, which
  // comes before the start node in the page content.
  selection->set_start_node_id(63);
  response =
      BuildGetPointerResponse(context, {}, /*include_node_identity=*/false);
  ASSERT_TRUE(response.Ok());
  bounds = response.GetResult().FindDictByDottedPath("selection.bounds");
  ASSERT_TRUE(bounds);
  EXPECT_THAT(*bounds,
              IsJson(R"({"x": 50, "y": 120, "width": 200, "height": 20})"));
}

TEST_F(GetPointerResponseSelectionTest, PointingWinsOverSelection) {
  const std::vector<Location> locations = {
      {.point = gfx::PointF(500, 300), .is_dwell = true}};
  const ToolResponse response =
      BuildGetPointerResponse(MakeContextWithSelection(), locations,
                              /*include_node_identity=*/false);
  ASSERT_TRUE(response.Ok());
  EXPECT_TRUE(response.GetResult().contains("points"));
  EXPECT_FALSE(response.GetResult().contains("selection"));
}

// A selection that cannot be placed in the viewport does not count, e.g. one
// inside a text field, whose text is not a node of the page content.
TEST_F(GetPointerResponseSelectionTest, SelectionThatCannotBeShown) {
  PointerContext no_text = MakeContextWithSelection();
  no_text.page_content.mutable_main_frame_data()
      ->mutable_frame_interaction_info()
      ->mutable_selection()
      ->clear_selected_text();
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
       {&no_text, &unknown_node, &no_node_ids, &end_before_start, &offscreen}) {
    const ToolResponse response =
        BuildGetPointerResponse(*context, {}, /*include_node_identity=*/false);
    ASSERT_TRUE(response.Ok());
    EXPECT_THAT(response.GetResult(),
                IsJson(R"({"status": "no_active_pointer"})"));
  }
}

// The selection is not used unless the feature param is on.
TEST(GetPointerResponseTest, SelectionIgnoredByDefault) {
  PointerContext context = MakeContext();
  auto* selection = context.page_content.mutable_main_frame_data()
                        ->mutable_frame_interaction_info()
                        ->mutable_selection();
  selection->set_start_node_id(57);
  selection->set_end_node_id(57);
  selection->set_selected_text("quick brown");
  const ToolResponse response =
      BuildGetPointerResponse(context, {}, /*include_node_identity=*/false);
  ASSERT_TRUE(response.Ok());
  EXPECT_THAT(response.GetResult(),
              IsJson(R"({"status": "no_active_pointer"})"));
}

// A screenshot that gives no viewport is an error, not an absent pointer.
TEST(GetPointerResponseTest, UnusableScreenshot) {
  const std::vector<Location> locations = {{.point = gfx::PointF(500, 300)}};
  PointerContext not_an_image = MakeContext();
  not_an_image.screenshot = {1, 2, 3};
  PointerContext no_scale_factor = MakeContext();
  no_scale_factor.scale_factor = 0;
  PointerContext infinite_scale_factor = MakeContext();
  infinite_scale_factor.scale_factor = std::numeric_limits<float>::infinity();
  for (const PointerContext* context :
       {&not_an_image, &no_scale_factor, &infinite_scale_factor}) {
    const ToolResponse response = BuildGetPointerResponse(
        *context, locations, /*include_node_identity=*/false);
    ASSERT_FALSE(response.Ok());
    EXPECT_EQ(response.error().code,
              actor::mojom::ActionResultCode::kScreenshotCaptureFailed);
  }
}

}  // namespace
}  // namespace ttc
