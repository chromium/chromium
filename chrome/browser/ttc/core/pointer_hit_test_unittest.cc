// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_hit_test.h"

#include <optional>
#include <string>

#include "base/values.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace ttc {
namespace {

using optimization_guide::proto::AnnotatedPageContent;
using optimization_guide::proto::ContentAttributeType;
using optimization_guide::proto::ContentNode;

// The page content's geometry is in device pixels; the tests use a 2x display.
constexpr float kScaleFactor = 2.0f;

// Sets up `node` as hit-testable content of `type` with `bounds`, in device
// pixels. A higher `z_order` is in front.
void InitNode(ContentNode* node,
              ContentAttributeType type,
              int dom_node_id,
              const gfx::Rect& bounds,
              int z_order) {
  auto* attributes = node->mutable_content_attributes();
  attributes->set_attribute_type(type);
  attributes->set_common_ancestor_dom_node_id(dom_node_id);
  auto* box = attributes->mutable_geometry()->mutable_visible_bounding_box();
  box->set_x(bounds.x());
  box->set_y(bounds.y());
  box->set_width(bounds.width());
  box->set_height(bounds.height());
  attributes->mutable_interaction_info()->set_document_scoped_z_order(z_order);
}

// A page of 800x600 device pixels with a root node to add content to.
AnnotatedPageContent MakePage() {
  AnnotatedPageContent page;
  page.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main-document");
  InitNode(page.mutable_root_node(),
           optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT, 1,
           gfx::Rect(0, 0, 800, 600), 1);
  return page;
}

TEST(PointerHitTestTest, DescribesTheElementAtThePoint) {
  AnnotatedPageContent page = MakePage();
  ContentNode* image = page.mutable_root_node()->add_children_nodes();
  InitNode(image, optimization_guide::proto::CONTENT_ATTRIBUTE_IMAGE, 57,
           gfx::Rect(200, 100, 240, 360), 2);
  image->mutable_content_attributes()->set_label("Book cover");
  image->mutable_content_attributes()->mutable_image_data()->set_image_caption(
      "The Way of Kings");

  // (150, 100) DIP is (300, 200) in device pixels, inside the image. Its
  // bounds come back in DIPs.
  std::optional<base::DictValue> description =
      DescribeNodeAtPoint(page, gfx::PointF(150, 100), kScaleFactor);
  ASSERT_TRUE(description);
  EXPECT_EQ(*description, base::DictValue()
                              .Set("dom_node_id", 57)
                              .Set("document_id", "main-document")
                              .Set("type", "image")
                              .Set("label", "Book cover")
                              .Set("text", "The Way of Kings")
                              .Set("bounds", base::DictValue()
                                                 .Set("x", 100)
                                                 .Set("y", 50)
                                                 .Set("width", 120)
                                                 .Set("height", 180)));
}

// Pointing at the page's background (the document, or a container spanning
// most of the viewport) or outside the page is pointing at no element.
TEST(PointerHitTestTest, NothingAtThePoint) {
  AnnotatedPageContent page = MakePage();
  // Like the body: a plain container over nearly the whole viewport.
  InitNode(page.mutable_root_node()->add_children_nodes(),
           optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER, 2,
           gfx::Rect(16, 16, 768, 568), 2);
  EXPECT_FALSE(DescribeNodeAtPoint(page, gfx::PointF(4, 4), kScaleFactor));
  EXPECT_FALSE(DescribeNodeAtPoint(page, gfx::PointF(10, 10), kScaleFactor));
  EXPECT_FALSE(
      DescribeNodeAtPoint(page, gfx::PointF(1000, 1000), kScaleFactor));
  EXPECT_FALSE(DescribeNodeAtPoint(page, gfx::PointF(10, 10), 0));
}

// An element's text is that of its descendants, in order, without the
// whitespace around it, cut to a length that is enough to recognize it by.
// Optional fields it lacks are left out.
TEST(PointerHitTestTest, TextComesFromDescendantsAndIsTruncated) {
  AnnotatedPageContent page = MakePage();
  ContentNode* paragraph = page.mutable_root_node()->add_children_nodes();
  InitNode(paragraph, optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH,
           10, gfx::Rect(0, 0, 400, 100), 2);
  // The text nodes are not hit-testable themselves here, so the paragraph is.
  // Indentation from the page's source does not use up the length.
  ContentNode* indentation = paragraph->add_children_nodes();
  indentation->mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content(std::string(500, ' '));
  ContentNode* first = paragraph->add_children_nodes();
  first->mutable_content_attributes()->mutable_text_data()->set_text_content(
      "\n    Hello  ");
  ContentNode* second = paragraph->add_children_nodes();
  second->mutable_content_attributes()->mutable_text_data()->set_text_content(
      std::string(500, 'x'));

  std::optional<base::DictValue> description =
      DescribeNodeAtPoint(page, gfx::PointF(20, 20), kScaleFactor);
  ASSERT_TRUE(description);
  EXPECT_EQ(*description->FindString("type"), "paragraph");
  EXPECT_EQ(*description->FindString("text"), "Hello " + std::string(122, 'x'));
  EXPECT_FALSE(description->Find("label"));
}

// The hit test finds the innermost node, e.g. the text in a button; what is
// described is the link or form control it is part of.
TEST(PointerHitTestTest, PartOfAControlIsTheControl) {
  AnnotatedPageContent page = MakePage();
  ContentNode* button = page.mutable_root_node()->add_children_nodes();
  InitNode(button, optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL,
           30, gfx::Rect(100, 100, 200, 80), 2);
  button->mutable_content_attributes()->set_label("Buy now");
  ContentNode* text = button->add_children_nodes();
  InitNode(text, optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT, 31,
           gfx::Rect(140, 120, 60, 40), 3);
  text->mutable_content_attributes()->mutable_text_data()->set_text_content(
      "Buy");

  std::optional<base::DictValue> description =
      DescribeNodeAtPoint(page, gfx::PointF(80, 70), kScaleFactor);
  ASSERT_TRUE(description);
  EXPECT_EQ(description->FindInt("dom_node_id"), 30);
  EXPECT_EQ(*description->FindString("type"), "form_control");
  EXPECT_EQ(*description->FindString("label"), "Buy now");
  EXPECT_EQ(*description->FindString("text"), "Buy");
}

// An element in an iframe is identified by the iframe's document, and the
// frontmost element wins.
TEST(PointerHitTestTest, ElementInIframeHasItsDocumentsId) {
  AnnotatedPageContent page = MakePage();
  ContentNode* iframe = page.mutable_root_node()->add_children_nodes();
  InitNode(iframe, optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME, 20,
           gfx::Rect(100, 100, 400, 400), 2);
  iframe->mutable_content_attributes()
      ->mutable_iframe_data()
      ->mutable_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("iframe-document");
  // Z-orders are per document.
  ContentNode* iframe_root = iframe->add_children_nodes();
  InitNode(iframe_root, optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT, 1,
           gfx::Rect(100, 100, 400, 400), 1);
  ContentNode* link = iframe_root->add_children_nodes();
  InitNode(link, optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR, 5,
           gfx::Rect(120, 120, 100, 40), 2);

  std::optional<base::DictValue> description =
      DescribeNodeAtPoint(page, gfx::PointF(70, 70), kScaleFactor);
  ASSERT_TRUE(description);
  EXPECT_EQ(description->FindInt("dom_node_id"), 5);
  EXPECT_EQ(*description->FindString("document_id"), "iframe-document");
  EXPECT_EQ(*description->FindString("type"), "anchor");

  // Elsewhere in the iframe there is only its document.
  EXPECT_FALSE(DescribeNodeAtPoint(page, gfx::PointF(200, 200), kScaleFactor));
}

}  // namespace
}  // namespace ttc
