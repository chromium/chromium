// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/dom_node_geometry.h"

#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace actor::ui {
namespace {
using base::test::ErrorIs;
using optimization_guide::proto::AnnotatedPageContent;
using optimization_guide::proto::ContentNode;
using optimization_guide::proto::FrameData;
using optimization_guide::proto::Geometry;
using optimization_guide::proto::IframeData;

constexpr std::string_view kDomNodeResultHistogram =
    "Actor.DomNodeGeometry.GetDomNodeResult";

constexpr char kArbiraryDocId[] = "4D7AF36711F7531ACB55F32BC7C6242E";

class ActorUiDomNodeGeometryTest : public testing::Test {
 public:
  ActorUiDomNodeGeometryTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi,
        {{features::kGlicActorUiOverlayMagicCursorName, "true"}});
  }

 protected:
  FrameData BuildMainFrameData(std::string doc_id) const {
    FrameData fd;
    *fd.mutable_document_identifier()->mutable_serialized_token() = doc_id;
    return fd;
  }

  AnnotatedPageContent BuildApcProto(std::string doc_id) const {
    AnnotatedPageContent apc;
    *apc.mutable_main_frame_data() = BuildMainFrameData(doc_id);
    return apc;
  }

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

void SetGeometry(ContentNode* node, const gfx::Rect& rect) {
  Geometry* geom = node->mutable_content_attributes()->mutable_geometry();
  geom->mutable_visible_bounding_box()->set_x(rect.x());
  geom->mutable_visible_bounding_box()->set_y(rect.y());
  geom->mutable_visible_bounding_box()->set_width(rect.width());
  geom->mutable_visible_bounding_box()->set_height(rect.height());
}

TEST_F(ActorUiDomNodeGeometryTest, NodeNotFound) {
  DomNode node{
      .node_id = 1509,
      .document_identifier = kArbiraryDocId,
  };
  auto geom =
      DomNodeGeometry::InitFromApc(BuildApcProto(node.document_identifier));
  EXPECT_THAT(geom->GetDomNode(node),
              ErrorIs(GetDomNodeResult::kNodeNotFoundInApc));
  histogram_tester_.ExpectUniqueSample(kDomNodeResultHistogram,
                                       GetDomNodeResult::kNodeNotFoundInApc, 1);
}

TEST_F(ActorUiDomNodeGeometryTest, NoMainFrameData) {
  DomNode node{
      .node_id = 1509,
      .document_identifier = kArbiraryDocId,
  };
  AnnotatedPageContent apc;
  auto geom = DomNodeGeometry::InitFromApc(apc);
  EXPECT_THAT(geom->GetDomNode(node),
              ErrorIs(GetDomNodeResult::kNoApcMainFrameData));
  histogram_tester_.ExpectUniqueSample(
      kDomNodeResultHistogram, GetDomNodeResult::kNoApcMainFrameData, 1);
}

TEST_F(ActorUiDomNodeGeometryTest, NoGeometry) {
  DomNode node{
      .node_id = 1509,
      .document_identifier = kArbiraryDocId,
  };
  AnnotatedPageContent apc = BuildApcProto(node.document_identifier);
  ContentNode* content_node = apc.mutable_root_node();
  content_node->mutable_content_attributes()->set_common_ancestor_dom_node_id(
      node.node_id);
  auto geom = DomNodeGeometry::InitFromApc(apc);
  EXPECT_THAT(geom->GetDomNode(node), ErrorIs(GetDomNodeResult::kNoGeometry));
  histogram_tester_.ExpectUniqueSample(kDomNodeResultHistogram,
                                       GetDomNodeResult::kNoGeometry, 1);
}

TEST_F(ActorUiDomNodeGeometryTest, OffScreen) {
  DomNode node{
      .node_id = 1509,
      .document_identifier = kArbiraryDocId,
  };
  AnnotatedPageContent apc = BuildApcProto(node.document_identifier);
  ContentNode* content_node = apc.mutable_root_node();
  content_node->mutable_content_attributes()->set_common_ancestor_dom_node_id(
      node.node_id);
  // Set the geometry, but not the visible bounding box.
  content_node->mutable_content_attributes()->mutable_geometry();
  auto geom = DomNodeGeometry::InitFromApc(apc);
  EXPECT_THAT(geom->GetDomNode(node), ErrorIs(GetDomNodeResult::kOffScreen));
  histogram_tester_.ExpectUniqueSample(kDomNodeResultHistogram,
                                       GetDomNodeResult::kOffScreen, 1);
}

TEST_F(ActorUiDomNodeGeometryTest, Success) {
  DomNode node{
      .node_id = 1509,
      .document_identifier = kArbiraryDocId,
  };
  AnnotatedPageContent apc = BuildApcProto(node.document_identifier);
  ContentNode* content_node = apc.mutable_root_node();
  content_node->mutable_content_attributes()->set_common_ancestor_dom_node_id(
      node.node_id);
  SetGeometry(content_node, gfx::Rect(10, 20, 30, 40));
  auto geom = DomNodeGeometry::InitFromApc(apc);
  auto p = geom->GetDomNode(node);
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(25, p->x());
  EXPECT_EQ(40, p->y());
  histogram_tester_.ExpectUniqueSample(kDomNodeResultHistogram,
                                       GetDomNodeResult::kSuccess, 1);
}

TEST_F(ActorUiDomNodeGeometryTest, NestedNodesAndIframes) {
  DomNode node{
      .node_id = 1510,
      .document_identifier = "IFRAME_DOC_ID",
  };
  AnnotatedPageContent apc = BuildApcProto("MAIN_DOC_ID");
  ContentNode* root_node = apc.mutable_root_node();
  ContentNode* child_node = root_node->add_children_nodes();
  IframeData* iframe_data =
      child_node->mutable_content_attributes()->mutable_iframe_data();
  *iframe_data->mutable_frame_data() = BuildMainFrameData("IFRAME_DOC_ID");
  ContentNode* grandchild_node = child_node->add_children_nodes();
  grandchild_node->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(node.node_id);
  SetGeometry(grandchild_node, gfx::Rect(100, 200, 50, 60));

  auto geom = DomNodeGeometry::InitFromApc(apc);
  auto p = geom->GetDomNode(node);
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(125, p->x());
  EXPECT_EQ(230, p->y());
  histogram_tester_.ExpectUniqueSample(kDomNodeResultHistogram,
                                       GetDomNodeResult::kSuccess, 1);
}

TEST_F(ActorUiDomNodeGeometryTest, MismatchedIframe) {
  DomNode node{
      .node_id = 1510,
      .document_identifier = "IFRAME_DOC_ID_2",
  };
  AnnotatedPageContent apc = BuildApcProto("MAIN_DOC_ID");
  ContentNode* root_node = apc.mutable_root_node();
  ContentNode* child_node = root_node->add_children_nodes();
  IframeData* iframe_data =
      child_node->mutable_content_attributes()->mutable_iframe_data();
  *iframe_data->mutable_frame_data() = BuildMainFrameData("IFRAME_DOC_ID");
  ContentNode* grandchild_node = child_node->add_children_nodes();
  grandchild_node->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(node.node_id);
  SetGeometry(grandchild_node, gfx::Rect(100, 200, 50, 60));

  auto geom = DomNodeGeometry::InitFromApc(apc);
  EXPECT_THAT(geom->GetDomNode(node),
              ErrorIs(GetDomNodeResult::kNodeNotFoundInApc));
  histogram_tester_.ExpectUniqueSample(kDomNodeResultHistogram,
                                       GetDomNodeResult::kNodeNotFoundInApc, 1);
}

}  // namespace
}  // namespace actor::ui
