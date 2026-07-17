// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/click_tool_request.h"

#include <memory>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/scroll_to_tool_request.h"
#include "chrome/browser/actor/tools/scroll_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace actor {
namespace {

constexpr char kDocumentIdentifier[] = "doc_id";
constexpr int32_t kFakeTabId = 123;

tabs::TabHandle NonNullTabHandleForRequestPolicyTest() {
  // These tests only store the tab handle; they never use it to find a tab.
  return tabs::TabHandle(kFakeTabId);
}

PageTarget NonRootDomNodeTarget() {
  return DomNode{.node_id = 456, .document_identifier = kDocumentIdentifier};
}

PageTarget RootDomTarget() {
  return DomNode{.node_id = kRootElementDomNodeId,
                 .document_identifier = kDocumentIdentifier};
}

PageTarget CoordinateTarget() {
  return gfx::Point(10, 20);
}

TEST(ClickToolRequestTest,
     BuildToolRequest_MapsLeftOnOccludedTargetToMojomClickType) {
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::ClickAction* click =
      actions.add_actions()->mutable_click();
  click->set_tab_id(123);
  click->set_click_type(
      optimization_guide::proto::ClickAction_ClickType_LEFT_ON_OCCLUDED_TARGET);
  click->set_click_count(
      optimization_guide::proto::ClickAction_ClickCount_SINGLE);
  click->mutable_target()->set_content_node_id(456);
  click->mutable_target()->mutable_document_identifier()->set_serialized_token(
      "doc_id");

  ASSERT_OK_AND_ASSIGN(std::vector<std::unique_ptr<ToolRequest>> requests,
                       BuildToolRequest(actions));
  ASSERT_EQ(1u, requests.size());

  // This covers actor_proto_conversion.cc's explicit mapping for the new
  // click-behind type. The other proto fields above only make the request
  // valid enough to reach the click-type conversion branch.
  ClickToolRequest& request = static_cast<ClickToolRequest&>(*requests.front());
  EXPECT_EQ(mojom::ClickType::kLeftOnOccludedTarget, request.GetClickType());
}

TEST(ClickToolRequestTest, RequiresTargetInLastApc_ForOccludedClick) {
  // Direct activation always reaches APC validation. The validator rejects
  // malformed targets instead of silently bypassing the check.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicActorToctouValidation);

  ClickToolRequest occluded_dom_node(
      NonNullTabHandleForRequestPolicyTest(), NonRootDomNodeTarget(),
      mojom::ClickType::kLeftOnOccludedTarget, mojom::ClickCount::kSingle);
  EXPECT_TRUE(occluded_dom_node.RequiresTargetInLastApc());

  ClickToolRequest occluded_root(
      NonNullTabHandleForRequestPolicyTest(), RootDomTarget(),
      mojom::ClickType::kLeftOnOccludedTarget, mojom::ClickCount::kSingle);
  EXPECT_TRUE(occluded_root.RequiresTargetInLastApc());

  ClickToolRequest occluded_coordinate(
      NonNullTabHandleForRequestPolicyTest(), CoordinateTarget(),
      mojom::ClickType::kLeftOnOccludedTarget, mojom::ClickCount::kSingle);
  EXPECT_TRUE(occluded_coordinate.RequiresTargetInLastApc());

  ClickToolRequest regular_dom_node(
      NonNullTabHandleForRequestPolicyTest(), NonRootDomNodeTarget(),
      mojom::ClickType::kLeft, mojom::ClickCount::kSingle);
  EXPECT_FALSE(regular_dom_node.RequiresTargetInLastApc());
}

TEST(PageToolRequestTargetApcTest,
     PageTools_RequireTargetInLastApcForEnabledNonRootDomNodes) {
  // With TOCTOU validation enabled, page tools check DOM targets against APC.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicActorToctouValidation);

  ClickToolRequest regular_click(
      NonNullTabHandleForRequestPolicyTest(), NonRootDomNodeTarget(),
      mojom::ClickType::kLeft, mojom::ClickCount::kSingle);
  EXPECT_TRUE(regular_click.RequiresTargetInLastApc());

  MoveMouseToolRequest move_mouse(NonNullTabHandleForRequestPolicyTest(),
                                  NonRootDomNodeTarget());
  EXPECT_TRUE(move_mouse.RequiresTargetInLastApc());

  DragAndReleaseToolRequest drag(NonNullTabHandleForRequestPolicyTest(),
                                 NonRootDomNodeTarget(), CoordinateTarget());
  EXPECT_TRUE(drag.RequiresTargetInLastApc());

  TypeToolRequest type(
      NonNullTabHandleForRequestPolicyTest(), NonRootDomNodeTarget(), "hello",
      /*follow_by_enter=*/false, TypeToolRequest::Mode::kAppend);
  EXPECT_TRUE(type.RequiresTargetInLastApc());

  SelectToolRequest select(NonNullTabHandleForRequestPolicyTest(),
                           NonRootDomNodeTarget(), "beta");
  EXPECT_TRUE(select.RequiresTargetInLastApc());

  ScrollToolRequest scroll(NonNullTabHandleForRequestPolicyTest(),
                           NonRootDomNodeTarget(),
                           ScrollToolRequest::Direction::kDown,
                           /*distance=*/100);
  EXPECT_TRUE(scroll.RequiresTargetInLastApc());

  ScrollToToolRequest scroll_to(NonNullTabHandleForRequestPolicyTest(),
                                NonRootDomNodeTarget());
  EXPECT_TRUE(scroll_to.RequiresTargetInLastApc());
}

TEST(PageToolRequestTargetApcTest,
     PageTools_SkipTargetInLastApcForRootAndCoordinateTargets) {
  // Enable TOCTOU validation to exercise the APC node-id check. The check does
  // not apply to these targets:
  // - Node id 0 is a sentinel for the document viewport, not an APC node id.
  // - A coordinate identifies a point, so APC resolves it through hit testing.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicActorToctouValidation);

  TypeToolRequest type_root(
      NonNullTabHandleForRequestPolicyTest(), RootDomTarget(), "hello",
      /*follow_by_enter=*/false, TypeToolRequest::Mode::kAppend);
  EXPECT_FALSE(type_root.RequiresTargetInLastApc());

  SelectToolRequest select_root(NonNullTabHandleForRequestPolicyTest(),
                                RootDomTarget(), "beta");
  EXPECT_FALSE(select_root.RequiresTargetInLastApc());

  ScrollToolRequest scroll_root(NonNullTabHandleForRequestPolicyTest(),
                                RootDomTarget(),
                                ScrollToolRequest::Direction::kDown,
                                /*distance=*/100);
  EXPECT_FALSE(scroll_root.RequiresTargetInLastApc());

  ScrollToToolRequest scroll_to_root(NonNullTabHandleForRequestPolicyTest(),
                                     RootDomTarget());
  EXPECT_FALSE(scroll_to_root.RequiresTargetInLastApc());

  TypeToolRequest type_coordinate(
      NonNullTabHandleForRequestPolicyTest(), CoordinateTarget(), "hello",
      /*follow_by_enter=*/false, TypeToolRequest::Mode::kAppend);
  EXPECT_FALSE(type_coordinate.RequiresTargetInLastApc());

  ScrollToolRequest scroll_coordinate(NonNullTabHandleForRequestPolicyTest(),
                                      CoordinateTarget(),
                                      ScrollToolRequest::Direction::kDown,
                                      /*distance=*/100);
  EXPECT_FALSE(scroll_coordinate.RequiresTargetInLastApc());
}

TEST(PageToolRequestTargetApcTest,
     PageTools_SkipTargetInLastApcWhenFeatureDisabled) {
  // Without TOCTOU validation, ordinary page tools do not require target ids
  // from APC.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicActorToctouValidation);

  MoveMouseToolRequest move_mouse(NonNullTabHandleForRequestPolicyTest(),
                                  NonRootDomNodeTarget());
  EXPECT_FALSE(move_mouse.RequiresTargetInLastApc());

  DragAndReleaseToolRequest drag(NonNullTabHandleForRequestPolicyTest(),
                                 NonRootDomNodeTarget(), CoordinateTarget());
  EXPECT_FALSE(drag.RequiresTargetInLastApc());

  TypeToolRequest type(
      NonNullTabHandleForRequestPolicyTest(), NonRootDomNodeTarget(), "hello",
      /*follow_by_enter=*/false, TypeToolRequest::Mode::kAppend);
  EXPECT_FALSE(type.RequiresTargetInLastApc());

  SelectToolRequest select(NonNullTabHandleForRequestPolicyTest(),
                           NonRootDomNodeTarget(), "beta");
  EXPECT_FALSE(select.RequiresTargetInLastApc());

  ScrollToolRequest scroll(NonNullTabHandleForRequestPolicyTest(),
                           NonRootDomNodeTarget(),
                           ScrollToolRequest::Direction::kDown,
                           /*distance=*/100);
  EXPECT_FALSE(scroll.RequiresTargetInLastApc());

  ScrollToToolRequest scroll_to(NonNullTabHandleForRequestPolicyTest(),
                                NonRootDomNodeTarget());
  EXPECT_FALSE(scroll_to.RequiresTargetInLastApc());
}

}  // namespace
}  // namespace actor
